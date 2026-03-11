#include "core/memory.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace life_orchestrator::core {
namespace {

std::string escape(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\' || ch == ';' || ch == '|' || ch == '=') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    return result;
}

std::string unescape(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    bool escaped = false;
    for (char ch : value) {
        if (escaped) {
            result.push_back(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else {
            result.push_back(ch);
        }
    }
    if (escaped) {
        throw std::runtime_error("dangling escape");
    }
    return result;
}

std::vector<std::string> split_escaped(const std::string& input, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    bool escaped = false;
    for (char ch : input) {
        if (escaped) {
            current.push_back(ch);
            escaped = false;
        } else if (ch == '\\') {
            escaped = true;
        } else if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (escaped) {
        throw std::runtime_error("dangling escape");
    }
    parts.push_back(current);
    return parts;
}

std::string serialize_map(const StringMap& map) {
    std::vector<std::pair<std::string, std::string>> ordered(map.begin(), map.end());
    std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    std::ostringstream out;
    bool first = true;
    for (const auto& [key, value] : ordered) {
        if (!first) {
            out << '|';
        }
        first = false;
        out << escape(key) << '=' << escape(value);
    }
    return out.str();
}

StringMap parse_map(const std::string& raw) {
    StringMap result;
    if (raw.empty()) {
        return result;
    }
    for (const auto& pair : split_escaped(raw, '|')) {
        const auto pos = pair.find('=');
        if (pos == std::string::npos) {
            throw std::runtime_error("map pair missing '='");
        }
        const auto key = unescape(pair.substr(0, pos));
        const auto value = unescape(pair.substr(pos + 1));
        result[key] = value;
    }
    return result;
}

std::string serialize_list(const std::vector<std::string>& values) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << '|';
        }
        out << escape(values[i]);
    }
    return out.str();
}

std::vector<std::string> parse_list(const std::string& raw) {
    if (raw.empty()) {
        return {};
    }
    auto parts = split_escaped(raw, '|');
    for (auto& part : parts) {
        part = unescape(part);
    }
    return parts;
}

template <typename T>
MemoryResultWith<T> success(T value) {
    return {true, "ok", std::move(value)};
}

MemoryResult ok_result() { return {true, "ok"}; }

MemoryResult error_result(std::string message) { return {false, std::move(message)}; }

template <typename T>
MemoryResultWith<T> error_with(std::string message) {
    return {false, std::move(message), std::nullopt};
}

std::filesystem::path layer_file(const std::filesystem::path& root, MemoryLayer layer) {
    switch (layer) {
        case MemoryLayer::LifeModelGraph: return root / "memory/life_graph/entities.ndjson";
        case MemoryLayer::EpisodicMemory: return root / "memory/episodic/records.ndjson";
        case MemoryLayer::PreferenceMemory: return root / "memory/preferences/records.ndjson";
        case MemoryLayer::RelationshipMemory: return root / "memory/relationship_memory/records.ndjson";
        case MemoryLayer::ProjectMemory: return root / "memory/project_memory/records.ndjson";
        case MemoryLayer::BehavioralHistory: return root / "memory/behavioral_history/records.ndjson";
        case MemoryLayer::KnowledgeRetrievalIndex: return root / "memory/retrieval_index/records.ndjson";
        case MemoryLayer::IntegrationConfiguration:
            return root / "memory/integration_configuration/records.ndjson";
    }
    return root / "memory/unknown.ndjson";
}

std::filesystem::path relationship_file(const std::filesystem::path& root) {
    return root / "memory/life_graph/relationships.ndjson";
}

std::filesystem::path manifest_file(const std::filesystem::path& root) {
    return root / "memory/metadata/store_manifest.json";
}

void ensure_parent(const std::filesystem::path& p) {
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path());
    }
}

void append_line(const std::filesystem::path& path, const std::string& line) {
    ensure_parent(path);
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) {
        throw std::runtime_error("unable to open file for append: " + path.string());
    }
    out << line << '\n';
    if (!out.good()) {
        throw std::runtime_error("failed writing file: " + path.string());
    }
}

