#pragma once

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
    MemoryResult upsert_availability_window(const AvailabilityWindow& record);
    MemoryResult upsert_constraint_set(const SchedulingConstraintSet& record);
    MemoryResult append_proposal(const SchedulingProposal& record);
    MemoryResult append_decision(const SchedulingDecisionRecord& record);
    MemoryResult append_conflict(const SchedulingConflict& record);

    MemoryResultWith<std::vector<ScheduledCommitment>> list_commitments_in_window(const TimestampString& start_time,
                                                                                  const TimestampString& end_time) const;
    MemoryResultWith<std::vector<SchedulingTaskCandidate>> list_task_candidates_by_status_and_range(
        ScheduleStatus status,
        const TimestampString& start_time,
        const TimestampString& end_time) const;
    MemoryResultWith<std::vector<AvailabilityWindow>> list_availability_windows_in_window(
        const TimestampString& start_time,
        const TimestampString& end_time) const;
    MemoryResultWith<std::vector<SchedulingProposal>> list_proposals_for_task_candidate(
        const ScheduleItemId& task_candidate_id) const;
    MemoryResultWith<std::vector<SchedulingConflict>> list_conflicts(const TimestampString& start_time,
                                                                     const TimestampString& end_time,
                                                                     const std::optional<ScheduleItemId>& schedule_item_id) const;
    MemoryResultWith<SchedulingProposal> get_proposal_by_id(const ProposalId& proposal_id) const;
    MemoryResultWith<ScheduledCommitment> get_commitment_by_id(const ScheduleItemId& commitment_id) const;
    MemoryResultWith<SchedulingTaskCandidate> get_task_candidate_by_id(const ScheduleItemId& task_candidate_id) const;
    MemoryResultWith<SchedulingConstraintSet> get_constraint_set_by_id(const ConstraintSetId& constraint_set_id) const;

private:
    IMemoryStore& store_;
};

}  // namespace life_orchestrator::core
