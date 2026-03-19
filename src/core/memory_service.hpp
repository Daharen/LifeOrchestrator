#pragma once

#include "core/behavioral.hpp"
#include "core/memory.hpp"

namespace life_orchestrator::core {

class MemoryService {
public:
    explicit MemoryService(IMemoryStore& store);

    IMemoryStore& store();
    const IMemoryStore& store() const;

    MemoryResult log_module_execution_episode(const SourceModuleId& source_module_id,
                                              const std::string& event_type,
                                              const std::string& summary,
                                              const std::vector<EntityId>& associated_entities);

    MemoryResult upsert_scheduled_commitment(const ScheduledCommitment& record);
    MemoryResult append_task_candidate(const SchedulingTaskCandidate& record);
    MemoryResult upsert_scheduling_candidate_record(const SchedulingCandidateRecord& record);
    MemoryResult upsert_schedule_proposal_artifact(const ScheduleProposalArtifact& record);
    MemoryResult upsert_availability_window(const AvailabilityWindow& record);
    MemoryResult upsert_constraint_set(const SchedulingConstraintSet& record);
    MemoryResult append_proposal(const SchedulingProposal& record);
    MemoryResult append_decision(const SchedulingDecisionRecord& record);
    MemoryResult append_conflict(const SchedulingConflict& record);
    MemoryResult append_behavioral_proposal(const BehavioralProposal& record);
    MemoryResult append_behavioral_state_snapshot(const BehavioralStateSnapshot& record);
    MemoryResult append_behavioral_decision(const BehavioralTriageDecision& record);
    MemoryResult upsert_behavioral_backlog_item(const BehavioralBacklogItem& record);
    MemoryResult append_behavioral_intervention(const BehavioralInterventionRecord& record);
    MemoryResult append_behavioral_reevaluation_artifact(const BehavioralReevaluationArtifact& record);
    MemoryResult upsert_activity_inventory_item(const ActivityInventoryItem& record);
    MemoryResult upsert_procedural_audit_run_record(const ProceduralAuditRunRecord& record);
    MemoryResult upsert_optimization_proposal_record(const OptimizationProposalRecord& record);

    MemoryResultWith<std::vector<ScheduledCommitment>> list_commitments_in_window(const TimestampString& start_time,
                                                                                  const TimestampString& end_time) const;
    MemoryResultWith<std::vector<SchedulingTaskCandidate>> list_task_candidates_by_status_and_range(
        ScheduleStatus status,
        const TimestampString& start_time,
        const TimestampString& end_time) const;
    MemoryResultWith<std::vector<AvailabilityWindow>> list_availability_windows_in_window(
        const TimestampString& start_time,
        const TimestampString& end_time) const;
    MemoryResultWith<std::vector<SchedulingCandidateRecord>> list_scheduling_candidate_records() const;
    MemoryResultWith<SchedulingCandidateRecord> get_scheduling_candidate_record_by_id(const SchedulingCandidateId& candidate_id) const;
    MemoryResultWith<std::vector<ScheduleProposalArtifact>> list_schedule_proposal_artifacts() const;
    MemoryResultWith<ScheduleProposalArtifact> get_schedule_proposal_artifact_by_id(const ScheduleProposalArtifactId& proposal_id) const;
    MemoryResultWith<std::vector<SchedulingProposal>> list_proposals_for_task_candidate(
        const ScheduleItemId& task_candidate_id) const;
    MemoryResultWith<std::vector<SchedulingConflict>> list_conflicts(const TimestampString& start_time,
                                                                     const TimestampString& end_time,
                                                                     const std::optional<ScheduleItemId>& schedule_item_id) const;
    MemoryResultWith<SchedulingProposal> get_proposal_by_id(const ProposalId& proposal_id) const;
    MemoryResultWith<ScheduledCommitment> get_commitment_by_id(const ScheduleItemId& commitment_id) const;
    MemoryResultWith<SchedulingTaskCandidate> get_task_candidate_by_id(const ScheduleItemId& task_candidate_id) const;
    MemoryResultWith<SchedulingConstraintSet> get_constraint_set_by_id(const ConstraintSetId& constraint_set_id) const;
    MemoryResultWith<std::vector<BehavioralProposal>> list_behavioral_proposals() const;
    MemoryResultWith<std::vector<BehavioralStateSnapshot>> list_recent_behavioral_state_snapshots(std::size_t max_records) const;
    MemoryResultWith<std::vector<BehavioralBacklogItem>> list_behavioral_backlog_items() const;
    MemoryResultWith<std::vector<BehavioralInterventionRecord>> list_behavioral_interventions(const std::string& status_filter, const std::optional<TimestampString>& due_by) const;
    MemoryResultWith<BehavioralProposal> get_behavioral_proposal_by_id(const BehavioralProposalId& proposal_id) const;
    MemoryResultWith<BehavioralTriageDecision> get_behavioral_decision_by_id(const BehavioralDecisionId& decision_id) const;
    MemoryResultWith<BehavioralBacklogItem> get_behavioral_backlog_item_by_proposal_id(const BehavioralProposalId& proposal_id) const;
    MemoryResultWith<std::vector<BehavioralReevaluationArtifact>> list_behavioral_reevaluation_artifacts() const;
    MemoryResultWith<BehavioralMemorySummary> get_behavioral_memory_summary() const;
    MemoryResultWith<std::vector<ActivityInventoryItem>> list_activity_inventory_items() const;
    MemoryResultWith<ActivityInventoryItem> get_activity_inventory_item_by_id(const ActivityInventoryItemId& activity_inventory_item_id) const;
    MemoryResultWith<std::vector<ProceduralAuditRunRecord>> list_procedural_audit_runs() const;
    MemoryResultWith<ProceduralAuditRunRecord> get_procedural_audit_run_by_id(const ProceduralAuditRunId& procedural_audit_run_id) const;
    MemoryResultWith<std::vector<OptimizationProposalRecord>> list_optimization_proposal_records() const;
    MemoryResultWith<OptimizationProposalRecord> get_optimization_proposal_record_by_id(const OptimizationProposalId& optimization_proposal_id) const;
    MemoryResultWith<std::vector<OptimizationProposalRecord>> list_optimization_proposals_for_audit_run(const ProceduralAuditRunId& procedural_audit_run_id) const;
    MemoryResultWith<ProceduralMemorySummary> get_procedural_memory_summary() const;

private:
    IMemoryStore& store_;
};

}  // namespace life_orchestrator::core