std::vector<std::string> read_lines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    if (!in.is_open()) {
        return lines;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::string serialize_fields(const std::vector<std::pair<std::string, std::string>>& fields) {
    std::ostringstream out;
    bool first = true;
    for (const auto& [key, value] : fields) {
        if (!first) {
            out << ';';
        }
        first = false;
        out << key << '=' << escape(value);
    }
    return out.str();
}

StringMap parse_fields(const std::string& line) {
    StringMap fields;
    for (const auto& token : split_escaped(line, ';')) {
        const auto pos = token.find('=');
        if (pos == std::string::npos) {
            throw std::runtime_error("field missing '='");
        }
        fields[token.substr(0, pos)] = unescape(token.substr(pos + 1));
    }
    return fields;
}

EntityType parse_entity_type(const std::string& value) {
    if (value == "Goal") return EntityType::Goal;
    if (value == "Commitment") return EntityType::Commitment;
    if (value == "Relationship") return EntityType::Relationship;
    if (value == "Project") return EntityType::Project;
    if (value == "Environment") return EntityType::Environment;
    if (value == "Location") return EntityType::Location;
    if (value == "Domain") return EntityType::Domain;
    if (value == "Task") return EntityType::Task;
    if (value == "Habit") return EntityType::Habit;
    if (value == "Person") return EntityType::Person;
    if (value == "Preference") return EntityType::Preference;
    return EntityType::Integration;
}

RelationshipType parse_relationship_type(const std::string& value) {
    if (value == "Supports") return RelationshipType::Supports;
    if (value == "Contains") return RelationshipType::Contains;
    if (value == "AssociatedWith") return RelationshipType::AssociatedWith;
    if (value == "ScheduledIn") return RelationshipType::ScheduledIn;
    if (value == "DependsOn") return RelationshipType::DependsOn;
    if (value == "RelatedTo") return RelationshipType::RelatedTo;
    if (value == "OwnedBy") return RelationshipType::OwnedBy;
    return RelationshipType::ConfiguredBy;
}

IntegrationStatus parse_integration_status(const std::string& value) {
    if (value == "Disabled") return IntegrationStatus::Disabled;
    if (value == "Enabled") return IntegrationStatus::Enabled;
    if (value == "Error") return IntegrationStatus::Error;
    return IntegrationStatus::Unknown;
}

CredentialStorageMode parse_storage_mode(const std::string& value) {
    if (value == "InlinePlaceholderOnly") return CredentialStorageMode::InlinePlaceholderOnly;
    if (value == "ExternalSecretReference") return CredentialStorageMode::ExternalSecretReference;
    return CredentialStorageMode::Unset;
}

}  // namespace

FileMemoryStore::FileMemoryStore(std::filesystem::path data_root, control_plane::EventLogger* event_logger)
    : data_root_(std::move(data_root)), event_logger_(event_logger), snapshot_version_(0) {}

void FileMemoryStore::emit_memory_event(EventCategory category,
                                        MemoryLayer layer,
                                        const std::string& record_id,
                                        MemoryOperationType operation,
                                        const SourceModuleId& source_module_id,
                                        const std::string& message) const {
    if (event_logger_ == nullptr) {
        return;
    }

    event_logger_->append(StructuredEvent{.category = category,
                                          .occurred_at = current_timestamp_utc(),
                                          .request_id = "memory",
                                          .module_id = source_module_id,
                                          .capability_id = "memory.store",
                                          .message = message,
                                          .fields = {{"layer", to_string(layer)},
                                                     {"record_id", record_id},
                                                     {"operation", to_string(operation)}}});
}

