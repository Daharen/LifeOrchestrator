#include "coordination/scheduling_engine.hpp"

#include <algorithm>

namespace life_orchestrator::coordination {
namespace {

int minute_of_day(const std::string& timestamp) {
    return std::stoi(timestamp.substr(11, 2)) * 60 + std::stoi(timestamp.substr(14, 2));
}

std::string add_minutes_same_day(const std::string& timestamp, int minutes_to_add) {
    const int total = minute_of_day(timestamp) + minutes_to_add;
    if (total < 0 || total >= 24 * 60) {
        return "";
    }
    std::string output = timestamp;
    const int hour = total / 60;
    const int minute = total % 60;
    output.replace(11, 5,
                   (hour < 10 ? "0" : "") + std::to_string(hour) + ":" +
                       (minute < 10 ? "0" : "") + std::to_string(minute));
    return output;
}

bool overlaps(const std::string& a_start, const std::string& a_end, const std::string& b_start, const std::string& b_end) {
    return a_start < b_end && b_start < a_end;
}

bool is_window_allowed(const core::AvailabilityWindow& window, const core::SchedulingConstraintSet* constraint_set) {
    if (!constraint_set) return true;
    if (!constraint_set->allowed_window_ids.empty() &&
        std::find(constraint_set->allowed_window_ids.begin(), constraint_set->allowed_window_ids.end(), window.window_id) == constraint_set->allowed_window_ids.end()) {
        return false;
    }
    return std::find(constraint_set->blocked_window_ids.begin(), constraint_set->blocked_window_ids.end(), window.window_id) == constraint_set->blocked_window_ids.end();
}

bool contained_by_any_window(const std::string& start_time,
                             const std::string& end_time,
                             const std::vector<core::AvailabilityWindow>& windows,
                             const core::SchedulingConstraintSet* constraint_set) {
    std::vector<core::AvailabilityWindow> allowed;
    for (const auto& window : windows) {
        if (window.start_time < window.end_time && is_window_allowed(window, constraint_set)) {
            allowed.push_back(window);
        }
    }
    if (!allowed.empty()) {
        for (const auto& window : allowed) {
            if (window.start_time <= start_time && end_time <= window.end_time) {
                return true;
            }
        }
        return false;
    }
    if (constraint_set && constraint_set->working_hours_only) {
        return minute_of_day(start_time) >= 9 * 60 && minute_of_day(end_time) <= 17 * 60;
    }
    return true;
}

}  // namespace

std::vector<core::SchedulingConflict> SchedulingEngine::detect_conflicts(const std::vector<core::ScheduledCommitment>& commitments,
                                                                         const std::vector<core::AvailabilityWindow>& windows,
                                                                         const std::string& source_module_id,
                                                                         const core::TimestampString& detected_at) const {
    std::vector<core::SchedulingConflict> conflicts;
    auto sorted = commitments;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) {
        return a.start_time == b.start_time ? a.schedule_item_id < b.schedule_item_id : a.start_time < b.start_time;
    });

    for (const auto& window : windows) {
        if (window.start_time >= window.end_time) {
            conflicts.push_back({"conflict.invalid_window." + window.window_id,
                                 core::ConflictType::InvalidWindow,
                                 window.window_id,
                                 "",
                                 "Availability window start must be before end.",
                                 detected_at,
                                 source_module_id,
                                 {{"window_id", window.window_id}}});
        }
    }

    for (std::size_t i = 0; i < sorted.size(); ++i) {
        for (std::size_t j = i + 1; j < sorted.size(); ++j) {
            if (overlaps(sorted[i].start_time, sorted[i].end_time, sorted[j].start_time, sorted[j].end_time)) {
                conflicts.push_back({"conflict.overlap." + sorted[i].schedule_item_id + "." + sorted[j].schedule_item_id,
                                     core::ConflictType::Overlap,
                                     sorted[i].schedule_item_id,
                                     sorted[j].schedule_item_id,
                                     "Commitments overlap in time.",
                                     detected_at,
                                     source_module_id,
                                     {{"primary_start", sorted[i].start_time}, {"secondary_start", sorted[j].start_time}}});
            }
        }
        if (!windows.empty() && !contained_by_any_window(sorted[i].start_time, sorted[i].end_time, windows, nullptr)) {
            conflicts.push_back({"conflict.outside." + sorted[i].schedule_item_id,
                                 core::ConflictType::OutsideAvailability,
                                 sorted[i].schedule_item_id,
                                 "",
                                 "Commitment falls outside availability windows.",
                                 detected_at,
                                 source_module_id,
                                 {}});
        }
    }

    std::sort(conflicts.begin(), conflicts.end(), [](const auto& a, const auto& b) { return a.conflict_id < b.conflict_id; });
    return conflicts;
}

