#pragma once

#include "control_plane/event_logger.hpp"
#include "core/contracts.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace life_orchestrator::core {

using EntityId = std::string;
using RecordId = std::string;
using RelationshipId = std::string;
using MemoryVersion = std::uint64_t;
using SourceModuleId = std::string;
using QueryToken = std::string;
using IntegrationId = std::string;
using IntegrationConfigId = std::string;

using StringMap = std::unordered_map<std::string, std::string>;

struct MemoryResult {
    bool ok;
    std::string message;
};

template <typename T>
struct MemoryResultWith {
    bool ok;
    std::string message;
    std::optional<T> value;
};

enum class MemoryLayer {
    LifeModelGraph,
    EpisodicMemory,
    PreferenceMemory,
    RelationshipMemory,
    ProjectMemory,
    BehavioralHistory,
    KnowledgeRetrievalIndex,
    IntegrationConfiguration
};

enum class EntityType {
    Goal,
    Commitment,
    Relationship,
    Project,
    Environment,
    Location,
    Domain,
    Task,
    Habit,
    Person,
    Preference,
    Integration
};

enum class RelationshipType {
    Supports,
    Contains,
    AssociatedWith,
    ScheduledIn,
    DependsOn,
    RelatedTo,
    OwnedBy,
    ConfiguredBy
};

enum class MemoryOperationType { Insert, Update, Upsert, Delete, Read, Query };

enum class IntegrationStatus { Disabled, Enabled, Error, Unknown };

enum class CredentialStorageMode { InlinePlaceholderOnly, ExternalSecretReference, Unset };

struct LifeEntity {
    EntityId entity_id;
    EntityType entity_type;
    std::string display_name;
    std::string canonical_name;
    std::string description;
    TimestampString created_at;
    TimestampString updated_at;
    SourceModuleId source_module_id;
    MemoryVersion version;
    bool archived;
    StringMap attributes;
};

struct LifeRelationship {
    RelationshipId relationship_id;
    EntityId from_entity_id;
    EntityId to_entity_id;
    RelationshipType relationship_type;
    TimestampString created_at;
    TimestampString updated_at;
    SourceModuleId source_module_id;
    MemoryVersion version;
    StringMap attributes;
};

struct LifeGraphSnapshotMetadata {
    MemoryVersion snapshot_version;
    TimestampString created_at;
    std::string schema_version;
    std::size_t entity_count;
    std::size_t relationship_count;
};

struct EpisodicMemoryRecord {
    RecordId record_id;
    TimestampString timestamp;
    std::string event_type;
    SourceModuleId source_module_id;
    std::vector<EntityId> associated_entity_ids;
    std::string summary;
    StringMap details;
    MemoryVersion version;
};

struct PreferenceRecord {
    RecordId record_id;
    std::string preference_key;
    std::string value;
    double confidence;
    SourceModuleId source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    MemoryVersion version;
};

struct RelationshipMemoryRecord {
    RecordId record_id;
    EntityId related_person_entity_id;
    std::string communication_cadence;
    std::vector<std::string> important_dates;
    std::vector<std::string> shared_interests;
    std::string notes;
    SourceModuleId source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    MemoryVersion version;
};

struct ProjectMemoryRecord {
    RecordId record_id;
    EntityId project_entity_id;
    std::vector<std::string> objectives;
    std::vector<std::string> milestones;
    std::vector<EntityId> active_task_ids;
    std::vector<EntityId> dependency_ids;
    std::string progress_summary;
    SourceModuleId source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    MemoryVersion version;
};

struct BehavioralHistoryRecord {
    RecordId record_id;
    std::string subject_key;
    std::string record_type;
    std::string completion_state;
    std::string response_state;
    std::string score_or_value;
    SourceModuleId source_module_id;
    TimestampString timestamp;
    MemoryVersion version;
};

struct KnowledgeRetrievalIndexRecord {
    RecordId record_id;
    std::string document_id;
    std::string source_reference;
    std::string indexing_status;
    StringMap metadata;
    SourceModuleId source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    MemoryVersion version;
};

struct IntegrationConfigurationRecord {
    IntegrationConfigId integration_config_id;
    IntegrationId integration_id;
    std::string display_name;
    bool enabled;
    IntegrationStatus status;
    std::vector<std::string> capability_visibility;
    StringMap connection_diagnostics;
    CredentialStorageMode credential_storage_mode;
    std::string credential_reference;
    StringMap non_secret_settings;
    TimestampString created_at;
    TimestampString updated_at;
    MemoryVersion version;
};

struct MemorySummary {
    std::size_t entity_count;
    std::size_t relationship_count;
    std::size_t episodic_count;
    std::size_t preference_count;
    std::size_t relationship_memory_count;
    std::size_t project_memory_count;
    std::size_t behavioral_history_count;
    std::size_t retrieval_index_count;
    std::size_t integration_configuration_count;
};

struct MemoryRecordView {
    MemoryLayer layer;
    RecordId record_id;
    StringMap fields;
};

class IMemoryStore {
public:
    virtual ~IMemoryStore() = default;

    virtual MemoryResult upsert_life_entity(const LifeEntity& entity) = 0;
    virtual MemoryResult upsert_life_relationship(const LifeRelationship& relationship) = 0;
    virtual MemoryResult append_episodic_record(const EpisodicMemoryRecord& record) = 0;
    virtual MemoryResult upsert_preference_record(const PreferenceRecord& record) = 0;
    virtual MemoryResult upsert_relationship_memory_record(const RelationshipMemoryRecord& record) = 0;
    virtual MemoryResult upsert_project_memory_record(const ProjectMemoryRecord& record) = 0;
    virtual MemoryResult append_behavioral_history_record(const BehavioralHistoryRecord& record) = 0;
    virtual MemoryResult upsert_retrieval_index_record(const KnowledgeRetrievalIndexRecord& record) = 0;