MemoryResult FileMemoryStore::upsert_life_entity(const LifeEntity& entity) {
    emit_memory_event(EventCategory::MemoryWriteStarted,
                      MemoryLayer::LifeModelGraph,
                      entity.entity_id,
                      MemoryOperationType::Upsert,
                      entity.source_module_id,
                      "Life entity write started.");
    try {
        entities_by_id_[entity.entity_id] = entity;
        append_line(layer_file(data_root_, MemoryLayer::LifeModelGraph),
                    serialize_fields({{"record_kind", "life_entity"},
                                      {"entity_id", entity.entity_id},
                                      {"entity_type", to_string(entity.entity_type)},
                                      {"display_name", entity.display_name},
                                      {"canonical_name", entity.canonical_name},
                                      {"description", entity.description},
                                      {"created_at", entity.created_at},
                                      {"updated_at", entity.updated_at},
                                      {"source_module_id", entity.source_module_id},
                                      {"version", std::to_string(entity.version)},
                                      {"archived", entity.archived ? "1" : "0"},
                                      {"attributes", serialize_map(entity.attributes)}}));
        ++snapshot_version_;
        emit_memory_event(EventCategory::MemoryWriteCompleted,
                          MemoryLayer::LifeModelGraph,
                          entity.entity_id,
                          MemoryOperationType::Upsert,
                          entity.source_module_id,
                          "Life entity write completed.");
        return ok_result();
    } catch (const std::exception& e) {
        emit_memory_event(EventCategory::MemoryWriteFailed,
                          MemoryLayer::LifeModelGraph,
                          entity.entity_id,
                          MemoryOperationType::Upsert,
                          entity.source_module_id,
                          e.what());
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::upsert_life_relationship(const LifeRelationship& relationship) {
    emit_memory_event(EventCategory::MemoryWriteStarted,
                      MemoryLayer::LifeModelGraph,
                      relationship.relationship_id,
                      MemoryOperationType::Upsert,
                      relationship.source_module_id,
                      "Life relationship write started.");
    try {
        relationships_by_id_[relationship.relationship_id] = relationship;
        append_line(relationship_file(data_root_),
                    serialize_fields({{"record_kind", "life_relationship"},
                                      {"relationship_id", relationship.relationship_id},
                                      {"from_entity_id", relationship.from_entity_id},
                                      {"to_entity_id", relationship.to_entity_id},
                                      {"relationship_type", to_string(relationship.relationship_type)},
                                      {"created_at", relationship.created_at},
                                      {"updated_at", relationship.updated_at},
                                      {"source_module_id", relationship.source_module_id},
                                      {"version", std::to_string(relationship.version)},
                                      {"attributes", serialize_map(relationship.attributes)}}));
        ++snapshot_version_;
        emit_memory_event(EventCategory::MemoryWriteCompleted,
                          MemoryLayer::LifeModelGraph,
                          relationship.relationship_id,
                          MemoryOperationType::Upsert,
                          relationship.source_module_id,
                          "Life relationship write completed.");
        return ok_result();
    } catch (const std::exception& e) {
        emit_memory_event(EventCategory::MemoryWriteFailed,
                          MemoryLayer::LifeModelGraph,
                          relationship.relationship_id,
                          MemoryOperationType::Upsert,
                          relationship.source_module_id,
                          e.what());
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::append_episodic_record(const EpisodicMemoryRecord& record) {
    emit_memory_event(EventCategory::MemoryWriteStarted,
                      MemoryLayer::EpisodicMemory,
                      record.record_id,
                      MemoryOperationType::Insert,
                      record.source_module_id,
                      "Episodic write started.");
    try {
        episodic_records_.push_back(record);
        append_line(layer_file(data_root_, MemoryLayer::EpisodicMemory),
                    serialize_fields({{"record_id", record.record_id},
                                      {"timestamp", record.timestamp},
                                      {"event_type", record.event_type},
                                      {"source_module_id", record.source_module_id},
                                      {"associated_entity_ids", serialize_list(record.associated_entity_ids)},
                                      {"summary", record.summary},
                                      {"details", serialize_map(record.details)},
                                      {"version", std::to_string(record.version)}}));
        emit_memory_event(EventCategory::MemoryWriteCompleted,
                          MemoryLayer::EpisodicMemory,
                          record.record_id,
                          MemoryOperationType::Insert,
                          record.source_module_id,
                          "Episodic write completed.");
        return ok_result();
    } catch (const std::exception& e) {
        emit_memory_event(EventCategory::MemoryWriteFailed,
                          MemoryLayer::EpisodicMemory,
                          record.record_id,
                          MemoryOperationType::Insert,
                          record.source_module_id,
                          e.what());
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::upsert_preference_record(const PreferenceRecord& record) {
    try {
        preferences_by_id_[record.record_id] = record;
        append_line(layer_file(data_root_, MemoryLayer::PreferenceMemory),
                    serialize_fields({{"record_id", record.record_id},
                                      {"preference_key", record.preference_key},
                                      {"value", record.value},
                                      {"confidence", std::to_string(record.confidence)},
                                      {"source_module_id", record.source_module_id},
                                      {"created_at", record.created_at},
                                      {"updated_at", record.updated_at},
                                      {"version", std::to_string(record.version)}}));
        emit_memory_event(EventCategory::MemoryWriteCompleted,
                          MemoryLayer::PreferenceMemory,
                          record.record_id,
                          MemoryOperationType::Upsert,
                          record.source_module_id,
                          "Preference write completed.");
        return ok_result();
    } catch (const std::exception& e) {
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::upsert_relationship_memory_record(const RelationshipMemoryRecord& record) {
    try {
        relationship_memory_by_id_[record.record_id] = record;
        append_line(layer_file(data_root_, MemoryLayer::RelationshipMemory),
                    serialize_fields({{"record_id", record.record_id},
                                      {"related_person_entity_id", record.related_person_entity_id},
                                      {"communication_cadence", record.communication_cadence},
                                      {"important_dates", serialize_list(record.important_dates)},
                                      {"shared_interests", serialize_list(record.shared_interests)},
                                      {"notes", record.notes},
                                      {"source_module_id", record.source_module_id},
                                      {"created_at", record.created_at},
                                      {"updated_at", record.updated_at},
                                      {"version", std::to_string(record.version)}}));
        return ok_result();
    } catch (const std::exception& e) {
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::upsert_project_memory_record(const ProjectMemoryRecord& record) {
    try {
        project_memory_by_id_[record.record_id] = record;
        append_line(layer_file(data_root_, MemoryLayer::ProjectMemory),
                    serialize_fields({{"record_id", record.record_id},
                                      {"project_entity_id", record.project_entity_id},
                                      {"objectives", serialize_list(record.objectives)},
                                      {"milestones", serialize_list(record.milestones)},
                                      {"active_task_ids", serialize_list(record.active_task_ids)},
                                      {"dependency_ids", serialize_list(record.dependency_ids)},
                                      {"progress_summary", record.progress_summary},
                                      {"source_module_id", record.source_module_id},
                                      {"created_at", record.created_at},
                                      {"updated_at", record.updated_at},
                                      {"version", std::to_string(record.version)}}));
        return ok_result();
    } catch (const std::exception& e) {
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::append_behavioral_history_record(const BehavioralHistoryRecord& record) {
    try {
        behavioral_history_records_.push_back(record);
        append_line(layer_file(data_root_, MemoryLayer::BehavioralHistory),
                    serialize_fields({{"record_id", record.record_id},
                                      {"subject_key", record.subject_key},
                                      {"record_type", record.record_type},
                                      {"completion_state", record.completion_state},
                                      {"response_state", record.response_state},
                                      {"score_or_value", record.score_or_value},
                                      {"source_module_id", record.source_module_id},
                                      {"timestamp", record.timestamp},
                                      {"version", std::to_string(record.version)}}));
        return ok_result();
    } catch (const std::exception& e) {
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::upsert_retrieval_index_record(const KnowledgeRetrievalIndexRecord& record) {
    try {
        retrieval_index_by_id_[record.record_id] = record;
        append_line(layer_file(data_root_, MemoryLayer::KnowledgeRetrievalIndex),
                    serialize_fields({{"record_id", record.record_id},
                                      {"document_id", record.document_id},
                                      {"source_reference", record.source_reference},
                                      {"indexing_status", record.indexing_status},
                                      {"metadata", serialize_map(record.metadata)},
                                      {"source_module_id", record.source_module_id},
                                      {"created_at", record.created_at},
                                      {"updated_at", record.updated_at},
                                      {"version", std::to_string(record.version)}}));
        return ok_result();
    } catch (const std::exception& e) {
        return error_result(e.what());
    }
}

MemoryResultWith<LifeEntity> FileMemoryStore::get_entity_by_id(const EntityId& entity_id) const {
    emit_memory_event(EventCategory::MemoryReadPerformed,
                      MemoryLayer::LifeModelGraph,
                      entity_id,
                      MemoryOperationType::Read,
                      "",
                      "Entity read performed.");
    auto it = entities_by_id_.find(entity_id);
    if (it == entities_by_id_.end()) {
        return error_with<LifeEntity>("entity not found");
    }
    return success(it->second);
}

MemoryResultWith<std::vector<LifeEntity>> FileMemoryStore::list_entities_by_type(EntityType type) const {
    std::vector<LifeEntity> records;
    for (const auto& [_, entity] : entities_by_id_) {
        if (entity.entity_type == type) {
            records.push_back(entity);
        }
    }
    std::sort(records.begin(), records.end(), [](const auto& lhs, const auto& rhs) { return lhs.entity_id < rhs.entity_id; });
    return success(records);
}

MemoryResultWith<std::vector<LifeRelationship>> FileMemoryStore::get_relationships_for_entity(
    const EntityId& entity_id) const {
    std::vector<LifeRelationship> records;
    for (const auto& [_, rel] : relationships_by_id_) {
        if (rel.from_entity_id == entity_id || rel.to_entity_id == entity_id) {
            records.push_back(rel);
        }
    }
    std::sort(records.begin(), records.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.relationship_id < rhs.relationship_id; });
    return success(records);
}

MemoryResultWith<ProjectMemoryRecord> FileMemoryStore::get_project_record_by_project_entity_id(
    const EntityId& project_entity_id) const {
    for (const auto& [_, record] : project_memory_by_id_) {
        if (record.project_entity_id == project_entity_id) {
            return success(record);
        }
    }
    return error_with<ProjectMemoryRecord>("project record not found");
}

MemoryResultWith<std::vector<PreferenceRecord>> FileMemoryStore::get_preferences_by_prefix_or_key(
    const std::string& key_or_prefix,
    bool exact_match) const {
    std::vector<PreferenceRecord> records;
    for (const auto& [_, record] : preferences_by_id_) {
        if ((exact_match && record.preference_key == key_or_prefix) ||
            (!exact_match && record.preference_key.rfind(key_or_prefix, 0) == 0)) {
            records.push_back(record);
        }
    }
    std::sort(records.begin(), records.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.preference_key < rhs.preference_key; });
    return success(records);
}

MemoryResultWith<std::vector<EpisodicMemoryRecord>> FileMemoryStore::list_recent_episodic_records(
    std::size_t max_records) const {
    std::vector<EpisodicMemoryRecord> records = episodic_records_;
    std::sort(records.begin(), records.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.timestamp > rhs.timestamp; });
    if (records.size() > max_records) {
        records.resize(max_records);
    }
    return success(records);
}

MemoryResultWith<std::vector<BehavioralHistoryRecord>> FileMemoryStore::list_behavioral_history_for_subject(
    const std::string& subject_key) const {
    std::vector<BehavioralHistoryRecord> records;
    for (const auto& record : behavioral_history_records_) {
        if (record.subject_key == subject_key) {
            records.push_back(record);
        }
    }
    std::sort(records.begin(), records.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.timestamp < rhs.timestamp; });
    return success(records);
}

MemoryResultWith<MemoryRecordView> FileMemoryStore::read_record_by_layer_and_id(MemoryLayer layer,
                                                                                  const RecordId& id) const {
    if (layer == MemoryLayer::PreferenceMemory) {
        auto it = preferences_by_id_.find(id);
        if (it == preferences_by_id_.end()) {
            return error_with<MemoryRecordView>("record not found");
        }
        return success(MemoryRecordView{.layer = layer,
                                        .record_id = id,
                                        .fields = {{"preference_key", it->second.preference_key},
                                                   {"value", it->second.value}}});
    }
    return error_with<MemoryRecordView>("layer not implemented for record read");
}

MemoryResultWith<std::vector<MemoryRecordView>> FileMemoryStore::list_records_for_query(
    MemoryLayer layer,
    const QueryToken& token) const {
    std::vector<MemoryRecordView> records;
    if (layer == MemoryLayer::EpisodicMemory) {
        for (const auto& record : episodic_records_) {
            if (record.event_type == token || record.summary.find(token) != std::string::npos) {
                records.push_back(MemoryRecordView{.layer = layer,
                                                   .record_id = record.record_id,
                                                   .fields = {{"event_type", record.event_type},
                                                              {"summary", record.summary}}});
            }
        }
    }
    emit_memory_event(EventCategory::MemoryQueryPerformed,
                      layer,
                      token,
                      MemoryOperationType::Query,
                      "",
                      "Memory query performed.");
    return success(records);
}

MemoryResultWith<LifeGraphSnapshotMetadata> FileMemoryStore::get_life_graph_snapshot_metadata() const {
    return success(LifeGraphSnapshotMetadata{.snapshot_version = snapshot_version_,
                                             .created_at = current_timestamp_utc(),
                                             .schema_version = "sprint2-v1",
                                             .entity_count = entities_by_id_.size(),
                                             .relationship_count = relationships_by_id_.size()});
}

MemoryResultWith<MemorySummary> FileMemoryStore::get_memory_summary() const {
    return success(MemorySummary{.entity_count = entities_by_id_.size(),
                                 .relationship_count = relationships_by_id_.size(),
                                 .episodic_count = episodic_records_.size(),
                                 .preference_count = preferences_by_id_.size(),
                                 .relationship_memory_count = relationship_memory_by_id_.size(),
                                 .project_memory_count = project_memory_by_id_.size(),
                                 .behavioral_history_count = behavioral_history_records_.size(),
                                 .retrieval_index_count = retrieval_index_by_id_.size(),
                                 .integration_configuration_count = integration_configs_by_id_.size()});
}

MemoryResult FileMemoryStore::load_from_disk() {
    emit_memory_event(EventCategory::MemoryLoadStarted,
                      MemoryLayer::LifeModelGraph,
                      "",
                      MemoryOperationType::Read,
                      "",
                      "Memory load started.");
    try {
        entities_by_id_.clear();
        relationships_by_id_.clear();
        preferences_by_id_.clear();
        relationship_memory_by_id_.clear();
        project_memory_by_id_.clear();
        retrieval_index_by_id_.clear();
        episodic_records_.clear();
        behavioral_history_records_.clear();
        snapshot_version_ = 0;

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::LifeModelGraph))) {
            auto fields = parse_fields(line);
            if (fields["record_kind"] != "life_entity") {
                throw std::runtime_error("malformed life entity record kind");
            }
            LifeEntity entity{.entity_id = fields["entity_id"],
                              .entity_type = parse_entity_type(fields["entity_type"]),
                              .display_name = fields["display_name"],
                              .canonical_name = fields["canonical_name"],
                              .description = fields["description"],
                              .created_at = fields["created_at"],
                              .updated_at = fields["updated_at"],
                              .source_module_id = fields["source_module_id"],
                              .version = static_cast<MemoryVersion>(std::stoull(fields["version"])),
                              .archived = fields["archived"] == "1",
                              .attributes = parse_map(fields["attributes"])};
            entities_by_id_[entity.entity_id] = entity;
        }

        for (const auto& line : read_lines(relationship_file(data_root_))) {
            auto fields = parse_fields(line);
            if (fields["record_kind"] != "life_relationship") {
                throw std::runtime_error("malformed life relationship record kind");
            }
            LifeRelationship rel{.relationship_id = fields["relationship_id"],
                                 .from_entity_id = fields["from_entity_id"],
                                 .to_entity_id = fields["to_entity_id"],
                                 .relationship_type = parse_relationship_type(fields["relationship_type"]),
                                 .created_at = fields["created_at"],
                                 .updated_at = fields["updated_at"],
                                 .source_module_id = fields["source_module_id"],
                                 .version = static_cast<MemoryVersion>(std::stoull(fields["version"])),
                                 .attributes = parse_map(fields["attributes"])};
            relationships_by_id_[rel.relationship_id] = rel;
        }

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::EpisodicMemory))) {
            auto fields = parse_fields(line);
            episodic_records_.push_back(EpisodicMemoryRecord{.record_id = fields["record_id"],
                                                             .timestamp = fields["timestamp"],
                                                             .event_type = fields["event_type"],
                                                             .source_module_id = fields["source_module_id"],
                                                             .associated_entity_ids = parse_list(fields["associated_entity_ids"]),
                                                             .summary = fields["summary"],
                                                             .details = parse_map(fields["details"]),
                                                             .version = static_cast<MemoryVersion>(
                                                                 std::stoull(fields["version"]))});
        }

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::PreferenceMemory))) {
            auto fields = parse_fields(line);
            PreferenceRecord record{.record_id = fields["record_id"],
                                    .preference_key = fields["preference_key"],
                                    .value = fields["value"],
                                    .confidence = std::stod(fields["confidence"]),
                                    .source_module_id = fields["source_module_id"],
                                    .created_at = fields["created_at"],
                                    .updated_at = fields["updated_at"],
                                    .version = static_cast<MemoryVersion>(std::stoull(fields["version"]))};
            preferences_by_id_[record.record_id] = record;
        }

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::RelationshipMemory))) {
            auto fields = parse_fields(line);
            RelationshipMemoryRecord record{.record_id = fields["record_id"],
                                            .related_person_entity_id = fields["related_person_entity_id"],
                                            .communication_cadence = fields["communication_cadence"],
                                            .important_dates = parse_list(fields["important_dates"]),
                                            .shared_interests = parse_list(fields["shared_interests"]),
                                            .notes = fields["notes"],
                                            .source_module_id = fields["source_module_id"],
                                            .created_at = fields["created_at"],
                                            .updated_at = fields["updated_at"],
                                            .version = static_cast<MemoryVersion>(std::stoull(fields["version"]))};
            relationship_memory_by_id_[record.record_id] = record;
        }

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::ProjectMemory))) {
            auto fields = parse_fields(line);
            ProjectMemoryRecord record{.record_id = fields["record_id"],
                                       .project_entity_id = fields["project_entity_id"],
                                       .objectives = parse_list(fields["objectives"]),
                                       .milestones = parse_list(fields["milestones"]),
                                       .active_task_ids = parse_list(fields["active_task_ids"]),
                                       .dependency_ids = parse_list(fields["dependency_ids"]),
                                       .progress_summary = fields["progress_summary"],
                                       .source_module_id = fields["source_module_id"],
                                       .created_at = fields["created_at"],
                                       .updated_at = fields["updated_at"],
                                       .version = static_cast<MemoryVersion>(std::stoull(fields["version"]))};
            project_memory_by_id_[record.record_id] = record;
        }

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::BehavioralHistory))) {
            auto fields = parse_fields(line);
            behavioral_history_records_.push_back(BehavioralHistoryRecord{
                .record_id = fields["record_id"],
                .subject_key = fields["subject_key"],
                .record_type = fields["record_type"],
                .completion_state = fields["completion_state"],
                .response_state = fields["response_state"],
                .score_or_value = fields["score_or_value"],
                .source_module_id = fields["source_module_id"],
                .timestamp = fields["timestamp"],
                .version = static_cast<MemoryVersion>(std::stoull(fields["version"]))});
        }

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::KnowledgeRetrievalIndex))) {
            auto fields = parse_fields(line);
            KnowledgeRetrievalIndexRecord record{.record_id = fields["record_id"],
                                                 .document_id = fields["document_id"],
                                                 .source_reference = fields["source_reference"],
                                                 .indexing_status = fields["indexing_status"],
                                                 .metadata = parse_map(fields["metadata"]),
                                                 .source_module_id = fields["source_module_id"],
                                                 .created_at = fields["created_at"],
                                                 .updated_at = fields["updated_at"],
                                                 .version = static_cast<MemoryVersion>(
                                                     std::stoull(fields["version"]))};
            retrieval_index_by_id_[record.record_id] = record;
        }

        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::IntegrationConfiguration))) {
            auto fields = parse_fields(line);
            IntegrationConfigurationRecord record{.integration_config_id = fields["integration_config_id"],
                                                  .integration_id = fields["integration_id"],
                                                  .display_name = fields["display_name"],
                                                  .enabled = fields["enabled"] == "1",
                                                  .status = parse_integration_status(fields["status"]),
                                                  .capability_visibility = parse_list(fields["capability_visibility"]),
                                                  .connection_diagnostics = parse_map(fields["connection_diagnostics"]),
                                                  .credential_storage_mode =
                                                      parse_storage_mode(fields["credential_storage_mode"]),
                                                  .credential_reference = fields["credential_reference"],
                                                  .non_secret_settings = parse_map(fields["non_secret_settings"]),
                                                  .created_at = fields["created_at"],
                                                  .updated_at = fields["updated_at"],
                                                  .version = static_cast<MemoryVersion>(
                                                      std::stoull(fields["version"]))};
            integration_configs_by_id_[record.integration_config_id] = record;
        }

        snapshot_version_ = entities_by_id_.size() + relationships_by_id_.size();
        emit_memory_event(EventCategory::MemoryLoadCompleted,
                          MemoryLayer::LifeModelGraph,
                          "",
                          MemoryOperationType::Read,
                          "",
                          "Memory load completed.");
        return ok_result();
    } catch (const std::exception& e) {
        emit_memory_event(EventCategory::MemoryLoadFailed,
                          MemoryLayer::LifeModelGraph,
                          "",
                          MemoryOperationType::Read,
                          "",
                          e.what());
        return error_result(e.what());
    }
}