std::vector<core::SchedulingProposal> SchedulingEngine::generate_proposals(
    const core::SchedulingTaskCandidate& candidate,
    const std::vector<core::ScheduledCommitment>& commitments,
    const std::vector<core::AvailabilityWindow>& windows,
    const core::SchedulingConstraintSet* constraint_set,
    const std::string& source_module_id,
    const core::TimestampString& generated_at,
    std::vector<core::SchedulingConflict>* derived_conflicts) const {
    const int minimum_gap_minutes = constraint_set ? constraint_set->minimum_gap_minutes : 0;
    std::vector<core::SchedulingConflict> conflicts;
    std::vector<core::SchedulingProposal> proposals;

    auto sorted_commitments = commitments;
    std::sort(sorted_commitments.begin(), sorted_commitments.end(), [](const auto& a, const auto& b) {
        return a.start_time == b.start_time ? a.schedule_item_id < b.schedule_item_id : a.start_time < b.start_time;
    });
    auto sorted_windows = windows;
    std::sort(sorted_windows.begin(), sorted_windows.end(), [](const auto& a, const auto& b) {
        return a.start_time == b.start_time ? a.window_id < b.window_id : a.start_time < b.start_time;
    });

    auto try_gap = [&](const std::string& gap_start, const std::string& gap_end, const std::string& basis) {
        const std::string proposed_start = std::max(gap_start, candidate.earliest_start);
        const std::string proposed_end = add_minutes_same_day(proposed_start, candidate.required_buffer_before_minutes + candidate.estimated_duration_minutes + candidate.required_buffer_after_minutes);
        if (proposed_end.empty()) return;
        if (proposed_end > gap_end || proposed_end > candidate.latest_end) {
            conflicts.push_back({"conflict.duration." + candidate.schedule_item_id + "." + basis,
                                 core::ConflictType::DurationInsufficient,
                                 candidate.schedule_item_id,
                                 basis,
                                 "Gap is too small for candidate duration and required buffers.",
                                 generated_at,
                                 source_module_id,
                                 {}});
            return;
        }
        const std::string commitment_start = add_minutes_same_day(proposed_start, candidate.required_buffer_before_minutes);
        const std::string commitment_end = add_minutes_same_day(commitment_start, candidate.estimated_duration_minutes);
        if (commitment_start.empty() || commitment_end.empty()) return;
        if (!contained_by_any_window(commitment_start, commitment_end, sorted_windows, constraint_set)) {
            conflicts.push_back({"conflict.outside_candidate." + candidate.schedule_item_id + "." + basis,
                                 core::ConflictType::OutsideAvailability,
                                 candidate.schedule_item_id,
                                 basis,
                                 "Candidate placement falls outside allowed availability.",
                                 generated_at,
                                 source_module_id,
                                 {{"basis", basis}}});
            return;
        }
        for (const auto& commitment : sorted_commitments) {
            const auto padded_start = add_minutes_same_day(commitment.start_time, -minimum_gap_minutes);
            const auto padded_end = add_minutes_same_day(commitment.end_time, minimum_gap_minutes);
            if (!padded_start.empty() && !padded_end.empty() && overlaps(proposed_start, proposed_end, padded_start, padded_end)) {
                return;
            }
        }
        proposals.push_back({"proposal." + candidate.schedule_item_id + "." + commitment_start,
                             candidate.schedule_item_id,
                             commitment_start,
                             commitment_end,
                             "UTC",
                             0,
                             "Earliest valid time first; ties broken by shortest slack then proposal id.",
                             constraint_set ? constraint_set->constraint_set_id : "",
                             generated_at,
                             source_module_id,
                             1,
                             core::ProposalStatus::Proposed});
    };

    if (!sorted_windows.empty()) {
        for (const auto& window : sorted_windows) {
            if (window.start_time >= window.end_time) {
                conflicts.push_back({"conflict.invalid_window." + window.window_id,
                                     core::ConflictType::InvalidWindow,
                                     candidate.schedule_item_id,
                                     window.window_id,
                                     "Availability window start must be before end.",
                                     generated_at,
                                     source_module_id,
                                     {}});
                continue;
            }
            if (!is_window_allowed(window, constraint_set)) {
                continue;
            }
            std::vector<core::ScheduledCommitment> overlapping;
            for (const auto& commitment : sorted_commitments) {
                if (overlaps(window.start_time, window.end_time, commitment.start_time, commitment.end_time)) {
                    overlapping.push_back(commitment);
                }
            }
            std::string cursor = std::max(window.start_time, candidate.earliest_start);
            for (const auto& commitment : overlapping) {
                const std::string padded_start = add_minutes_same_day(commitment.start_time, -minimum_gap_minutes);
                const std::string padded_end = add_minutes_same_day(commitment.end_time, minimum_gap_minutes);
                if (!padded_start.empty() && cursor < padded_start) {
                    try_gap(cursor, padded_start, window.window_id);
                }
                if (!padded_end.empty() && padded_end > cursor) {
                    cursor = padded_end;
                }
            }
            if (cursor < window.end_time) {
                try_gap(cursor, window.end_time, window.window_id);
            }
        }
    } else {
        if (constraint_set && !constraint_set->working_hours_only) {
            conflicts.push_back({"conflict.outside_candidate." + candidate.schedule_item_id + ".unbounded",
                                 core::ConflictType::OutsideAvailability,
                                 candidate.schedule_item_id,
                                 "",
                                 "No availability windows exist for proposal generation.",
                                 generated_at,
                                 source_module_id,
                                 {}});
        } else {
            try_gap(candidate.earliest_start, candidate.latest_end, "working_hours_only");
        }
    }

    std::sort(proposals.begin(), proposals.end(), [](const auto& a, const auto& b) {
        if (a.proposed_start_time != b.proposed_start_time) return a.proposed_start_time < b.proposed_start_time;
        const auto a_slack = minute_of_day(a.proposed_end_time) - minute_of_day(a.proposed_start_time);
        const auto b_slack = minute_of_day(b.proposed_end_time) - minute_of_day(b.proposed_start_time);
        if (a_slack != b_slack) return a_slack < b_slack;
        return a.proposal_id < b.proposal_id;
    });
    for (std::size_t i = 0; i < proposals.size(); ++i) {
        proposals[i].proposal_rank = static_cast<int>(i + 1);
    }
    std::sort(conflicts.begin(), conflicts.end(), [](const auto& a, const auto& b) { return a.conflict_id < b.conflict_id; });
    if (derived_conflicts) *derived_conflicts = conflicts;
    return proposals;
}

