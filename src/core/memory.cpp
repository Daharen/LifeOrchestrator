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
    std::sort(ordered.begin(), ordered.end());
    std::ostringstream out;
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (i > 0) out << '|';
        out << escape(ordered[i].first) << '=' << escape(ordered[i].second);
    }
    return out.str();
}

StringMap parse_map(const std::string& raw) {
    if (raw.empty()) return {};
    StringMap result;
    for (const auto& pair : split_escaped(raw, '|')) {
        const auto pos = pair.find('=');
        if (pos == std::string::npos) throw std::runtime_error("map pair missing '='");
        result[unescape(pair.substr(0, pos))] = unescape(pair.substr(pos + 1));
    }
    return result;
}

std::string serialize_list(const std::vector<std::string>& values) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << '|';
        out << escape(values[i]);
    }
    return out.str();
}

std::vector<std::string> parse_list(const std::string& raw) {
    if (raw.empty()) return {};
    auto parts = split_escaped(raw, '|');
    for (auto& part : parts) part = unescape(part);
    return parts;
}

std::string serialize_fields(const std::vector<std::pair<std::string, std::string>>& fields) {
    std::ostringstream out;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) out << ';';
        out << fields[i].first << '=' << escape(fields[i].second);
    }
    return out.str();
}

StringMap parse_fields(const std::string& line) {
    StringMap fields;
    for (const auto& token : split_escaped(line, ';')) {
        const auto pos = token.find('=');
        if (pos == std::string::npos) throw std::runtime_error("field missing '='");
        fields[token.substr(0, pos)] = unescape(token.substr(pos + 1));
    }
    return fields;
}

void ensure_parent(const std::filesystem::path& path) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
}

void append_line(const std::filesystem::path& path, const std::string& line) {
    ensure_parent(path);
    std::ofstream out(path, std::ios::app);
    if (!out.is_open()) throw std::runtime_error("unable to open file for append: " + path.string());
    out << line << '\n';
}

std::vector<std::string> read_lines(const std::filesystem::path& path) {
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    return lines;
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

bool overlaps(const TimestampString& start_a, const TimestampString& end_a, const TimestampString& start_b, const TimestampString& end_b) {
    return start_a < end_b && start_b < end_a;
}

bool in_window(const TimestampString& start_a, const TimestampString& end_a, const TimestampString& window_start, const TimestampString& window_end) {
    return overlaps(start_a, end_a, window_start, window_end);
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
        case MemoryLayer::IntegrationConfiguration: return root / "memory/integration_configuration/records.ndjson";
        case MemoryLayer::Scheduling: return root / "memory/scheduling/summary.ndjson";
        case MemoryLayer::BehavioralTriage: return root / "memory/behavioral_triage/summary.ndjson";
        case MemoryLayer::ProceduralAuditing: return root / "memory/procedural_auditing/summary.ndjson";
    }
    return root / "memory/unknown.ndjson";
}

std::filesystem::path scheduling_file(const std::filesystem::path& root, const std::string& name) {
    return root / ("memory/scheduling/" + name + ".ndjson");
}

std::filesystem::path behavioral_file(const std::filesystem::path& root, const std::string& name) {
    return root / ("memory/behavioral_triage/" + name + ".ndjson");
}
std::filesystem::path procedural_file(const std::filesystem::path& root, const std::string& name) {
    return root / ("memory/procedural_auditing/" + name + ".ndjson");
}

std::filesystem::path relationship_file(const std::filesystem::path& root) { return root / "memory/life_graph/relationships.ndjson"; }
std::filesystem::path manifest_file(const std::filesystem::path& root) { return root / "memory/metadata/store_manifest.json"; }

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
ScheduleStatus parse_schedule_status(const std::string& value) {
    if (value == "Pending") return ScheduleStatus::Pending;
    if (value == "Scheduled") return ScheduleStatus::Scheduled;
    if (value == "Completed") return ScheduleStatus::Completed;
    if (value == "Cancelled") return ScheduleStatus::Cancelled;
    return ScheduleStatus::Rejected;
}
SchedulingPriority parse_priority(const std::string& value) {
    if (value == "Low") return SchedulingPriority::Low;
    if (value == "High") return SchedulingPriority::High;
    if (value == "Critical") return SchedulingPriority::Critical;
    return SchedulingPriority::Normal;
}
ProposalStatus parse_proposal_status(const std::string& value) {
    if (value == "Accepted") return ProposalStatus::Accepted;
    if (value == "Rejected") return ProposalStatus::Rejected;
    if (value == "Expired") return ProposalStatus::Expired;
    if (value == "Committed") return ProposalStatus::Committed;
    return ProposalStatus::Proposed;
}
ConflictType parse_conflict_type(const std::string& value) {
    if (value == "Overlap") return ConflictType::Overlap;
    if (value == "OutsideAvailability") return ConflictType::OutsideAvailability;
    if (value == "DurationInsufficient") return ConflictType::DurationInsufficient;
    if (value == "DependencyViolation") return ConflictType::DependencyViolation;
    return ConflictType::InvalidWindow;
}
SchedulingCandidateStatus parse_scheduling_candidate_status(const std::string& value) {
    if (value == "deferred") return SchedulingCandidateStatus::Deferred;
    if (value == "scheduled_proposal_generated") return SchedulingCandidateStatus::ScheduledProposalGenerated;
    if (value == "rejected") return SchedulingCandidateStatus::Rejected;
    return SchedulingCandidateStatus::Candidate;
}

ScheduleProposalArtifactStatus parse_schedule_proposal_artifact_status(const std::string& value) {
    if (value == "conflict_detected") return ScheduleProposalArtifactStatus::ConflictDetected;
    if (value == "superseded") return ScheduleProposalArtifactStatus::Superseded;
    if (value == "rejected") return ScheduleProposalArtifactStatus::Rejected;
    return ScheduleProposalArtifactStatus::Proposed;
}

ScheduleProposalConflictStatus parse_schedule_proposal_conflict_status(const std::string& value) {
    if (value == "potential_conflict") return ScheduleProposalConflictStatus::PotentialConflict;
    if (value == "blocked") return ScheduleProposalConflictStatus::Blocked;
    return ScheduleProposalConflictStatus::None;
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
    if (!event_logger_) return;
    event_logger_->append({category,
                           current_timestamp_utc(),
                           "memory",
                           source_module_id,
                           "memory.store",
                           message,
                           {{"layer", to_string(layer)}, {"record_id", record_id}, {"operation", to_string(operation)}}});
}