MemoryResult FileMemoryStore::persist_to_disk() {
    ensure_parent(manifest_file(data_root_));
    std::ofstream out(manifest_file(data_root_));
    if (!out.is_open()) {
        return error_result("unable to write manifest");
    }
    out << "{\n"
        << "  \"schema_version\": \"sprint2-v1\",\n"
        << "  \"snapshot_version\": " << snapshot_version_ << ",\n"
        << "  \"updated_at\": \"" << current_timestamp_utc() << "\"\n"
        << "}\n";
    return static_cast<bool>(out) ? ok_result() : error_result("manifest write failed");
}

std::string to_string(MemoryLayer value) {
    switch (value) {
        case MemoryLayer::LifeModelGraph: return "LifeModelGraph";
        case MemoryLayer::EpisodicMemory: return "EpisodicMemory";
        case MemoryLayer::PreferenceMemory: return "PreferenceMemory";
        case MemoryLayer::RelationshipMemory: return "RelationshipMemory";
        case MemoryLayer::ProjectMemory: return "ProjectMemory";
        case MemoryLayer::BehavioralHistory: return "BehavioralHistory";
        case MemoryLayer::KnowledgeRetrievalIndex: return "KnowledgeRetrievalIndex";
        case MemoryLayer::IntegrationConfiguration: return "IntegrationConfiguration";
    }
    return "Unknown";
}

