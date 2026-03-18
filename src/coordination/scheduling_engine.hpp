#pragma once

#include "core/contracts.hpp"

#include <vector>

namespace life_orchestrator::coordination {

class SchedulingEngine {
public:
    std::vector<core::SchedulingConflict> detect_conflicts(const std::vector<core::ScheduledCommitment>& commitments,
                                                           const std::vector<core::AvailabilityWindow>& windows,
                                                           const std::string& source_module_id,
                                                           const core::TimestampString& detected_at) const;

    std::vector<core::SchedulingProposal> generate_proposals(
        const core::SchedulingTaskCandidate& candidate,
        const std::vector<core::ScheduledCommitment>& commitments,
        const std::vector<core::AvailabilityWindow>& windows,
        const core::SchedulingConstraintSet* constraint_set,
        const std::string& source_module_id,
        const core::TimestampString& generated_at,
        std::vector<core::SchedulingConflict>* derived_conflicts) const;

    bool proposal_still_valid(const core::SchedulingProposal& proposal,
                              const core::SchedulingTaskCandidate& candidate,
                              const std::vector<core::ScheduledCommitment>& commitments,
                              const std::vector<core::AvailabilityWindow>& windows,
                              const core::SchedulingConstraintSet* constraint_set) const;
};

}  // namespace life_orchestrator::coordination