    virtual MemoryResultWith<LifeEntity> get_entity_by_id(const EntityId& entity_id) const = 0;
    virtual MemoryResultWith<std::vector<LifeEntity>> list_entities_by_type(EntityType type) const = 0;
    virtual MemoryResultWith<std::vector<LifeRelationship>> get_relationships_for_entity(
        const EntityId& entity_id) const = 0;
    virtual MemoryResultWith<ProjectMemoryRecord> get_project_record_by_project_entity_id(
        const EntityId& project_entity_id) const = 0;
    virtual MemoryResultWith<std::vector<PreferenceRecord>> get_preferences_by_prefix_or_key(
        const std::string& key_or_prefix,
        bool exact_match) const = 0;
    virtual MemoryResultWith<std::vector<EpisodicMemoryRecord>> list_recent_episodic_records(
        std::size_t max_records) const = 0;
    virtual MemoryResultWith<std::vector<BehavioralHistoryRecord>> list_behavioral_history_for_subject(
        const std::string& subject_key) const = 0;
    virtual MemoryResultWith<MemoryRecordView> read_record_by_layer_and_id(MemoryLayer layer,
                                                                            const RecordId& id) const = 0;
    virtual MemoryResultWith<std::vector<MemoryRecordView>> list_records_for_query(
        MemoryLayer layer,
        const QueryToken& token) const = 0;
    virtual MemoryResultWith<LifeGraphSnapshotMetadata> get_life_graph_snapshot_metadata() const = 0;
    virtual MemoryResultWith<MemorySummary> get_memory_summary() const = 0;

    virtual MemoryResult load_from_disk() = 0;
    virtual MemoryResult persist_to_disk() = 0;
};

class FileMemoryStore : public IMemoryStore {
public:
    FileMemoryStore(std::filesystem::path data_root, control_plane::EventLogger* event_logger = nullptr);

    MemoryResult upsert_life_entity(const LifeEntity& entity) override;
    MemoryResult upsert_life_relationship(const LifeRelationship& relationship) override;
    MemoryResult append_episodic_record(const EpisodicMemoryRecord& record) override;
    MemoryResult upsert_preference_record(const PreferenceRecord& record) override;
    MemoryResult upsert_relationship_memory_record(const RelationshipMemoryRecord& record) override;
    MemoryResult upsert_project_memory_record(const ProjectMemoryRecord& record) override;
    MemoryResult append_behavioral_history_record(const BehavioralHistoryRecord& record) override;
    MemoryResult upsert_retrieval_index_record(const KnowledgeRetrievalIndexRecord& record) override;

    MemoryResultWith<LifeEntity> get_entity_by_id(const EntityId& entity_id) const override;
    MemoryResultWith<std::vector<LifeEntity>> list_entities_by_type(EntityType type) const override;
    MemoryResultWith<std::vector<LifeRelationship>> get_relationships_for_entity(
        const EntityId& entity_id) const override;
    MemoryResultWith<ProjectMemoryRecord> get_project_record_by_project_entity_id(
        const EntityId& project_entity_id) const override;
    MemoryResultWith<std::vector<PreferenceRecord>> get_preferences_by_prefix_or_key(
        const std::string& key_or_prefix,
        bool exact_match) const override;
    MemoryResultWith<std::vector<EpisodicMemoryRecord>> list_recent_episodic_records(
        std::size_t max_records) const override;
    MemoryResultWith<std::vector<BehavioralHistoryRecord>> list_behavioral_history_for_subject(
        const std::string& subject_key) const override;
    MemoryResultWith<MemoryRecordView> read_record_by_layer_and_id(MemoryLayer layer,
                                                                    const RecordId& id) const override;
    MemoryResultWith<std::vector<MemoryRecordView>> list_records_for_query(
        MemoryLayer layer,
        const QueryToken& token) const override;
    MemoryResultWith<LifeGraphSnapshotMetadata> get_life_graph_snapshot_metadata() const override;
    MemoryResultWith<MemorySummary> get_memory_summary() const override;

    MemoryResult load_from_disk() override;
    MemoryResult persist_to_disk() override;

private:
    std::filesystem::path data_root_;
    control_plane::EventLogger* event_logger_;

    std::unordered_map<EntityId, LifeEntity> entities_by_id_;
    std::unordered_map<RelationshipId, LifeRelationship> relationships_by_id_;
    std::unordered_map<RecordId, PreferenceRecord> preferences_by_id_;
    std::unordered_map<RecordId, RelationshipMemoryRecord> relationship_memory_by_id_;
    std::unordered_map<RecordId, ProjectMemoryRecord> project_memory_by_id_;
    std::unordered_map<RecordId, KnowledgeRetrievalIndexRecord> retrieval_index_by_id_;
    std::unordered_map<IntegrationConfigId, IntegrationConfigurationRecord> integration_configs_by_id_;
    std::vector<EpisodicMemoryRecord> episodic_records_;
    std::vector<BehavioralHistoryRecord> behavioral_history_records_;

    MemoryVersion snapshot_version_;

    void emit_memory_event(EventCategory category,
                           MemoryLayer layer,
                           const std::string& record_id,
                           MemoryOperationType operation,
                           const SourceModuleId& source_module_id,
                           const std::string& message) const;
};

std::string to_string(MemoryLayer value);
std::string to_string(EntityType value);
std::string to_string(RelationshipType value);
std::string to_string(MemoryOperationType value);
std::string to_string(IntegrationStatus value);
std::string to_string(CredentialStorageMode value);

}  // namespace life_orchestrator::core