std::string to_string(EntityType value) {
    switch (value) {
        case EntityType::Goal: return "Goal";
        case EntityType::Commitment: return "Commitment";
        case EntityType::Relationship: return "Relationship";
        case EntityType::Project: return "Project";
        case EntityType::Environment: return "Environment";
        case EntityType::Location: return "Location";
        case EntityType::Domain: return "Domain";
        case EntityType::Task: return "Task";
        case EntityType::Habit: return "Habit";
        case EntityType::Person: return "Person";
        case EntityType::Preference: return "Preference";
        case EntityType::Integration: return "Integration";
    }
    return "Unknown";
}

std::string to_string(RelationshipType value) {
    switch (value) {
        case RelationshipType::Supports: return "Supports";
        case RelationshipType::Contains: return "Contains";
        case RelationshipType::AssociatedWith: return "AssociatedWith";
        case RelationshipType::ScheduledIn: return "ScheduledIn";
        case RelationshipType::DependsOn: return "DependsOn";
        case RelationshipType::RelatedTo: return "RelatedTo";
        case RelationshipType::OwnedBy: return "OwnedBy";
        case RelationshipType::ConfiguredBy: return "ConfiguredBy";
    }
    return "Unknown";
}

std::string to_string(MemoryOperationType value) {
    switch (value) {
        case MemoryOperationType::Insert: return "Insert";
        case MemoryOperationType::Update: return "Update";
        case MemoryOperationType::Upsert: return "Upsert";
        case MemoryOperationType::Delete: return "Delete";
        case MemoryOperationType::Read: return "Read";
        case MemoryOperationType::Query: return "Query";
    }
    return "Unknown";
}

std::string to_string(IntegrationStatus value) {
    switch (value) {
        case IntegrationStatus::Disabled: return "Disabled";
        case IntegrationStatus::Enabled: return "Enabled";
        case IntegrationStatus::Error: return "Error";
        case IntegrationStatus::Unknown: return "Unknown";
    }
    return "Unknown";
}

std::string to_string(CredentialStorageMode value) {
    switch (value) {
        case CredentialStorageMode::InlinePlaceholderOnly: return "InlinePlaceholderOnly";
        case CredentialStorageMode::ExternalSecretReference: return "ExternalSecretReference";
        case CredentialStorageMode::Unset: return "Unset";
    }
    return "Unknown";
}

}  // namespace life_orchestrator::core