MemoryResult FileMemoryStore::upsert_life_entity(const LifeEntity& entity) {
    try {
        entities_by_id_[entity.entity_id] = entity;
        append_line(layer_file(data_root_, MemoryLayer::LifeModelGraph), serialize_fields({{"record_kind", "life_entity"}, {"entity_id", entity.entity_id}, {"entity_type", to_string(entity.entity_type)}, {"display_name", entity.display_name}, {"canonical_name", entity.canonical_name}, {"description", entity.description}, {"created_at", entity.created_at}, {"updated_at", entity.updated_at}, {"source_module_id", entity.source_module_id}, {"version", std::to_string(entity.version)}, {"archived", entity.archived ? "1" : "0"}, {"attributes", serialize_map(entity.attributes)}}));
        ++snapshot_version_;
        emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::LifeModelGraph, entity.entity_id, MemoryOperationType::Upsert, entity.source_module_id, "Life entity write completed.");
        return ok_result();
    } catch (const std::exception& e) { return error_result(e.what()); }
}
MemoryResult FileMemoryStore::upsert_life_relationship(const LifeRelationship& relationship) {
    try {
        relationships_by_id_[relationship.relationship_id] = relationship;
        append_line(relationship_file(data_root_), serialize_fields({{"record_kind", "life_relationship"}, {"relationship_id", relationship.relationship_id}, {"from_entity_id", relationship.from_entity_id}, {"to_entity_id", relationship.to_entity_id}, {"relationship_type", to_string(relationship.relationship_type)}, {"created_at", relationship.created_at}, {"updated_at", relationship.updated_at}, {"source_module_id", relationship.source_module_id}, {"version", std::to_string(relationship.version)}, {"attributes", serialize_map(relationship.attributes)}}));
        ++snapshot_version_;
        return ok_result();
    } catch (const std::exception& e) { return error_result(e.what()); }
}
MemoryResult FileMemoryStore::append_episodic_record(const EpisodicMemoryRecord& record) {
    try {
        episodic_records_.push_back(record);
        append_line(layer_file(data_root_, MemoryLayer::EpisodicMemory), serialize_fields({{"record_id", record.record_id}, {"timestamp", record.timestamp}, {"event_type", record.event_type}, {"source_module_id", record.source_module_id}, {"associated_entity_ids", serialize_list(record.associated_entity_ids)}, {"summary", record.summary}, {"details", serialize_map(record.details)}, {"version", std::to_string(record.version)}}));
        emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::EpisodicMemory, record.record_id, MemoryOperationType::Insert, record.source_module_id, "Episodic write completed.");
        return ok_result();
    } catch (const std::exception& e) { return error_result(e.what()); }
}
MemoryResult FileMemoryStore::upsert_preference_record(const PreferenceRecord& record) { try { preferences_by_id_[record.record_id] = record; append_line(layer_file(data_root_, MemoryLayer::PreferenceMemory), serialize_fields({{"record_id", record.record_id}, {"preference_key", record.preference_key}, {"value", record.value}, {"confidence", std::to_string(record.confidence)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_relationship_memory_record(const RelationshipMemoryRecord& record) { try { relationship_memory_by_id_[record.record_id] = record; append_line(layer_file(data_root_, MemoryLayer::RelationshipMemory), serialize_fields({{"record_id", record.record_id}, {"related_person_entity_id", record.related_person_entity_id}, {"communication_cadence", record.communication_cadence}, {"important_dates", serialize_list(record.important_dates)}, {"shared_interests", serialize_list(record.shared_interests)}, {"notes", record.notes}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_project_memory_record(const ProjectMemoryRecord& record) { try { project_memory_by_id_[record.record_id] = record; append_line(layer_file(data_root_, MemoryLayer::ProjectMemory), serialize_fields({{"record_id", record.record_id}, {"project_entity_id", record.project_entity_id}, {"objectives", serialize_list(record.objectives)}, {"milestones", serialize_list(record.milestones)}, {"active_task_ids", serialize_list(record.active_task_ids)}, {"dependency_ids", serialize_list(record.dependency_ids)}, {"progress_summary", record.progress_summary}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_behavioral_history_record(const BehavioralHistoryRecord& record) { try { behavioral_history_records_.push_back(record); append_line(layer_file(data_root_, MemoryLayer::BehavioralHistory), serialize_fields({{"record_id", record.record_id}, {"subject_key", record.subject_key}, {"record_type", record.record_type}, {"completion_state", record.completion_state}, {"response_state", record.response_state}, {"score_or_value", record.score_or_value}, {"source_module_id", record.source_module_id}, {"timestamp", record.timestamp}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_retrieval_index_record(const KnowledgeRetrievalIndexRecord& record) { try { retrieval_index_by_id_[record.record_id] = record; append_line(layer_file(data_root_, MemoryLayer::KnowledgeRetrievalIndex), serialize_fields({{"record_id", record.record_id}, {"document_id", record.document_id}, {"source_reference", record.source_reference}, {"indexing_status", record.indexing_status}, {"metadata", serialize_map(record.metadata)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }

MemoryResult FileMemoryStore::upsert_scheduled_commitment(const ScheduledCommitment& record) { try { commitments_by_id_[record.schedule_item_id] = record; append_line(scheduling_file(data_root_, "commitments"), serialize_fields({{"schedule_item_id", record.schedule_item_id}, {"related_entity_id", record.related_entity_id}, {"title", record.title}, {"description", record.description}, {"start_time", record.start_time}, {"end_time", record.end_time}, {"timezone", record.timezone}, {"priority", to_string(record.priority)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}, {"status", to_string(record.status)}, {"attributes", serialize_map(record.attributes)}})); emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::Scheduling, record.schedule_item_id, MemoryOperationType::Upsert, record.source_module_id, "Scheduling commitment write completed."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_task_candidate(const SchedulingTaskCandidate& record) { try { task_candidates_by_id_[record.schedule_item_id] = record; append_line(scheduling_file(data_root_, "task_candidates"), serialize_fields({{"schedule_item_id", record.schedule_item_id}, {"related_entity_id", record.related_entity_id}, {"title", record.title}, {"description", record.description}, {"estimated_duration_minutes", std::to_string(record.estimated_duration_minutes)}, {"earliest_start", record.earliest_start}, {"latest_end", record.latest_end}, {"priority", to_string(record.priority)}, {"splittable", record.splittable ? "1" : "0"}, {"required_buffer_before_minutes", std::to_string(record.required_buffer_before_minutes)}, {"required_buffer_after_minutes", std::to_string(record.required_buffer_after_minutes)}, {"dependency_ids", serialize_list(record.dependency_ids)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}, {"status", to_string(record.status)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_scheduling_candidate_record(const SchedulingCandidateRecord& record) { try { scheduling_candidates_by_id_[record.candidate_id] = record; append_line(scheduling_file(data_root_, "candidates"), serialize_fields({{"candidate_id", record.candidate_id}, {"source_intervention_id", record.source_intervention_id}, {"source_proposal_id", record.source_proposal_id}, {"source_audit_run_id", record.source_audit_run_id}, {"source_activity_id", record.source_activity_id}, {"estimated_duration_minutes", std::to_string(record.estimated_duration_minutes)}, {"urgency", record.urgency}, {"scheduling_window_hint", record.scheduling_window_hint}, {"recommended_time_of_day", record.recommended_time_of_day}, {"recommended_day_span", record.recommended_day_span}, {"rationale", record.rationale}, {"status", to_string(record.status)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::Scheduling, record.candidate_id, MemoryOperationType::Upsert, record.source_module_id, "Scheduling candidate write completed."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_schedule_proposal_artifact(const ScheduleProposalArtifact& record) { try { schedule_proposal_artifacts_by_id_[record.schedule_proposal_id] = record; append_line(scheduling_file(data_root_, "proposal_artifacts"), serialize_fields({{"schedule_proposal_id", record.schedule_proposal_id}, {"source_candidate_id", record.source_candidate_id}, {"source_intervention_id", record.source_intervention_id}, {"source_proposal_id", record.source_proposal_id}, {"source_audit_run_id", record.source_audit_run_id}, {"source_activity_id", record.source_activity_id}, {"proposed_start_time", record.proposed_start_time}, {"proposed_end_time", record.proposed_end_time}, {"timezone", record.timezone}, {"duration_minutes", std::to_string(record.duration_minutes)}, {"scheduling_window_hint", record.scheduling_window_hint}, {"recommended_time_of_day", record.recommended_time_of_day}, {"rationale", record.rationale}, {"proposal_status", to_string(record.proposal_status)}, {"conflict_status", to_string(record.conflict_status)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); emit_memory_event(EventCategory::SchedulingProposalGenerated, MemoryLayer::Scheduling, record.schedule_proposal_id, MemoryOperationType::Upsert, record.source_module_id, "Schedule proposal artifact persisted."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_availability_window(const AvailabilityWindow& record) { try { windows_by_id_[record.window_id] = record; append_line(scheduling_file(data_root_, "availability_windows"), serialize_fields({{"window_id", record.window_id}, {"title", record.title}, {"start_time", record.start_time}, {"end_time", record.end_time}, {"timezone", record.timezone}, {"availability_type", record.availability_type}, {"recurrence_placeholder", record.recurrence_placeholder}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_constraint_set(const SchedulingConstraintSet& record) { try { constraint_sets_by_id_[record.constraint_set_id] = record; append_line(scheduling_file(data_root_, "constraint_sets"), serialize_fields({{"constraint_set_id", record.constraint_set_id}, {"max_commitments_per_day", std::to_string(record.max_commitments_per_day)}, {"minimum_gap_minutes", std::to_string(record.minimum_gap_minutes)}, {"working_hours_only", record.working_hours_only ? "1" : "0"}, {"allowed_window_ids", serialize_list(record.allowed_window_ids)}, {"blocked_window_ids", serialize_list(record.blocked_window_ids)}, {"preference_tags", serialize_list(record.preference_tags)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_proposal(const SchedulingProposal& record) { try { proposals_by_id_[record.proposal_id] = record; append_line(scheduling_file(data_root_, "proposals"), serialize_fields({{"proposal_id", record.proposal_id}, {"related_task_candidate_id", record.related_task_candidate_id}, {"proposed_start_time", record.proposed_start_time}, {"proposed_end_time", record.proposed_end_time}, {"timezone", record.timezone}, {"proposal_rank", std::to_string(record.proposal_rank)}, {"rationale", record.rationale}, {"based_on_constraint_set_id", record.based_on_constraint_set_id}, {"generated_at", record.generated_at}, {"source_module_id", record.source_module_id}, {"version", std::to_string(record.version)}, {"status", to_string(record.status)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_decision(const SchedulingDecisionRecord& record) { try { decisions_by_id_[record.decision_id] = record; append_line(scheduling_file(data_root_, "decisions"), serialize_fields({{"decision_id", record.decision_id}, {"proposal_id", record.proposal_id}, {"resulting_commitment_id", record.resulting_commitment_id}, {"decision_type", record.decision_type}, {"decided_at", record.decided_at}, {"source_module_id", record.source_module_id}, {"summary", record.summary}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_conflict(const SchedulingConflict& record) { try { conflicts_by_id_[record.conflict_id] = record; append_line(scheduling_file(data_root_, "conflicts"), serialize_fields({{"conflict_id", record.conflict_id}, {"conflict_type", to_string(record.conflict_type)}, {"primary_schedule_item_id", record.primary_schedule_item_id}, {"secondary_schedule_item_id", record.secondary_schedule_item_id}, {"message", record.message}, {"detected_at", record.detected_at}, {"source_module_id", record.source_module_id}, {"fields", serialize_map(record.fields)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }

MemoryResultWith<LifeEntity> FileMemoryStore::get_entity_by_id(const EntityId& entity_id) const { auto it = entities_by_id_.find(entity_id); return it == entities_by_id_.end() ? error_with<LifeEntity>("entity not found") : success(it->second); }
MemoryResultWith<std::vector<LifeEntity>> FileMemoryStore::list_entities_by_type(EntityType type) const { std::vector<LifeEntity> out; for (const auto& [_, entity] : entities_by_id_) if (entity.entity_type == type) out.push_back(entity); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.entity_id < b.entity_id; }); return success(out); }
MemoryResultWith<std::vector<LifeRelationship>> FileMemoryStore::get_relationships_for_entity(const EntityId& entity_id) const { std::vector<LifeRelationship> out; for (const auto& [_, rel] : relationships_by_id_) if (rel.from_entity_id == entity_id || rel.to_entity_id == entity_id) out.push_back(rel); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.relationship_id < b.relationship_id; }); return success(out); }
MemoryResultWith<ProjectMemoryRecord> FileMemoryStore::get_project_record_by_project_entity_id(const EntityId& project_entity_id) const { for (const auto& [_, record] : project_memory_by_id_) if (record.project_entity_id == project_entity_id) return success(record); return error_with<ProjectMemoryRecord>("project record not found"); }
MemoryResultWith<std::vector<PreferenceRecord>> FileMemoryStore::get_preferences_by_prefix_or_key(const std::string& key_or_prefix, bool exact_match) const { std::vector<PreferenceRecord> out; for (const auto& [_, rec] : preferences_by_id_) if ((exact_match && rec.preference_key == key_or_prefix) || (!exact_match && rec.preference_key.rfind(key_or_prefix,0)==0)) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.preference_key < b.preference_key; }); return success(out); }
MemoryResultWith<std::vector<EpisodicMemoryRecord>> FileMemoryStore::list_recent_episodic_records(std::size_t max_records) const { auto out = episodic_records_; std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.timestamp > b.timestamp; }); if (out.size() > max_records) out.resize(max_records); return success(out); }
MemoryResultWith<std::vector<BehavioralHistoryRecord>> FileMemoryStore::list_behavioral_history_for_subject(const std::string& subject_key) const { std::vector<BehavioralHistoryRecord> out; for (const auto& rec : behavioral_history_records_) if (rec.subject_key == subject_key) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.timestamp < b.timestamp; }); return success(out); }
MemoryResultWith<MemoryRecordView> FileMemoryStore::read_record_by_layer_and_id(MemoryLayer layer, const RecordId& id) const { if (layer == MemoryLayer::PreferenceMemory) { auto it = preferences_by_id_.find(id); if (it == preferences_by_id_.end()) return error_with<MemoryRecordView>("record not found"); return success(MemoryRecordView{layer, id, {{"preference_key", it->second.preference_key}, {"value", it->second.value}}}); } if (layer == MemoryLayer::Scheduling) { auto it = commitments_by_id_.find(id); if (it != commitments_by_id_.end()) return success(MemoryRecordView{layer, id, {{"title", it->second.title}, {"start_time", it->second.start_time}}}); } return error_with<MemoryRecordView>("layer not implemented for record read"); }
MemoryResultWith<std::vector<MemoryRecordView>> FileMemoryStore::list_records_for_query(MemoryLayer layer, const QueryToken& token) const { std::vector<MemoryRecordView> out; if (layer == MemoryLayer::EpisodicMemory) { for (const auto& rec : episodic_records_) if (rec.event_type == token || rec.summary.find(token) != std::string::npos) out.push_back({layer, rec.record_id, {{"event_type", rec.event_type}, {"summary", rec.summary}}}); } return success(out); }
MemoryResultWith<LifeGraphSnapshotMetadata> FileMemoryStore::get_life_graph_snapshot_metadata() const { return success(LifeGraphSnapshotMetadata{snapshot_version_, current_timestamp_utc(), "sprint3-v1", entities_by_id_.size(), relationships_by_id_.size()}); }
MemoryResultWith<MemorySummary> FileMemoryStore::get_memory_summary() const { return success(MemorySummary{entities_by_id_.size(), relationships_by_id_.size(), episodic_records_.size(), preferences_by_id_.size(), relationship_memory_by_id_.size(), project_memory_by_id_.size(), behavioral_history_records_.size(), retrieval_index_by_id_.size(), integration_configs_by_id_.size(), commitments_by_id_.size(), task_candidates_by_id_.size(), scheduling_candidates_by_id_.size(), windows_by_id_.size(), constraint_sets_by_id_.size(), schedule_proposal_artifacts_by_id_.size(), decisions_by_id_.size(), conflicts_by_id_.size(), behavioral_proposals_by_id_.size(), behavioral_state_snapshots_by_id_.size(), behavioral_decisions_by_id_.size(), behavioral_backlog_items_by_id_.size(), behavioral_interventions_by_id_.size(), behavioral_reevaluations_by_id_.size(), activity_inventory_by_id_.size(), procedural_audit_runs_by_id_.size(), optimization_proposals_by_id_.size()}); }

MemoryResultWith<std::vector<ScheduledCommitment>> FileMemoryStore::list_commitments_in_window(const TimestampString& start_time, const TimestampString& end_time) const { std::vector<ScheduledCommitment> out; for (const auto& [_, rec] : commitments_by_id_) if (in_window(rec.start_time, rec.end_time, start_time, end_time)) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.start_time == b.start_time ? a.schedule_item_id < b.schedule_item_id : a.start_time < b.start_time; }); emit_memory_event(EventCategory::MemoryQueryPerformed, MemoryLayer::Scheduling, start_time + ":" + end_time, MemoryOperationType::Query, "", "Scheduling commitments queried."); return success(out); }
MemoryResultWith<std::vector<SchedulingTaskCandidate>> FileMemoryStore::list_task_candidates_by_status_and_range(ScheduleStatus status, const TimestampString& start_time, const TimestampString& end_time) const { std::vector<SchedulingTaskCandidate> out; for (const auto& [_, rec] : task_candidates_by_id_) if (rec.status == status && overlaps(rec.earliest_start, rec.latest_end, start_time, end_time)) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.earliest_start == b.earliest_start ? a.schedule_item_id < b.schedule_item_id : a.earliest_start < b.earliest_start; }); return success(out); }
MemoryResultWith<std::vector<AvailabilityWindow>> FileMemoryStore::list_availability_windows_in_window(const TimestampString& start_time, const TimestampString& end_time) const { std::vector<AvailabilityWindow> out; for (const auto& [_, rec] : windows_by_id_) if (in_window(rec.start_time, rec.end_time, start_time, end_time)) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.start_time == b.start_time ? a.window_id < b.window_id : a.start_time < b.start_time; }); return success(out); }
MemoryResultWith<std::vector<SchedulingCandidateRecord>> FileMemoryStore::list_scheduling_candidate_records() const { std::vector<SchedulingCandidateRecord> out; for (const auto& [_, rec] : scheduling_candidates_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ if (a.source_intervention_id != b.source_intervention_id) return a.source_intervention_id < b.source_intervention_id; return a.candidate_id < b.candidate_id; }); return success(out); }
MemoryResultWith<SchedulingCandidateRecord> FileMemoryStore::get_scheduling_candidate_record_by_id(const SchedulingCandidateId& candidate_id) const { auto it = scheduling_candidates_by_id_.find(candidate_id); return it == scheduling_candidates_by_id_.end() ? error_with<SchedulingCandidateRecord>("scheduling candidate not found") : success(it->second); }
MemoryResultWith<std::vector<ScheduleProposalArtifact>> FileMemoryStore::list_schedule_proposal_artifacts() const { std::vector<ScheduleProposalArtifact> out; for (const auto& [_, rec] : schedule_proposal_artifacts_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ if (a.source_candidate_id != b.source_candidate_id) return a.source_candidate_id < b.source_candidate_id; return a.schedule_proposal_id < b.schedule_proposal_id; }); return success(out); }
MemoryResultWith<ScheduleProposalArtifact> FileMemoryStore::get_schedule_proposal_artifact_by_id(const ScheduleProposalArtifactId& proposal_id) const { auto it = schedule_proposal_artifacts_by_id_.find(proposal_id); return it == schedule_proposal_artifacts_by_id_.end() ? error_with<ScheduleProposalArtifact>("schedule proposal artifact not found") : success(it->second); }
MemoryResultWith<std::vector<SchedulingProposal>> FileMemoryStore::list_proposals_for_task_candidate(const ScheduleItemId& task_candidate_id) const { std::vector<SchedulingProposal> out; for (const auto& [_, rec] : proposals_by_id_) if (rec.related_task_candidate_id == task_candidate_id) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.proposal_rank == b.proposal_rank ? a.proposal_id < b.proposal_id : a.proposal_rank < b.proposal_rank; }); return success(out); }
MemoryResultWith<std::vector<SchedulingConflict>> FileMemoryStore::list_conflicts(const TimestampString& start_time, const TimestampString& end_time, const std::optional<ScheduleItemId>& schedule_item_id) const { std::vector<SchedulingConflict> out; for (const auto& [_, rec] : conflicts_by_id_) { bool time_match = rec.detected_at >= start_time && rec.detected_at <= end_time; bool item_match = !schedule_item_id || rec.primary_schedule_item_id == *schedule_item_id || rec.secondary_schedule_item_id == *schedule_item_id; if (time_match && item_match) out.push_back(rec); } std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.detected_at == b.detected_at ? a.conflict_id < b.conflict_id : a.detected_at < b.detected_at; }); return success(out); }
MemoryResultWith<SchedulingProposal> FileMemoryStore::get_proposal_by_id(const ProposalId& proposal_id) const { auto it = proposals_by_id_.find(proposal_id); return it == proposals_by_id_.end() ? error_with<SchedulingProposal>("proposal not found") : success(it->second); }
MemoryResultWith<ScheduledCommitment> FileMemoryStore::get_commitment_by_id(const ScheduleItemId& commitment_id) const { auto it = commitments_by_id_.find(commitment_id); return it == commitments_by_id_.end() ? error_with<ScheduledCommitment>("commitment not found") : success(it->second); }
MemoryResultWith<SchedulingTaskCandidate> FileMemoryStore::get_task_candidate_by_id(const ScheduleItemId& task_candidate_id) const { auto it = task_candidates_by_id_.find(task_candidate_id); return it == task_candidates_by_id_.end() ? error_with<SchedulingTaskCandidate>("task candidate not found") : success(it->second); }
MemoryResultWith<SchedulingConstraintSet> FileMemoryStore::get_constraint_set_by_id(const ConstraintSetId& constraint_set_id) const { auto it = constraint_sets_by_id_.find(constraint_set_id); return it == constraint_sets_by_id_.end() ? error_with<SchedulingConstraintSet>("constraint set not found") : success(it->second); }
MemoryResult FileMemoryStore::append_behavioral_proposal(const BehavioralProposal& record) { try { auto it = behavioral_proposals_by_id_.find(record.behavioral_proposal_id); if (it != behavioral_proposals_by_id_.end() && it->second.updated_at == record.updated_at && it->second.attributes == record.attributes && it->second.title == record.title && it->second.description == record.description) return ok_result(); behavioral_proposals_by_id_[record.behavioral_proposal_id] = record; append_line(behavioral_file(data_root_, "proposals"), serialize_fields({{"behavioral_proposal_id", record.behavioral_proposal_id}, {"proposal_type", to_string(record.proposal_type)}, {"title", record.title}, {"description", record.description}, {"source_module_id", record.source_module_id}, {"related_entity_ids", serialize_list(record.related_entity_ids)}, {"priority", to_string(record.priority)}, {"estimated_behavioral_effort", std::to_string(record.estimated_behavioral_effort)}, {"expected_benefit", std::to_string(record.expected_benefit)}, {"expected_time_cost_minutes", std::to_string(record.expected_time_cost_minutes)}, {"presentation_mode", to_string(record.presentation_mode)}, {"earliest_presentation_time", record.earliest_presentation_time.value_or("")}, {"latest_relevant_time", record.latest_relevant_time.value_or("")}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}, {"attributes", serialize_map(record.attributes)}})); emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::BehavioralTriage, record.behavioral_proposal_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral proposal write completed."); emit_memory_event(EventCategory::BehavioralProposalTriaged, MemoryLayer::BehavioralTriage, record.behavioral_proposal_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral proposal persisted."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_behavioral_state_snapshot(const BehavioralStateSnapshot& record) { try { auto it = behavioral_state_snapshots_by_id_.find(record.behavioral_state_snapshot_id); if (it != behavioral_state_snapshots_by_id_.end() && it->second.captured_at == record.captured_at && it->second.attributes == record.attributes && it->second.notes == record.notes) return ok_result(); behavioral_state_snapshots_by_id_[record.behavioral_state_snapshot_id] = record; append_line(behavioral_file(data_root_, "state_snapshots"), serialize_fields({{"behavioral_state_snapshot_id", record.behavioral_state_snapshot_id}, {"captured_at", record.captured_at}, {"source_module_id", record.source_module_id}, {"active_intervention_count", std::to_string(record.active_intervention_count)}, {"backlog_count", std::to_string(record.backlog_count)}, {"schedule_density_score", std::to_string(record.schedule_density_score)}, {"recent_compliance_rate", std::to_string(record.recent_compliance_rate)}, {"recent_failure_frequency", std::to_string(record.recent_failure_frequency)}, {"fatigue_score", std::to_string(record.fatigue_score)}, {"stress_score", std::to_string(record.stress_score)}, {"behavioral_capacity_level", to_string(record.behavioral_capacity_level)}, {"psychological_state_level", to_string(record.psychological_state_level)}, {"notes", record.notes}, {"version", std::to_string(record.version)}, {"attributes", serialize_map(record.attributes)}})); emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::BehavioralTriage, record.behavioral_state_snapshot_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral state snapshot write completed."); emit_memory_event(EventCategory::BehavioralStateRecorded, MemoryLayer::BehavioralTriage, record.behavioral_state_snapshot_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral state snapshot persisted."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_behavioral_decision(const BehavioralTriageDecision& record) { try { auto it = behavioral_decisions_by_id_.find(record.behavioral_decision_id); if (it != behavioral_decisions_by_id_.end() && it->second.decided_at == record.decided_at && it->second.decision_type == record.decision_type && it->second.created_intervention_id == record.created_intervention_id) return ok_result(); behavioral_decisions_by_id_[record.behavioral_decision_id] = record; append_line(behavioral_file(data_root_, "decisions"), serialize_fields({{"behavioral_decision_id", record.behavioral_decision_id}, {"behavioral_proposal_id", record.behavioral_proposal_id}, {"decision_type", to_string(record.decision_type)}, {"behavioral_roi_score", std::to_string(record.behavioral_roi_score)}, {"capacity_gate_reason", record.capacity_gate_reason}, {"priority_reason", record.priority_reason}, {"scheduled_for", record.scheduled_for.value_or("")}, {"created_intervention_id", record.created_intervention_id.value_or("")}, {"decided_at", record.decided_at}, {"source_module_id", record.source_module_id}, {"summary", record.summary}, {"version", std::to_string(record.version)}})); emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::BehavioralTriage, record.behavioral_decision_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral decision write completed."); emit_memory_event(EventCategory::BehavioralDecisionPersisted, MemoryLayer::BehavioralTriage, record.behavioral_decision_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral decision persisted."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_behavioral_backlog_item(const BehavioralBacklogItem& record) { try { behavioral_backlog_items_by_id_[record.backlog_item_id] = record; behavioral_backlog_item_id_by_proposal_id_[record.behavioral_proposal_id] = record.backlog_item_id; append_line(behavioral_file(data_root_, "backlog"), serialize_fields({{"backlog_item_id", record.backlog_item_id}, {"behavioral_proposal_id", record.behavioral_proposal_id}, {"status", to_string(record.status)}, {"deferred_reason", record.deferred_reason}, {"first_deferred_at", record.first_deferred_at}, {"last_reconsidered_at", record.last_reconsidered_at.value_or("")}, {"reconsider_after", record.reconsider_after.value_or("")}, {"source_module_id", record.source_module_id}, {"version", std::to_string(record.version)}, {"source_proposal_id", record.source_proposal_id}, {"source_audit_run_id", record.source_audit_run_id}, {"source_activity_id", record.source_activity_id}, {"priority", record.priority}, {"effort_estimate", record.effort_estimate}, {"rationale", record.rationale}})); emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::BehavioralTriage, record.backlog_item_id, MemoryOperationType::Upsert, record.source_module_id, "Behavioral backlog write completed."); emit_memory_event(EventCategory::BehavioralBacklogUpdated, MemoryLayer::BehavioralTriage, record.backlog_item_id, MemoryOperationType::Upsert, record.source_module_id, "Behavioral backlog updated."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_behavioral_intervention(const BehavioralInterventionRecord& record) { try { auto it = behavioral_interventions_by_id_.find(record.intervention_id); if (it != behavioral_interventions_by_id_.end() && it->second.status == record.status && it->second.created_at == record.created_at) return ok_result(); behavioral_interventions_by_id_[record.intervention_id] = record; append_line(behavioral_file(data_root_, "interventions"), serialize_fields({{"intervention_id", record.intervention_id}, {"behavioral_proposal_id", record.behavioral_proposal_id}, {"behavioral_decision_id", record.behavioral_decision_id}, {"title", record.title}, {"presentation_mode", to_string(record.presentation_mode)}, {"scheduled_for", record.scheduled_for.value_or("")}, {"created_at", record.created_at}, {"status", record.status}, {"source_module_id", record.source_module_id}, {"version", std::to_string(record.version)}, {"source_proposal_id", record.source_proposal_id}, {"source_audit_run_id", record.source_audit_run_id}, {"source_activity_id", record.source_activity_id}, {"priority", record.priority}, {"effort_estimate", record.effort_estimate}, {"rationale", record.rationale}})); emit_memory_event(EventCategory::MemoryWriteCompleted, MemoryLayer::BehavioralTriage, record.intervention_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral intervention write completed."); emit_memory_event(EventCategory::BehavioralInterventionScheduled, MemoryLayer::BehavioralTriage, record.intervention_id, MemoryOperationType::Insert, record.source_module_id, "Behavioral intervention scheduled."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_activity_inventory_item(const ActivityInventoryItem& record) { try { activity_inventory_by_id_[record.activity_inventory_item_id] = record; append_line(procedural_file(data_root_, "activity_inventory"), serialize_fields({{"activity_inventory_item_id", record.activity_inventory_item_id}, {"title", record.title}, {"description", record.description}, {"domain_source", record.domain_source}, {"frequency", record.frequency}, {"duration_minutes", std::to_string(record.duration_minutes)}, {"effort_estimate", std::to_string(record.effort_estimate)}, {"outcome_value", std::to_string(record.outcome_value)}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}, {"attributes", serialize_map(record.attributes)}})); emit_memory_event(EventCategory::ProceduralInventoryUpdated, MemoryLayer::ProceduralAuditing, record.activity_inventory_item_id, MemoryOperationType::Upsert, record.source_module_id, "Procedural activity inventory updated."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::upsert_procedural_audit_run_record(const ProceduralAuditRunRecord& record) { try { procedural_audit_runs_by_id_[record.procedural_audit_run_id] = record; append_line(procedural_file(data_root_, "audit_runs"), serialize_fields({{"procedural_audit_run_id", record.procedural_audit_run_id}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}, {"activity_count", std::to_string(record.activity_count)}, {"generated_proposal_count", std::to_string(record.generated_proposal_count)}, {"status", record.status}, {"summary", record.summary}, {"attributes", serialize_map(record.attributes)}})); emit_memory_event(record.status == "Completed" ? EventCategory::ProceduralAuditCompleted : (record.status == "Failed" ? EventCategory::ProceduralAuditFailed : EventCategory::ProceduralAuditStarted), MemoryLayer::ProceduralAuditing, record.procedural_audit_run_id, MemoryOperationType::Upsert, record.source_module_id, "Procedural audit run persisted."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResult FileMemoryStore::append_behavioral_reevaluation_artifact(const BehavioralReevaluationArtifact& record) { try { auto it = behavioral_reevaluations_by_id_.find(record.behavioral_reevaluation_id); if (it != behavioral_reevaluations_by_id_.end() && it->second.reevaluated_at == record.reevaluated_at && it->second.backlog_count == record.backlog_count && it->second.intervention_count == record.intervention_count && it->second.source_state_snapshot_id == record.source_state_snapshot_id && it->second.notes_or_rationale == record.notes_or_rationale) return ok_result(); behavioral_reevaluations_by_id_[record.behavioral_reevaluation_id] = record; append_line(behavioral_file(data_root_, "reevaluations"), serialize_fields({{"behavioral_reevaluation_id", record.behavioral_reevaluation_id}, {"reevaluated_at", record.reevaluated_at}, {"source_module_id", record.source_module_id}, {"backlog_count", std::to_string(record.backlog_count)}, {"intervention_count", std::to_string(record.intervention_count)}, {"source_state_snapshot_id", record.source_state_snapshot_id}, {"notes_or_rationale", record.notes_or_rationale}, {"reevaluated_backlog_item_ids", serialize_list(record.reevaluated_backlog_item_ids)}, {"intervention_ids", serialize_list(record.intervention_ids)}, {"version", std::to_string(record.version)}})); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }

MemoryResult FileMemoryStore::upsert_optimization_proposal_record(const OptimizationProposalRecord& record) { try { optimization_proposals_by_id_[record.optimization_proposal_id] = record; append_line(procedural_file(data_root_, "optimization_proposals"), serialize_fields({{"optimization_proposal_id", record.optimization_proposal_id}, {"procedural_audit_run_id", record.procedural_audit_run_id}, {"activity_inventory_item_id", record.activity_inventory_item_id}, {"opportunity_type", to_string(record.opportunity_type)}, {"effort_value_classification", to_string(record.effort_value_classification)}, {"recovered_minutes_per_week", std::to_string(record.energy_recovery_estimate.recovered_minutes_per_week)}, {"recovered_effort_points", std::to_string(record.energy_recovery_estimate.recovered_effort_points)}, {"confidence_label", record.energy_recovery_estimate.confidence_label}, {"title", record.title}, {"rationale", record.rationale}, {"source_module_id", record.source_module_id}, {"created_at", record.created_at}, {"updated_at", record.updated_at}, {"version", std::to_string(record.version)}, {"linked_behavioral_proposal_id", record.linked_behavioral_proposal_id}, {"triage_status", record.triage_status}, {"triage_decision_id", record.triage_decision_id}, {"automation_feasibility", to_string(record.automation_feasibility)}, {"risk_tier", record.risk_tier}, {"reliability_estimate", std::to_string(record.reliability_estimate)}, {"time_recovery_minutes", std::to_string(record.time_recovery_minutes)}, {"cognitive_recovery_score", std::to_string(record.cognitive_recovery_score)}, {"stress_recovery_score", std::to_string(record.stress_recovery_score)}, {"financial_cost_estimate", std::to_string(record.financial_cost_estimate)}, {"marginal_benefit_score", std::to_string(record.marginal_benefit_score)}, {"diminishing_return_flag", record.diminishing_return_flag ? "1" : "0"}, {"source_audit_run_id", record.source_audit_run_id}, {"attributes", serialize_map(record.attributes)}})); emit_memory_event(EventCategory::ProceduralProposalGenerated, MemoryLayer::ProceduralAuditing, record.optimization_proposal_id, MemoryOperationType::Upsert, record.source_module_id, "Procedural optimization proposal persisted."); return ok_result(); } catch (const std::exception& e) { return error_result(e.what()); } }
MemoryResultWith<std::vector<ActivityInventoryItem>> FileMemoryStore::list_activity_inventory_items() const { std::vector<ActivityInventoryItem> out; for (const auto& [_, rec] : activity_inventory_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.activity_inventory_item_id < b.activity_inventory_item_id; }); return success(out); }
MemoryResultWith<ActivityInventoryItem> FileMemoryStore::get_activity_inventory_item_by_id(const ActivityInventoryItemId& activity_inventory_item_id) const { auto it = activity_inventory_by_id_.find(activity_inventory_item_id); return it == activity_inventory_by_id_.end() ? error_with<ActivityInventoryItem>("activity inventory item not found") : success(it->second); }
MemoryResultWith<std::vector<ProceduralAuditRunRecord>> FileMemoryStore::list_procedural_audit_runs() const { std::vector<ProceduralAuditRunRecord> out; for (const auto& [_, rec] : procedural_audit_runs_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.created_at == b.created_at ? a.procedural_audit_run_id < b.procedural_audit_run_id : a.created_at < b.created_at; }); return success(out); }
MemoryResultWith<ProceduralAuditRunRecord> FileMemoryStore::get_procedural_audit_run_by_id(const ProceduralAuditRunId& procedural_audit_run_id) const { auto it = procedural_audit_runs_by_id_.find(procedural_audit_run_id); return it == procedural_audit_runs_by_id_.end() ? error_with<ProceduralAuditRunRecord>("procedural audit run not found") : success(it->second); }
MemoryResultWith<std::vector<OptimizationProposalRecord>> FileMemoryStore::list_optimization_proposal_records() const { std::vector<OptimizationProposalRecord> out; for (const auto& [_, rec] : optimization_proposals_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ if (a.created_at != b.created_at) return a.created_at < b.created_at; return a.optimization_proposal_id < b.optimization_proposal_id; }); return success(out); }
MemoryResultWith<OptimizationProposalRecord> FileMemoryStore::get_optimization_proposal_record_by_id(const OptimizationProposalId& optimization_proposal_id) const { auto it = optimization_proposals_by_id_.find(optimization_proposal_id); return it == optimization_proposals_by_id_.end() ? error_with<OptimizationProposalRecord>("optimization proposal not found") : success(it->second); }
MemoryResultWith<std::vector<OptimizationProposalRecord>> FileMemoryStore::list_optimization_proposals_for_audit_run(const ProceduralAuditRunId& procedural_audit_run_id) const { std::vector<OptimizationProposalRecord> out; for (const auto& [_, rec] : optimization_proposals_by_id_) if (rec.procedural_audit_run_id == procedural_audit_run_id) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.optimization_proposal_id < b.optimization_proposal_id; }); return success(out); }
MemoryResultWith<ProceduralMemorySummary> FileMemoryStore::get_procedural_memory_summary() const { return success(ProceduralMemorySummary{activity_inventory_by_id_.size(), procedural_audit_runs_by_id_.size(), optimization_proposals_by_id_.size()}); }

MemoryResultWith<std::vector<BehavioralProposal>> FileMemoryStore::list_behavioral_proposals() const { std::vector<BehavioralProposal> out; for (const auto& [_, rec] : behavioral_proposals_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.created_at == b.created_at ? a.behavioral_proposal_id < b.behavioral_proposal_id : a.created_at < b.created_at; }); emit_memory_event(EventCategory::MemoryQueryPerformed, MemoryLayer::BehavioralTriage, "proposals", MemoryOperationType::Query, "", "Behavioral proposals queried."); return success(out); }
MemoryResultWith<std::vector<BehavioralStateSnapshot>> FileMemoryStore::list_recent_behavioral_state_snapshots(std::size_t max_records) const { std::vector<BehavioralStateSnapshot> out; for (const auto& [_, rec] : behavioral_state_snapshots_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.captured_at == b.captured_at ? a.behavioral_state_snapshot_id > b.behavioral_state_snapshot_id : a.captured_at > b.captured_at; }); if (out.size() > max_records) out.resize(max_records); return success(out); }
MemoryResultWith<std::vector<BehavioralBacklogItem>> FileMemoryStore::list_behavioral_backlog_items() const { std::vector<BehavioralBacklogItem> out; for (const auto& [_, rec] : behavioral_backlog_items_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [&](const auto& a, const auto& b){ const auto pa = behavioral_proposals_by_id_.find(a.behavioral_proposal_id); const auto pb = behavioral_proposals_by_id_.find(b.behavioral_proposal_id); const auto ra = pa == behavioral_proposals_by_id_.end() ? 0 : priority_rank(pa->second.priority); const auto rb = pb == behavioral_proposals_by_id_.end() ? 0 : priority_rank(pb->second.priority); if (a.first_deferred_at != b.first_deferred_at) return a.first_deferred_at < b.first_deferred_at; if (ra != rb) return ra > rb; return a.backlog_item_id < b.backlog_item_id; }); return success(out); }
MemoryResultWith<std::vector<BehavioralInterventionRecord>> FileMemoryStore::list_behavioral_interventions(const std::string& status_filter, const std::optional<TimestampString>& due_by) const { std::vector<BehavioralInterventionRecord> out; for (const auto& [_, rec] : behavioral_interventions_by_id_) { if (!status_filter.empty() && rec.status != status_filter) continue; if (due_by && rec.scheduled_for && *rec.scheduled_for > *due_by) continue; out.push_back(rec); } std::sort(out.begin(), out.end(), [&](const auto& a, const auto& b){ const auto sa = a.scheduled_for.value_or("9999-12-31T23:59:59.999Z"); const auto sb = b.scheduled_for.value_or("9999-12-31T23:59:59.999Z"); if (sa != sb) return sa < sb; auto pa = behavioral_proposals_by_id_.find(a.behavioral_proposal_id); auto pb = behavioral_proposals_by_id_.find(b.behavioral_proposal_id); const auto ra = pa == behavioral_proposals_by_id_.end() ? 0 : priority_rank(pa->second.priority); const auto rb = pb == behavioral_proposals_by_id_.end() ? 0 : priority_rank(pb->second.priority); if (ra != rb) return ra > rb; return a.intervention_id < b.intervention_id; }); return success(out); }
MemoryResultWith<BehavioralProposal> FileMemoryStore::get_behavioral_proposal_by_id(const BehavioralProposalId& proposal_id) const { auto it = behavioral_proposals_by_id_.find(proposal_id); return it == behavioral_proposals_by_id_.end() ? error_with<BehavioralProposal>("behavioral proposal not found") : success(it->second); }
MemoryResultWith<BehavioralTriageDecision> FileMemoryStore::get_behavioral_decision_by_id(const BehavioralDecisionId& decision_id) const { auto it = behavioral_decisions_by_id_.find(decision_id); return it == behavioral_decisions_by_id_.end() ? error_with<BehavioralTriageDecision>("behavioral decision not found") : success(it->second); }
MemoryResultWith<BehavioralBacklogItem> FileMemoryStore::get_behavioral_backlog_item_by_proposal_id(const BehavioralProposalId& proposal_id) const { auto id_it = behavioral_backlog_item_id_by_proposal_id_.find(proposal_id); if (id_it == behavioral_backlog_item_id_by_proposal_id_.end()) return error_with<BehavioralBacklogItem>("behavioral backlog not found"); auto it = behavioral_backlog_items_by_id_.find(id_it->second); return it == behavioral_backlog_items_by_id_.end() ? error_with<BehavioralBacklogItem>("behavioral backlog not found") : success(it->second); }
MemoryResultWith<std::vector<BehavioralReevaluationArtifact>> FileMemoryStore::list_behavioral_reevaluation_artifacts() const { std::vector<BehavioralReevaluationArtifact> out; for (const auto& [_, rec] : behavioral_reevaluations_by_id_) out.push_back(rec); std::sort(out.begin(), out.end(), [](const auto& a, const auto& b){ return a.reevaluated_at == b.reevaluated_at ? a.behavioral_reevaluation_id < b.behavioral_reevaluation_id : a.reevaluated_at < b.reevaluated_at; }); return success(out); }
MemoryResultWith<BehavioralMemorySummary> FileMemoryStore::get_behavioral_memory_summary() const { return success(BehavioralMemorySummary{behavioral_proposals_by_id_.size(), behavioral_state_snapshots_by_id_.size(), behavioral_decisions_by_id_.size(), behavioral_backlog_items_by_id_.size(), behavioral_interventions_by_id_.size(), behavioral_reevaluations_by_id_.size()}); }

MemoryResult FileMemoryStore::load_from_disk() {
    emit_memory_event(EventCategory::MemoryLoadStarted, MemoryLayer::LifeModelGraph, "", MemoryOperationType::Read, "", "Memory load started.");
    try {
        entities_by_id_.clear(); relationships_by_id_.clear(); preferences_by_id_.clear(); relationship_memory_by_id_.clear(); project_memory_by_id_.clear(); retrieval_index_by_id_.clear(); integration_configs_by_id_.clear(); commitments_by_id_.clear(); task_candidates_by_id_.clear(); scheduling_candidates_by_id_.clear(); windows_by_id_.clear(); constraint_sets_by_id_.clear(); proposals_by_id_.clear(); decisions_by_id_.clear(); conflicts_by_id_.clear(); behavioral_proposals_by_id_.clear(); behavioral_state_snapshots_by_id_.clear(); behavioral_decisions_by_id_.clear(); behavioral_backlog_items_by_id_.clear(); behavioral_backlog_item_id_by_proposal_id_.clear(); behavioral_interventions_by_id_.clear(); behavioral_reevaluations_by_id_.clear(); activity_inventory_by_id_.clear(); procedural_audit_runs_by_id_.clear(); optimization_proposals_by_id_.clear(); episodic_records_.clear(); behavioral_history_records_.clear();
        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::LifeModelGraph))) { auto f = parse_fields(line); if (f["record_kind"] != "life_entity") throw std::runtime_error("malformed life entity record kind"); entities_by_id_[f["entity_id"]] = {f["entity_id"], parse_entity_type(f["entity_type"]), f["display_name"], f["canonical_name"], f["description"], f["created_at"], f["updated_at"], f["source_module_id"], static_cast<MemoryVersion>(std::stoull(f["version"])), f["archived"]=="1", parse_map(f["attributes"])}; }
        for (const auto& line : read_lines(relationship_file(data_root_))) { auto f = parse_fields(line); relationships_by_id_[f["relationship_id"]] = {f["relationship_id"], f["from_entity_id"], f["to_entity_id"], parse_relationship_type(f["relationship_type"]), f["created_at"], f["updated_at"], f["source_module_id"], static_cast<MemoryVersion>(std::stoull(f["version"])), parse_map(f["attributes"])}; }
        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::EpisodicMemory))) { auto f = parse_fields(line); episodic_records_.push_back({f["record_id"], f["timestamp"], f["event_type"], f["source_module_id"], parse_list(f["associated_entity_ids"]), f["summary"], parse_map(f["details"]), static_cast<MemoryVersion>(std::stoull(f["version"]))}); }
        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::PreferenceMemory))) { auto f = parse_fields(line); preferences_by_id_[f["record_id"]] = {f["record_id"], f["preference_key"], f["value"], std::stod(f["confidence"]), f["source_module_id"], f["created_at"], f["updated_at"], static_cast<MemoryVersion>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::ProjectMemory))) { auto f = parse_fields(line); project_memory_by_id_[f["record_id"]] = {f["record_id"], f["project_entity_id"], parse_list(f["objectives"]), parse_list(f["milestones"]), parse_list(f["active_task_ids"]), parse_list(f["dependency_ids"]), f["progress_summary"], f["source_module_id"], f["created_at"], f["updated_at"], static_cast<MemoryVersion>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::BehavioralHistory))) { auto f = parse_fields(line); behavioral_history_records_.push_back({f["record_id"], f["subject_key"], f["record_type"], f["completion_state"], f["response_state"], f["score_or_value"], f["source_module_id"], f["timestamp"], static_cast<MemoryVersion>(std::stoull(f["version"]))}); }
        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::KnowledgeRetrievalIndex))) { auto f = parse_fields(line); retrieval_index_by_id_[f["record_id"]] = {f["record_id"], f["document_id"], f["source_reference"], f["indexing_status"], parse_map(f["metadata"]), f["source_module_id"], f["created_at"], f["updated_at"], static_cast<MemoryVersion>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(layer_file(data_root_, MemoryLayer::IntegrationConfiguration))) { auto f = parse_fields(line); integration_configs_by_id_[f["integration_config_id"]] = {f["integration_config_id"], f["integration_id"], f["display_name"], f["enabled"]=="1", parse_integration_status(f["status"]), parse_list(f["capability_visibility"]), parse_map(f["connection_diagnostics"]), parse_storage_mode(f["credential_storage_mode"]), f["credential_reference"], parse_map(f["non_secret_settings"]), f["created_at"], f["updated_at"], static_cast<MemoryVersion>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "commitments"))) { auto f = parse_fields(line); commitments_by_id_[f["schedule_item_id"]] = {f["schedule_item_id"], f["related_entity_id"], f["title"], f["description"], f["start_time"], f["end_time"], f["timezone"], parse_priority(f["priority"]), f["source_module_id"], f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"])), parse_schedule_status(f["status"]), parse_map(f["attributes"])}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "task_candidates"))) { auto f = parse_fields(line); task_candidates_by_id_[f["schedule_item_id"]] = {f["schedule_item_id"], f["related_entity_id"], f["title"], f["description"], std::stoi(f["estimated_duration_minutes"]), f["earliest_start"], f["latest_end"], parse_priority(f["priority"]), f["splittable"]=="1", std::stoi(f["required_buffer_before_minutes"]), std::stoi(f["required_buffer_after_minutes"]), parse_list(f["dependency_ids"]), f["source_module_id"], f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"])), parse_schedule_status(f["status"])}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "candidates"))) { auto f = parse_fields(line); scheduling_candidates_by_id_[f["candidate_id"]] = {f["candidate_id"], f["source_intervention_id"], f.contains("source_proposal_id") ? f["source_proposal_id"] : std::string{"none"}, f.contains("source_audit_run_id") ? f["source_audit_run_id"] : std::string{"none"}, f.contains("source_activity_id") ? f["source_activity_id"] : std::string{"none"}, f.contains("estimated_duration_minutes") ? std::stoi(f["estimated_duration_minutes"]) : 0, f.contains("urgency") ? f["urgency"] : std::string{"Normal"}, f.contains("scheduling_window_hint") ? f["scheduling_window_hint"] : std::string{"unspecified"}, f.contains("recommended_time_of_day") ? f["recommended_time_of_day"] : std::string{"unspecified"}, f.contains("recommended_day_span") ? f["recommended_day_span"] : std::string{"unspecified"}, f.contains("rationale") ? f["rationale"] : std::string{"none"}, parse_scheduling_candidate_status(f.contains("status") ? f["status"] : std::string{"candidate"}), f.contains("source_module_id") ? f["source_module_id"] : std::string{"coordination.scheduling"}, f.contains("created_at") ? f["created_at"] : std::string{"1970-01-01T00:00:00.000Z"}, f.contains("updated_at") ? f["updated_at"] : (f.contains("created_at") ? f["created_at"] : std::string{"1970-01-01T00:00:00.000Z"}), f.contains("version") ? static_cast<std::uint64_t>(std::stoull(f["version"])) : 1}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "proposal_artifacts"))) { auto f = parse_fields(line); schedule_proposal_artifacts_by_id_[f["schedule_proposal_id"]] = {f["schedule_proposal_id"], f.contains("source_candidate_id") ? f["source_candidate_id"] : std::string{"none"}, f.contains("source_intervention_id") ? f["source_intervention_id"] : std::string{"none"}, f.contains("source_proposal_id") ? f["source_proposal_id"] : std::string{"none"}, f.contains("source_audit_run_id") ? f["source_audit_run_id"] : std::string{"none"}, f.contains("source_activity_id") ? f["source_activity_id"] : std::string{"none"}, f.contains("proposed_start_time") ? f["proposed_start_time"] : std::string{"unspecified"}, f.contains("proposed_end_time") ? f["proposed_end_time"] : std::string{"unspecified"}, f.contains("timezone") ? f["timezone"] : std::string{"UTC"}, f.contains("duration_minutes") ? std::stoi(f["duration_minutes"]) : 0, f.contains("scheduling_window_hint") ? f["scheduling_window_hint"] : std::string{"unspecified"}, f.contains("recommended_time_of_day") ? f["recommended_time_of_day"] : std::string{"unspecified"}, f.contains("rationale") ? f["rationale"] : std::string{"none"}, parse_schedule_proposal_artifact_status(f.contains("proposal_status") ? f["proposal_status"] : std::string{"proposed"}), parse_schedule_proposal_conflict_status(f.contains("conflict_status") ? f["conflict_status"] : std::string{"none"}), f.contains("source_module_id") ? f["source_module_id"] : std::string{"coordination.scheduling"}, f.contains("created_at") ? f["created_at"] : std::string{"1970-01-01T00:00:00.000Z"}, f.contains("updated_at") ? f["updated_at"] : (f.contains("created_at") ? f["created_at"] : std::string{"1970-01-01T00:00:00.000Z"}), f.contains("version") ? static_cast<std::uint64_t>(std::stoull(f["version"])) : 1}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "availability_windows"))) { auto f = parse_fields(line); windows_by_id_[f["window_id"]] = {f["window_id"], f["title"], f["start_time"], f["end_time"], f["timezone"], f["availability_type"], f["recurrence_placeholder"], f["source_module_id"], f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "constraint_sets"))) { auto f = parse_fields(line); constraint_sets_by_id_[f["constraint_set_id"]] = {f["constraint_set_id"], std::stoi(f["max_commitments_per_day"]), std::stoi(f["minimum_gap_minutes"]), f["working_hours_only"]=="1", parse_list(f["allowed_window_ids"]), parse_list(f["blocked_window_ids"]), parse_list(f["preference_tags"]), f["source_module_id"], f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "proposals"))) { auto f = parse_fields(line); proposals_by_id_[f["proposal_id"]] = {f["proposal_id"], f["related_task_candidate_id"], f["proposed_start_time"], f["proposed_end_time"], f["timezone"], std::stoi(f["proposal_rank"]), f["rationale"], f["based_on_constraint_set_id"], f["generated_at"], f["source_module_id"], static_cast<std::uint64_t>(std::stoull(f["version"])), parse_proposal_status(f["status"])}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "decisions"))) { auto f = parse_fields(line); decisions_by_id_[f["decision_id"]] = {f["decision_id"], f["proposal_id"], f["resulting_commitment_id"], f["decision_type"], f["decided_at"], f["source_module_id"], f["summary"], static_cast<std::uint64_t>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(scheduling_file(data_root_, "conflicts"))) { auto f = parse_fields(line); conflicts_by_id_[f["conflict_id"]] = {f["conflict_id"], parse_conflict_type(f["conflict_type"]), f["primary_schedule_item_id"], f["secondary_schedule_item_id"], f["message"], f["detected_at"], f["source_module_id"], parse_map(f["fields"])}; }
        for (const auto& line : read_lines(behavioral_file(data_root_, "proposals"))) { auto f = parse_fields(line); behavioral_proposals_by_id_[f["behavioral_proposal_id"]] = {f["behavioral_proposal_id"], behavioral_proposal_type_from_string(f["proposal_type"]), f["title"], f["description"], f["source_module_id"], parse_list(f["related_entity_ids"]), behavioral_priority_from_string(f["priority"]), std::stod(f["estimated_behavioral_effort"]), std::stod(f["expected_benefit"]), std::stoi(f["expected_time_cost_minutes"]), intervention_presentation_mode_from_string(f["presentation_mode"]), f["earliest_presentation_time"].empty()?std::nullopt:std::optional<TimestampString>(f["earliest_presentation_time"]), f["latest_relevant_time"].empty()?std::nullopt:std::optional<TimestampString>(f["latest_relevant_time"]), f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"])), parse_map(f["attributes"])}; }
        for (const auto& line : read_lines(behavioral_file(data_root_, "state_snapshots"))) { auto f = parse_fields(line); behavioral_state_snapshots_by_id_[f["behavioral_state_snapshot_id"]] = {f["behavioral_state_snapshot_id"], f["captured_at"], f["source_module_id"], std::stoi(f["active_intervention_count"]), std::stoi(f["backlog_count"]), std::stod(f["schedule_density_score"]), std::stod(f["recent_compliance_rate"]), std::stod(f["recent_failure_frequency"]), std::stod(f["fatigue_score"]), std::stod(f["stress_score"]), behavioral_capacity_level_from_string(f["behavioral_capacity_level"]), psychological_state_level_from_string(f["psychological_state_level"]), f["notes"], static_cast<std::uint64_t>(std::stoull(f["version"])), f.contains("attributes") ? parse_map(f["attributes"]) : StringMap{}}; }
        for (const auto& line : read_lines(behavioral_file(data_root_, "decisions"))) { auto f = parse_fields(line); behavioral_decisions_by_id_[f["behavioral_decision_id"]] = {f["behavioral_decision_id"], f["behavioral_proposal_id"], behavioral_decision_type_from_string(f["decision_type"]), std::stod(f["behavioral_roi_score"]), f["capacity_gate_reason"], f["priority_reason"], f["scheduled_for"].empty()?std::nullopt:std::optional<TimestampString>(f["scheduled_for"]), f["created_intervention_id"].empty()?std::nullopt:std::optional<InterventionId>(f["created_intervention_id"]), f["decided_at"], f["source_module_id"], f["summary"], static_cast<std::uint64_t>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(behavioral_file(data_root_, "backlog"))) { auto f = parse_fields(line); behavioral_backlog_items_by_id_[f["backlog_item_id"]] = {f["backlog_item_id"], f["behavioral_proposal_id"], backlog_status_from_string(f["status"]), f["deferred_reason"], f["first_deferred_at"], f["last_reconsidered_at"].empty()?std::nullopt:std::optional<TimestampString>(f["last_reconsidered_at"]), f["reconsider_after"].empty()?std::nullopt:std::optional<TimestampString>(f["reconsider_after"]), f["source_module_id"], static_cast<std::uint64_t>(std::stoull(f["version"])), f.contains("source_proposal_id") ? f["source_proposal_id"] : std::string{"none"}, f.contains("source_audit_run_id") ? f["source_audit_run_id"] : std::string{"none"}, f.contains("source_activity_id") ? f["source_activity_id"] : std::string{"none"}, f.contains("priority") ? f["priority"] : std::string{"Normal"}, f.contains("effort_estimate") ? f["effort_estimate"] : std::string{"0"}, f.contains("rationale") ? f["rationale"] : std::string{"none"}}; behavioral_backlog_item_id_by_proposal_id_[f["behavioral_proposal_id"]] = f["backlog_item_id"]; }
        for (const auto& line : read_lines(behavioral_file(data_root_, "interventions"))) { auto f = parse_fields(line); behavioral_interventions_by_id_[f["intervention_id"]] = {f["intervention_id"], f["behavioral_proposal_id"], f["behavioral_decision_id"], f["title"], intervention_presentation_mode_from_string(f["presentation_mode"]), f["scheduled_for"].empty()?std::nullopt:std::optional<TimestampString>(f["scheduled_for"]), f["created_at"], f["status"], f["source_module_id"], static_cast<std::uint64_t>(std::stoull(f["version"])), f.contains("source_proposal_id") ? f["source_proposal_id"] : std::string{"none"}, f.contains("source_audit_run_id") ? f["source_audit_run_id"] : std::string{"none"}, f.contains("source_activity_id") ? f["source_activity_id"] : std::string{"none"}, f.contains("priority") ? f["priority"] : std::string{"Normal"}, f.contains("effort_estimate") ? f["effort_estimate"] : std::string{"0"}, f.contains("rationale") ? f["rationale"] : std::string{"none"}}; }
        for (const auto& line : read_lines(behavioral_file(data_root_, "reevaluations"))) { auto f = parse_fields(line); behavioral_reevaluations_by_id_[f["behavioral_reevaluation_id"]] = {f["behavioral_reevaluation_id"], f["reevaluated_at"], f["source_module_id"], static_cast<std::size_t>(std::stoull(f["backlog_count"])), static_cast<std::size_t>(std::stoull(f["intervention_count"])), f.contains("source_state_snapshot_id") ? f["source_state_snapshot_id"] : std::string{"none"}, f.contains("notes_or_rationale") ? f["notes_or_rationale"] : std::string{"none"}, parse_list(f["reevaluated_backlog_item_ids"]), parse_list(f["intervention_ids"]), static_cast<std::uint64_t>(std::stoull(f["version"]))}; }
        for (const auto& line : read_lines(procedural_file(data_root_, "activity_inventory"))) { auto f = parse_fields(line); activity_inventory_by_id_[f["activity_inventory_item_id"]] = {f["activity_inventory_item_id"], f["title"], f["description"], f["domain_source"], f["frequency"], std::stoi(f["duration_minutes"]), std::stoi(f["effort_estimate"]), std::stoi(f["outcome_value"]), f["source_module_id"], f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"])), parse_map(f["attributes"])}; }
        for (const auto& line : read_lines(procedural_file(data_root_, "audit_runs"))) { auto f = parse_fields(line); procedural_audit_runs_by_id_[f["procedural_audit_run_id"]] = {f["procedural_audit_run_id"], f["source_module_id"], f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"])), static_cast<std::size_t>(std::stoull(f["activity_count"])), static_cast<std::size_t>(std::stoull(f["generated_proposal_count"])), f["status"], f["summary"], parse_map(f["attributes"])}; }
        for (const auto& line : read_lines(procedural_file(data_root_, "optimization_proposals"))) { auto f = parse_fields(line); optimization_proposals_by_id_[f["optimization_proposal_id"]] = {f["optimization_proposal_id"], f["procedural_audit_run_id"], f["activity_inventory_item_id"], optimization_opportunity_type_from_string(f["opportunity_type"]), effort_value_classification_from_string(f["effort_value_classification"]), {std::stoi(f["recovered_minutes_per_week"]), std::stoi(f["recovered_effort_points"]), f["confidence_label"]}, f["title"], f["rationale"], f["source_module_id"], f["created_at"], f["updated_at"], static_cast<std::uint64_t>(std::stoull(f["version"])), f.contains("linked_behavioral_proposal_id") ? f["linked_behavioral_proposal_id"] : std::string{}, f.contains("triage_status") ? f["triage_status"] : std::string{}, f.contains("triage_decision_id") ? f["triage_decision_id"] : std::string{}, automation_feasibility_from_string(f.contains("automation_feasibility") ? f["automation_feasibility"] : std::string{"NotApplicable"}), f.contains("risk_tier") ? f["risk_tier"] : std::string{"Unknown"}, f.contains("reliability_estimate") ? std::stod(f["reliability_estimate"]) : 0.0, f.contains("time_recovery_minutes") ? std::stoi(f["time_recovery_minutes"]) : (f.contains("recovered_minutes_per_week") ? std::stoi(f["recovered_minutes_per_week"]) : 0), f.contains("cognitive_recovery_score") ? std::stoi(f["cognitive_recovery_score"]) : 0, f.contains("stress_recovery_score") ? std::stoi(f["stress_recovery_score"]) : 0, f.contains("financial_cost_estimate") ? std::stoi(f["financial_cost_estimate"]) : 0, f.contains("marginal_benefit_score") ? std::stoi(f["marginal_benefit_score"]) : 0, f.contains("diminishing_return_flag") && f["diminishing_return_flag"] == "1", f.contains("source_audit_run_id") ? f["source_audit_run_id"] : f["procedural_audit_run_id"], parse_map(f["attributes"])}; }
        snapshot_version_ = entities_by_id_.size() + relationships_by_id_.size() + commitments_by_id_.size() + behavioral_proposals_by_id_.size() + behavioral_state_snapshots_by_id_.size() + behavioral_reevaluations_by_id_.size() + activity_inventory_by_id_.size() + optimization_proposals_by_id_.size();
        emit_memory_event(EventCategory::MemoryLoadCompleted, MemoryLayer::LifeModelGraph, "", MemoryOperationType::Read, "", "Memory load completed.");
        return ok_result();
    } catch (const std::exception& e) { emit_memory_event(EventCategory::MemoryLoadFailed, MemoryLayer::LifeModelGraph, "", MemoryOperationType::Read, "", e.what()); return error_result(e.what()); }
}

MemoryResult FileMemoryStore::persist_to_disk() {
    ensure_parent(manifest_file(data_root_));
    std::ofstream out(manifest_file(data_root_));
    if (!out.is_open()) return error_result("unable to write manifest");
    out << "{\n  \"schema_version\": \"sprint3-v1\",\n  \"snapshot_version\": " << snapshot_version_ << ",\n  \"updated_at\": \"" << current_timestamp_utc() << "\"\n}\n";
    return static_cast<bool>(out) ? ok_result() : error_result("manifest write failed");
}

std::string to_string(MemoryLayer value) { switch (value) { case MemoryLayer::LifeModelGraph: return "LifeModelGraph"; case MemoryLayer::EpisodicMemory: return "EpisodicMemory"; case MemoryLayer::PreferenceMemory: return "PreferenceMemory"; case MemoryLayer::RelationshipMemory: return "RelationshipMemory"; case MemoryLayer::ProjectMemory: return "ProjectMemory"; case MemoryLayer::BehavioralHistory: return "BehavioralHistory"; case MemoryLayer::KnowledgeRetrievalIndex: return "KnowledgeRetrievalIndex"; case MemoryLayer::IntegrationConfiguration: return "IntegrationConfiguration"; case MemoryLayer::Scheduling: return "Scheduling"; case MemoryLayer::BehavioralTriage: return "BehavioralTriage"; case MemoryLayer::ProceduralAuditing: return "ProceduralAuditing"; } return "Unknown"; }
std::string to_string(EntityType value) { switch (value) { case EntityType::Goal: return "Goal"; case EntityType::Commitment: return "Commitment"; case EntityType::Relationship: return "Relationship"; case EntityType::Project: return "Project"; case EntityType::Environment: return "Environment"; case EntityType::Location: return "Location"; case EntityType::Domain: return "Domain"; case EntityType::Task: return "Task"; case EntityType::Habit: return "Habit"; case EntityType::Person: return "Person"; case EntityType::Preference: return "Preference"; case EntityType::Integration: return "Integration"; } return "Unknown"; }
std::string to_string(RelationshipType value) { switch (value) { case RelationshipType::Supports: return "Supports"; case RelationshipType::Contains: return "Contains"; case RelationshipType::AssociatedWith: return "AssociatedWith"; case RelationshipType::ScheduledIn: return "ScheduledIn"; case RelationshipType::DependsOn: return "DependsOn"; case RelationshipType::RelatedTo: return "RelatedTo"; case RelationshipType::OwnedBy: return "OwnedBy"; case RelationshipType::ConfiguredBy: return "ConfiguredBy"; } return "Unknown"; }
std::string to_string(MemoryOperationType value) { switch (value) { case MemoryOperationType::Insert: return "Insert"; case MemoryOperationType::Update: return "Update"; case MemoryOperationType::Upsert: return "Upsert"; case MemoryOperationType::Delete: return "Delete"; case MemoryOperationType::Read: return "Read"; case MemoryOperationType::Query: return "Query"; } return "Unknown"; }
std::string to_string(IntegrationStatus value) { switch (value) { case IntegrationStatus::Disabled: return "Disabled"; case IntegrationStatus::Enabled: return "Enabled"; case IntegrationStatus::Error: return "Error"; case IntegrationStatus::Unknown: return "Unknown"; } return "Unknown"; }
std::string to_string(CredentialStorageMode value) { switch (value) { case CredentialStorageMode::InlinePlaceholderOnly: return "InlinePlaceholderOnly"; case CredentialStorageMode::ExternalSecretReference: return "ExternalSecretReference"; case CredentialStorageMode::Unset: return "Unset"; } return "Unknown"; }

}  // namespace life_orchestrator::core
