#pragma once

#include "control_plane/event_logger.hpp"
#include "core/behavioral.hpp"
#include "core/contracts.hpp"
#include "core/procedural.hpp"

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
    IntegrationConfiguration,
    Scheduling,
    BehavioralTriage,
    ProceduralAuditing
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
    std::size_t scheduling_commitment_count;
    std::size_t scheduling_task_candidate_count;
    std::size_t scheduling_window_count;
    std::size_t scheduling_constraint_count;
    std::size_t scheduling_proposal_count;
    std::size_t scheduling_decision_count;
    std::size_t scheduling_conflict_count;
    std::size_t behavioral_proposal_count;
    std::size_t behavioral_state_snapshot_count;
    std::size_t behavioral_decision_count;
    std::size_t behavioral_backlog_count;
    std::size_t behavioral_intervention_count;
    std::size_t activity_inventory_count;
    std::size_t procedural_audit_run_count;
    std::size_t optimization_proposal_count;
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

    virtual MemoryResult upsert_scheduled_commitment(const ScheduledCommitment& record) = 0;
    virtual MemoryResult append_task_candidate(const SchedulingTaskCandidate& record) = 0;
    virtual MemoryResult upsert_availability_window(const AvailabilityWindow& record) = 0;
    virtual MemoryResult upsert_constraint_set(const SchedulingConstraintSet& record) = 0;
    virtual MemoryResult append_proposal(const SchedulingProposal& record) = 0;
    virtual MemoryResult append_decision(const SchedulingDecisionRecord& record) = 0;
    virtual MemoryResult append_conflict(const SchedulingConflict& record) = 0;
    virtual MemoryResult append_behavioral_proposal(const BehavioralProposal& record) = 0;
    virtual MemoryResult append_behavioral_state_snapshot(const BehavioralStateSnapshot& record) = 0;
    virtual MemoryResult append_behavioral_decision(const BehavioralTriageDecision& record) = 0;
    virtual MemoryResult upsert_behavioral_backlog_item(const BehavioralBacklogItem& record) = 0;
    virtual MemoryResult append_behavioral_intervention(const BehavioralInterventionRecord& record) = 0;
    virtual MemoryResult append_behavioral_reevaluation_artifact(const BehavioralReevaluationArtifact& record) = 0;
    virtual MemoryResult upsert_activity_inventory_item(const ActivityInventoryItem& record) = 0;
    virtual MemoryResult upsert_procedural_audit_run_record(const ProceduralAuditRunRecord& record) = 0;
    virtual MemoryResult upsert_optimization_proposal_record(const OptimizationProposalRecord& record) = 0;


    virtual MemoryResultWith<LifeEntity> get_entity_by_id(const EntityId& entity_id) const = 0;
    virtual MemoryResultWith<std::vector<LifeEntity>> list_entities_by_type(EntityType type) const = 0;
    virtual MemoryResultWith<std::vector<LifeRelationship>> get_relationships_for_entity(const EntityId& entity_id) const = 0;
    virtual MemoryResultWith<ProjectMemoryRecord> get_project_record_by_project_entity_id(const EntityId& project_entity_id) const = 0;
    virtual MemoryResultWith<std::vector<PreferenceRecord>> get_preferences_by_prefix_or_key(const std::string& key_or_prefix, bool exact_match) const = 0;
    virtual MemoryResultWith<std::vector<EpisodicMemoryRecord>> list_recent_episodic_records(std::size_t max_records) const = 0;
    virtual MemoryResultWith<std::vector<BehavioralHistoryRecord>> list_behavioral_history_for_subject(const std::string& subject_key) const = 0;
    virtual MemoryResultWith<MemoryRecordView> read_record_by_layer_and_id(MemoryLayer layer, const RecordId& id) const = 0;
    virtual MemoryResultWith<std::vector<MemoryRecordView>> list_records_for_query(MemoryLayer layer, const QueryToken& token) const = 0;
    virtual MemoryResultWith<LifeGraphSnapshotMetadata> get_life_graph_snapshot_metadata() const = 0;
    virtual MemoryResultWith<MemorySummary> get_memory_summary() const = 0;

    virtual MemoryResultWith<std::vector<ScheduledCommitment>> list_commitments_in_window(const TimestampString& start_time, const TimestampString& end_time) const = 0;
    virtual MemoryResultWith<std::vector<SchedulingTaskCandidate>> list_task_candidates_by_status_and_range(ScheduleStatus status, const TimestampString& start_time, const TimestampString& end_time) const = 0;
    virtual MemoryResultWith<std::vector<AvailabilityWindow>> list_availability_windows_in_window(const TimestampString& start_time, const TimestampString& end_time) const = 0;
    virtual MemoryResultWith<std::vector<SchedulingProposal>> list_proposals_for_task_candidate(const ScheduleItemId& task_candidate_id) const = 0;
    virtual MemoryResultWith<std::vector<SchedulingConflict>> list_conflicts(const TimestampString& start_time, const TimestampString& end_time, const std::optional<ScheduleItemId>& schedule_item_id) const = 0;
    virtual MemoryResultWith<SchedulingProposal> get_proposal_by_id(const ProposalId& proposal_id) const = 0;
    virtual MemoryResultWith<ScheduledCommitment> get_commitment_by_id(const ScheduleItemId& commitment_id) const = 0;
    virtual MemoryResultWith<SchedulingTaskCandidate> get_task_candidate_by_id(const ScheduleItemId& task_candidate_id) const = 0;
    virtual MemoryResultWith<SchedulingConstraintSet> get_constraint_set_by_id(const ConstraintSetId& constraint_set_id) const = 0;
    virtual MemoryResultWith<std::vector<BehavioralProposal>> list_behavioral_proposals() const = 0;
    virtual MemoryResultWith<std::vector<BehavioralStateSnapshot>> list_recent_behavioral_state_snapshots(std::size_t max_records) const = 0;
    virtual MemoryResultWith<std::vector<BehavioralBacklogItem>> list_behavioral_backlog_items() const = 0;
    virtual MemoryResultWith<std::vector<BehavioralInterventionRecord>> list_behavioral_interventions(const std::string& status_filter, const std::optional<TimestampString>& due_by) const = 0;
    virtual MemoryResultWith<BehavioralProposal> get_behavioral_proposal_by_id(const BehavioralProposalId& proposal_id) const = 0;
    virtual MemoryResultWith<BehavioralTriageDecision> get_behavioral_decision_by_id(const BehavioralDecisionId& decision_id) const = 0;
    virtual MemoryResultWith<BehavioralBacklogItem> get_behavioral_backlog_item_by_proposal_id(const BehavioralProposalId& proposal_id) const = 0;
    virtual MemoryResultWith<std::vector<BehavioralReevaluationArtifact>> list_behavioral_reevaluation_artifacts() const = 0;
    virtual MemoryResultWith<BehavioralMemorySummary> get_behavioral_memory_summary() const = 0;
    virtual MemoryResultWith<std::vector<ActivityInventoryItem>> list_activity_inventory_items() const = 0;
    virtual MemoryResultWith<ActivityInventoryItem> get_activity_inventory_item_by_id(const ActivityInventoryItemId& activity_inventory_item_id) const = 0;
    virtual MemoryResultWith<std::vector<ProceduralAuditRunRecord>> list_procedural_audit_runs() const = 0;
    virtual MemoryResultWith<ProceduralAuditRunRecord> get_procedural_audit_run_by_id(const ProceduralAuditRunId& procedural_audit_run_id) const = 0;
    virtual MemoryResultWith<std::vector<OptimizationProposalRecord>> list_optimization_proposal_records() const = 0;
    virtual MemoryResultWith<OptimizationProposalRecord> get_optimization_proposal_record_by_id(const OptimizationProposalId& optimization_proposal_id) const = 0;
    virtual MemoryResultWith<std::vector<OptimizationProposalRecord>> list_optimization_proposals_for_audit_run(const ProceduralAuditRunId& procedural_audit_run_id) const = 0;
    virtual MemoryResultWith<ProceduralMemorySummary> get_procedural_memory_summary() const = 0;

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

    MemoryResult upsert_scheduled_commitment(const ScheduledCommitment& record) override;
    MemoryResult append_task_candidate(const SchedulingTaskCandidate& record) override;
    MemoryResult upsert_availability_window(const AvailabilityWindow& record) override;
    MemoryResult upsert_constraint_set(const SchedulingConstraintSet& record) override;
    MemoryResult append_proposal(const SchedulingProposal& record) override;
    MemoryResult append_decision(const SchedulingDecisionRecord& record) override;
    MemoryResult append_conflict(const SchedulingConflict& record) override;
    MemoryResult append_behavioral_proposal(const BehavioralProposal& record) override;
    MemoryResult append_behavioral_state_snapshot(const BehavioralStateSnapshot& record) override;
    MemoryResult append_behavioral_decision(const BehavioralTriageDecision& record) override;
    MemoryResult upsert_behavioral_backlog_item(const BehavioralBacklogItem& record) override;
    MemoryResult append_behavioral_intervention(const BehavioralInterventionRecord& record) override;
    MemoryResult append_behavioral_reevaluation_artifact(const BehavioralReevaluationArtifact& record) override;
    MemoryResult upsert_activity_inventory_item(const ActivityInventoryItem& record) override;
    MemoryResult upsert_procedural_audit_run_record(const ProceduralAuditRunRecord& record) override;
    MemoryResult upsert_optimization_proposal_record(const OptimizationProposalRecord& record) override;

    MemoryResultWith<LifeEntity> get_entity_by_id(const EntityId& entity_id) const override;
    MemoryResultWith<std::vector<LifeEntity>> list_entities_by_type(EntityType type) const override;
    MemoryResultWith<std::vector<LifeRelationship>> get_relationships_for_entity(const EntityId& entity_id) const override;
    MemoryResultWith<ProjectMemoryRecord> get_project_record_by_project_entity_id(const EntityId& project_entity_id) const override;
    MemoryResultWith<std::vector<PreferenceRecord>> get_preferences_by_prefix_or_key(const std::string& key_or_prefix, bool exact_match) const override;
    MemoryResultWith<std::vector<EpisodicMemoryRecord>> list_recent_episodic_records(std::size_t max_records) const override;
    MemoryResultWith<std::vector<BehavioralHistoryRecord>> list_behavioral_history_for_subject(const std::string& subject_key) const override;
    MemoryResultWith<MemoryRecordView> read_record_by_layer_and_id(MemoryLayer layer, const RecordId& id) const override;
    MemoryResultWith<std::vector<MemoryRecordView>> list_records_for_query(MemoryLayer layer, const QueryToken& token) const override;
    MemoryResultWith<LifeGraphSnapshotMetadata> get_life_graph_snapshot_metadata() const override;
    MemoryResultWith<MemorySummary> get_memory_summary() const override;

    MemoryResultWith<std::vector<ScheduledCommitment>> list_commitments_in_window(const TimestampString& start_time, const TimestampString& end_time) const override;
    MemoryResultWith<std::vector<SchedulingTaskCandidate>> list_task_candidates_by_status_and_range(ScheduleStatus status, const TimestampString& start_time, const TimestampString& end_time) const override;
    MemoryResultWith<std::vector<AvailabilityWindow>> list_availability_windows_in_window(const TimestampString& start_time, const TimestampString& end_time) const override;
    MemoryResultWith<std::vector<SchedulingProposal>> list_proposals_for_task_candidate(const ScheduleItemId& task_candidate_id) const override;
    MemoryResultWith<std::vector<SchedulingConflict>> list_conflicts(const TimestampString& start_time, const TimestampString& end_time, const std::optional<ScheduleItemId>& schedule_item_id) const override;
    MemoryResultWith<SchedulingProposal> get_proposal_by_id(const ProposalId& proposal_id) const override;
    MemoryResultWith<ScheduledCommitment> get_commitment_by_id(const ScheduleItemId& commitment_id) const override;
    MemoryResultWith<SchedulingTaskCandidate> get_task_candidate_by_id(const ScheduleItemId& task_candidate_id) const override;
    MemoryResultWith<SchedulingConstraintSet> get_constraint_set_by_id(const ConstraintSetId& constraint_set_id) const override;
    MemoryResultWith<std::vector<BehavioralProposal>> list_behavioral_proposals() const override;
    MemoryResultWith<std::vector<BehavioralStateSnapshot>> list_recent_behavioral_state_snapshots(std::size_t max_records) const override;
    MemoryResultWith<std::vector<BehavioralBacklogItem>> list_behavioral_backlog_items() const override;
    MemoryResultWith<std::vector<BehavioralInterventionRecord>> list_behavioral_interventions(const std::string& status_filter, const std::optional<TimestampString>& due_by) const override;
    MemoryResultWith<BehavioralProposal> get_behavioral_proposal_by_id(const BehavioralProposalId& proposal_id) const override;
    MemoryResultWith<BehavioralTriageDecision> get_behavioral_decision_by_id(const BehavioralDecisionId& decision_id) const override;
    MemoryResultWith<BehavioralBacklogItem> get_behavioral_backlog_item_by_proposal_id(const BehavioralProposalId& proposal_id) const override;
    MemoryResultWith<std::vector<BehavioralReevaluationArtifact>> list_behavioral_reevaluation_artifacts() const override;
    MemoryResultWith<BehavioralMemorySummary> get_behavioral_memory_summary() const override;
    MemoryResultWith<std::vector<ActivityInventoryItem>> list_activity_inventory_items() const override;
    MemoryResultWith<ActivityInventoryItem> get_activity_inventory_item_by_id(const ActivityInventoryItemId& activity_inventory_item_id) const override;
    MemoryResultWith<std::vector<ProceduralAuditRunRecord>> list_procedural_audit_runs() const override;
    MemoryResultWith<ProceduralAuditRunRecord> get_procedural_audit_run_by_id(const ProceduralAuditRunId& procedural_audit_run_id) const override;
    MemoryResultWith<std::vector<OptimizationProposalRecord>> list_optimization_proposal_records() const override;
    MemoryResultWith<OptimizationProposalRecord> get_optimization_proposal_record_by_id(const OptimizationProposalId& optimization_proposal_id) const override;
    MemoryResultWith<std::vector<OptimizationProposalRecord>> list_optimization_proposals_for_audit_run(const ProceduralAuditRunId& procedural_audit_run_id) const override;
    MemoryResultWith<ProceduralMemorySummary> get_procedural_memory_summary() const override;

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
    std::unordered_map<ScheduleItemId, ScheduledCommitment> commitments_by_id_;
    std::unordered_map<ScheduleItemId, SchedulingTaskCandidate> task_candidates_by_id_;
    std::unordered_map<WindowId, AvailabilityWindow> windows_by_id_;
    std::unordered_map<ConstraintSetId, SchedulingConstraintSet> constraint_sets_by_id_;
    std::unordered_map<ProposalId, SchedulingProposal> proposals_by_id_;
    std::unordered_map<ScheduleDecisionId, SchedulingDecisionRecord> decisions_by_id_;
    std::unordered_map<std::string, SchedulingConflict> conflicts_by_id_;
    std::unordered_map<BehavioralProposalId, BehavioralProposal> behavioral_proposals_by_id_;
    std::unordered_map<BehavioralStateSnapshotId, BehavioralStateSnapshot> behavioral_state_snapshots_by_id_;
    std::unordered_map<BehavioralDecisionId, BehavioralTriageDecision> behavioral_decisions_by_id_;
    std::unordered_map<BacklogItemId, BehavioralBacklogItem> behavioral_backlog_items_by_id_;
    std::unordered_map<BehavioralProposalId, BacklogItemId> behavioral_backlog_item_id_by_proposal_id_;
    std::unordered_map<InterventionId, BehavioralInterventionRecord> behavioral_interventions_by_id_;
    std::unordered_map<BehavioralReevaluationId, BehavioralReevaluationArtifact> behavioral_reevaluations_by_id_;
    std::unordered_map<ActivityInventoryItemId, ActivityInventoryItem> activity_inventory_by_id_;
    std::unordered_map<ProceduralAuditRunId, ProceduralAuditRunRecord> procedural_audit_runs_by_id_;
    std::unordered_map<OptimizationProposalId, OptimizationProposalRecord> optimization_proposals_by_id_;
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