bool SchedulingEngine::proposal_still_valid(const core::SchedulingProposal& proposal,
                                            const core::SchedulingTaskCandidate& candidate,
                                            const std::vector<core::ScheduledCommitment>& commitments,
                                            const std::vector<core::AvailabilityWindow>& windows,
                                            const core::SchedulingConstraintSet* constraint_set) const {
    if (proposal.status != core::ProposalStatus::Proposed && proposal.status != core::ProposalStatus::Accepted) return false;
    if (proposal.proposed_start_time < candidate.earliest_start || proposal.proposed_end_time > candidate.latest_end) return false;
    if (!contained_by_any_window(proposal.proposed_start_time, proposal.proposed_end_time, windows, constraint_set)) return false;
    const int minimum_gap_minutes = constraint_set ? constraint_set->minimum_gap_minutes : 0;
    for (const auto& commitment : commitments) {
        const auto padded_start = add_minutes_same_day(commitment.start_time, -minimum_gap_minutes);
        const auto padded_end = add_minutes_same_day(commitment.end_time, minimum_gap_minutes);
        if (!padded_start.empty() && !padded_end.empty() && overlaps(proposal.proposed_start_time, proposal.proposed_end_time, padded_start, padded_end)) {
            return false;
        }
    }
    return true;
}

}  // namespace life_orchestrator::coordination
