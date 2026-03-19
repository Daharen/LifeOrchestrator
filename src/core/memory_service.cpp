#include "core/memory_service.hpp"

namespace life_orchestrator::core {

MemoryService::MemoryService(IMemoryStore& store) : store_(store) {}

IMemoryStore& MemoryService::store() { return store_; }
const IMemoryStore& MemoryService::store() const { return store_; }

MemoryResult MemoryService::log_module_execution_episode(const SourceModuleId& source_module_id,
                                                         const std::string& event_type,
                                                         const std::string& summary,
                                                         const std::vector<EntityId>& associated_entities) {
    EpisodicMemoryRecord record{.record_id = source_module_id + ":" + current_timestamp_utc(),
                                .timestamp = current_timestamp_utc(),
                                .event_type = event_type,
                                .source_module_id = source_module_id,
                                .associated_entity_ids = associated_entities,
                                .summary = summary,
                                .details = {},
                                .version = 1};
    return store_.append_episodic_record(record);
}

MemoryResult MemoryService::upsert_scheduled_commitment(const ScheduledCommitment& record) { return store_.upsert_scheduled_commitment(record); }
MemoryResult MemoryService::append_task_candidate(const SchedulingTaskCandidate& record) { return store_.append_task_candidate(record); }
MemoryResult MemoryService::upsert_availability_window(const AvailabilityWindow& record) { return store_.upsert_availability_window(record); }
MemoryResult MemoryService::upsert_constraint_set(const SchedulingConstraintSet& record) { return store_.upsert_constraint_set(record); }
MemoryResult MemoryService::append_proposal(const SchedulingProposal& record) { return store_.append_proposal(record); }
MemoryResult MemoryService::append_decision(const SchedulingDecisionRecord& record) { return store_.append_decision(record); }
MemoryResult MemoryService::append_conflict(const SchedulingConflict& record) { return store_.append_conflict(record); }
MemoryResult MemoryService::append_behavioral_proposal(const BehavioralProposal& record) { return store_.append_behavioral_proposal(record); }
MemoryResult MemoryService::append_behavioral_state_snapshot(const BehavioralStateSnapshot& record) { return store_.append_behavioral_state_snapshot(record); }
MemoryResult MemoryService::append_behavioral_decision(const BehavioralTriageDecision& record) { return store_.append_behavioral_decision(record); }
MemoryResult MemoryService::upsert_behavioral_backlog_item(const BehavioralBacklogItem& record) { return store_.upsert_behavioral_backlog_item(record); }
MemoryResult MemoryService::append_behavioral_intervention(const BehavioralInterventionRecord& record) { return store_.append_behavioral_intervention(record); }
MemoryResult MemoryService::append_behavioral_reevaluation_artifact(const BehavioralReevaluationArtifact& record) { return store_.append_behavioral_reevaluation_artifact(record); }
MemoryResult MemoryService::upsert_activity_inventory_item(const ActivityInventoryItem& record) { return store_.upsert_activity_inventory_item(record); }
MemoryResult MemoryService::upsert_procedural_audit_run_record(const ProceduralAuditRunRecord& record) { return store_.upsert_procedural_audit_run_record(record); }
MemoryResult MemoryService::upsert_optimization_proposal_record(const OptimizationProposalRecord& record) { return store_.upsert_optimization_proposal_record(record); }

MemoryResultWith<std::vector<ScheduledCommitment>> MemoryService::list_commitments_in_window(const TimestampString& start_time, const TimestampString& end_time) const { return store_.list_commitments_in_window(start_time, end_time); }
MemoryResultWith<std::vector<SchedulingTaskCandidate>> MemoryService::list_task_candidates_by_status_and_range(ScheduleStatus status, const TimestampString& start_time, const TimestampString& end_time) const { return store_.list_task_candidates_by_status_and_range(status, start_time, end_time); }
MemoryResultWith<std::vector<AvailabilityWindow>> MemoryService::list_availability_windows_in_window(const TimestampString& start_time, const TimestampString& end_time) const { return store_.list_availability_windows_in_window(start_time, end_time); }
MemoryResultWith<std::vector<SchedulingProposal>> MemoryService::list_proposals_for_task_candidate(const ScheduleItemId& task_candidate_id) const { return store_.list_proposals_for_task_candidate(task_candidate_id); }
MemoryResultWith<std::vector<SchedulingConflict>> MemoryService::list_conflicts(const TimestampString& start_time, const TimestampString& end_time, const std::optional<ScheduleItemId>& schedule_item_id) const { return store_.list_conflicts(start_time, end_time, schedule_item_id); }
MemoryResultWith<SchedulingProposal> MemoryService::get_proposal_by_id(const ProposalId& proposal_id) const { return store_.get_proposal_by_id(proposal_id); }
MemoryResultWith<ScheduledCommitment> MemoryService::get_commitment_by_id(const ScheduleItemId& commitment_id) const { return store_.get_commitment_by_id(commitment_id); }
MemoryResultWith<SchedulingTaskCandidate> MemoryService::get_task_candidate_by_id(const ScheduleItemId& task_candidate_id) const { return store_.get_task_candidate_by_id(task_candidate_id); }
MemoryResultWith<SchedulingConstraintSet> MemoryService::get_constraint_set_by_id(const ConstraintSetId& constraint_set_id) const { return store_.get_constraint_set_by_id(constraint_set_id); }
MemoryResultWith<std::vector<BehavioralProposal>> MemoryService::list_behavioral_proposals() const { return store_.list_behavioral_proposals(); }
MemoryResultWith<std::vector<BehavioralStateSnapshot>> MemoryService::list_recent_behavioral_state_snapshots(std::size_t max_records) const { return store_.list_recent_behavioral_state_snapshots(max_records); }
MemoryResultWith<std::vector<BehavioralBacklogItem>> MemoryService::list_behavioral_backlog_items() const { return store_.list_behavioral_backlog_items(); }
MemoryResultWith<std::vector<BehavioralInterventionRecord>> MemoryService::list_behavioral_interventions(const std::string& status_filter, const std::optional<TimestampString>& due_by) const { return store_.list_behavioral_interventions(status_filter, due_by); }
MemoryResultWith<BehavioralProposal> MemoryService::get_behavioral_proposal_by_id(const BehavioralProposalId& proposal_id) const { return store_.get_behavioral_proposal_by_id(proposal_id); }
MemoryResultWith<BehavioralTriageDecision> MemoryService::get_behavioral_decision_by_id(const BehavioralDecisionId& decision_id) const { return store_.get_behavioral_decision_by_id(decision_id); }
MemoryResultWith<BehavioralBacklogItem> MemoryService::get_behavioral_backlog_item_by_proposal_id(const BehavioralProposalId& proposal_id) const { return store_.get_behavioral_backlog_item_by_proposal_id(proposal_id); }
MemoryResultWith<std::vector<BehavioralReevaluationArtifact>> MemoryService::list_behavioral_reevaluation_artifacts() const { return store_.list_behavioral_reevaluation_artifacts(); }
MemoryResultWith<BehavioralMemorySummary> MemoryService::get_behavioral_memory_summary() const { return store_.get_behavioral_memory_summary(); }
MemoryResultWith<std::vector<ActivityInventoryItem>> MemoryService::list_activity_inventory_items() const { return store_.list_activity_inventory_items(); }
MemoryResultWith<ActivityInventoryItem> MemoryService::get_activity_inventory_item_by_id(const ActivityInventoryItemId& activity_inventory_item_id) const { return store_.get_activity_inventory_item_by_id(activity_inventory_item_id); }
MemoryResultWith<std::vector<ProceduralAuditRunRecord>> MemoryService::list_procedural_audit_runs() const { return store_.list_procedural_audit_runs(); }
MemoryResultWith<ProceduralAuditRunRecord> MemoryService::get_procedural_audit_run_by_id(const ProceduralAuditRunId& procedural_audit_run_id) const { return store_.get_procedural_audit_run_by_id(procedural_audit_run_id); }
MemoryResultWith<std::vector<OptimizationProposalRecord>> MemoryService::list_optimization_proposal_records() const { return store_.list_optimization_proposal_records(); }
MemoryResultWith<OptimizationProposalRecord> MemoryService::get_optimization_proposal_record_by_id(const OptimizationProposalId& optimization_proposal_id) const { return store_.get_optimization_proposal_record_by_id(optimization_proposal_id); }
MemoryResultWith<std::vector<OptimizationProposalRecord>> MemoryService::list_optimization_proposals_for_audit_run(const ProceduralAuditRunId& procedural_audit_run_id) const { return store_.list_optimization_proposals_for_audit_run(procedural_audit_run_id); }
MemoryResultWith<ProceduralMemorySummary> MemoryService::get_procedural_memory_summary() const { return store_.get_procedural_memory_summary(); }

}  // namespace life_orchestrator::core
