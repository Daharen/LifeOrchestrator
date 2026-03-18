#include "coordination/scheduling_coordination_module.hpp"

#include <algorithm>

namespace life_orchestrator::coordination {
namespace {

std::string param(const core::ActionRequest& request, const std::string& key, const std::string& fallback = "") {
    auto it = request.parameters.find(key);
    return it == request.parameters.end() ? fallback : it->second;
}

core::SchedulingPriority parse_priority(const std::string& value) {
    if (value == "Low") return core::SchedulingPriority::Low;
    if (value == "High") return core::SchedulingPriority::High;
    if (value == "Critical") return core::SchedulingPriority::Critical;
    return core::SchedulingPriority::Normal;
}

core::ActionResponse fail(const core::ActionRequest& request, const std::string& module_id, const std::string& message) {
    return {request.request_id, core::ExecutionStatus::InvalidRequest, module_id, message, {}, core::current_timestamp_utc()};
}

}  // namespace

SchedulingCoordinationModule::SchedulingCoordinationModule(core::MemoryService* memory_service)
    : descriptor_{"coordination.scheduling", "Scheduling Coordination Module", core::ModuleClass::Coordination,
                  "Deterministic scheduling coordination over persisted memory state.",
                  {"scheduling.add_commitment", "scheduling.detect_conflicts", "scheduling.propose_time_blocks", "scheduling.commit_proposal", "scheduling.list_schedule_window"},
                  "Explicit key-value parameters per scheduling operation.",
                  "Structured ids, proposals, conflicts, and schedule window payload fields.",
                  "Memory-backed deterministic coordination state.", {}, core::RiskTier::Suggestive},
      memory_service_(memory_service) {}

const core::ModuleDescriptor& SchedulingCoordinationModule::descriptor() const { return descriptor_; }
bool SchedulingCoordinationModule::supports_capability(const core::CapabilityId& capability_id) const { return std::find(descriptor_.capabilities.begin(), descriptor_.capabilities.end(), capability_id) != descriptor_.capabilities.end(); }

core::ActionResponse SchedulingCoordinationModule::execute(const core::ActionRequest& request) {
    memory_service_->log_module_execution_episode(descriptor_.module_id, request.capability_id, "Scheduling operation started.", {});
    if (request.capability_id == "scheduling.add_commitment") return add_commitment(request);
    if (request.capability_id == "scheduling.detect_conflicts") return detect_conflicts(request);
    if (request.capability_id == "scheduling.propose_time_blocks") return propose_time_blocks(request);
    if (request.capability_id == "scheduling.commit_proposal") return commit_proposal(request);
    if (request.capability_id == "scheduling.list_schedule_window") return list_schedule_window(request);
    return {request.request_id, core::ExecutionStatus::NotFound, descriptor_.module_id, "Capability not supported.", {}, core::current_timestamp_utc()};
}

core::ActionResponse SchedulingCoordinationModule::add_commitment(const core::ActionRequest& request) {
    const auto start_time = param(request, "start_time");
    const auto end_time = param(request, "end_time");
    if (start_time.empty() || end_time.empty() || start_time >= end_time) return fail(request, descriptor_.module_id, "Commitment start_time must be before end_time.");
    const auto id = param(request, "schedule_item_id", "commitment." + request.request_id);
    const auto now = core::current_timestamp_utc();
    core::ScheduledCommitment record{id, param(request, "related_entity_id"), param(request, "title"), param(request, "description"), start_time, end_time, param(request, "timezone", "UTC"), parse_priority(param(request, "priority", "Normal")), descriptor_.module_id, now, now, 1, core::ScheduleStatus::Scheduled, {}};
    auto result = memory_service_->upsert_scheduled_commitment(record);
    if (!result.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, result.message, {}, core::current_timestamp_utc()};
    auto commitments = memory_service_->list_commitments_in_window(start_time, end_time);
    auto windows = memory_service_->list_availability_windows_in_window(start_time, end_time);
    auto conflicts = engine_.detect_conflicts(commitments.value.value_or(std::vector<core::ScheduledCommitment>{}), windows.value.value_or(std::vector<core::AvailabilityWindow>{}), descriptor_.module_id, now);
    for (const auto& conflict : conflicts) memory_service_->append_conflict(conflict);
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Commitment persisted.", {{"commitment_id", id}, {"conflict_count", std::to_string(conflicts.size())}}, core::current_timestamp_utc()};
}

core::ActionResponse SchedulingCoordinationModule::detect_conflicts(const core::ActionRequest& request) {
    const auto start_time = param(request, "start_time");
    const auto end_time = param(request, "end_time");
    auto commitments = memory_service_->list_commitments_in_window(start_time, end_time);
    auto windows = memory_service_->list_availability_windows_in_window(start_time, end_time);
    auto conflicts = engine_.detect_conflicts(commitments.value.value_or(std::vector<core::ScheduledCommitment>{}), windows.value.value_or(std::vector<core::AvailabilityWindow>{}), descriptor_.module_id, core::current_timestamp_utc());
    for (const auto& conflict : conflicts) memory_service_->append_conflict(conflict);
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Conflict detection completed.", {{"conflict_count", std::to_string(conflicts.size())}}, core::current_timestamp_utc()};
}

core::ActionResponse SchedulingCoordinationModule::propose_time_blocks(const core::ActionRequest& request) {
    const auto now = core::current_timestamp_utc();
    core::SchedulingTaskCandidate candidate{param(request, "schedule_item_id", "task." + request.request_id), param(request, "related_entity_id"), param(request, "title"), param(request, "description"), std::stoi(param(request, "estimated_duration_minutes", "30")), param(request, "earliest_start"), param(request, "latest_end"), parse_priority(param(request, "priority", "Normal")), param(request, "splittable", "0") == "1", std::stoi(param(request, "required_buffer_before_minutes", "0")), std::stoi(param(request, "required_buffer_after_minutes", "0")), {}, descriptor_.module_id, now, now, 1, core::ScheduleStatus::Pending};
    memory_service_->append_task_candidate(candidate);
    core::SchedulingConstraintSet constraint{"", 0, 0, param(request, "working_hours_only", "0") == "1", {}, {}, {}, descriptor_.module_id, now, now, 1};
    core::SchedulingConstraintSet* constraint_ptr = nullptr;
    if (!param(request, "constraint_set_id").empty()) {
        auto read = memory_service_->get_constraint_set_by_id(param(request, "constraint_set_id"));
        if (read.ok) {
            constraint = *read.value;
            constraint_ptr = &constraint;
        }
    } else {
        constraint.minimum_gap_minutes = std::stoi(param(request, "minimum_gap_minutes", "0"));
        constraint.allowed_window_ids = {};
        constraint_ptr = &constraint;
    }
    auto commitments = memory_service_->list_commitments_in_window(candidate.earliest_start, candidate.latest_end);
    auto windows = memory_service_->list_availability_windows_in_window(candidate.earliest_start, candidate.latest_end);
    std::vector<core::SchedulingConflict> conflicts;
    auto proposals = engine_.generate_proposals(candidate, commitments.value.value_or(std::vector<core::ScheduledCommitment>{}), windows.value.value_or(std::vector<core::AvailabilityWindow>{}), constraint_ptr, descriptor_.module_id, now, &conflicts);
    for (const auto& conflict : conflicts) memory_service_->append_conflict(conflict);
    for (const auto& proposal : proposals) memory_service_->append_proposal(proposal);
    if (proposals.empty()) return {request.request_id, core::ExecutionStatus::Rejected, descriptor_.module_id, "No valid proposals available.", {{"task_candidate_id", candidate.schedule_item_id}, {"conflict_count", std::to_string(conflicts.size())}}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Scheduling proposals generated.", {{"task_candidate_id", candidate.schedule_item_id}, {"proposal_count", std::to_string(proposals.size())}, {"first_proposal_id", proposals.front().proposal_id}}, core::current_timestamp_utc()};
}

core::ActionResponse SchedulingCoordinationModule::commit_proposal(const core::ActionRequest& request) {
    auto proposal = memory_service_->get_proposal_by_id(param(request, "proposal_id"));
    if (!proposal.ok) return fail(request, descriptor_.module_id, "Proposal not found.");
    auto candidate = memory_service_->get_task_candidate_by_id(proposal.value->related_task_candidate_id);
    if (!candidate.ok) return fail(request, descriptor_.module_id, "Task candidate not found.");
    core::SchedulingConstraintSet constraint{};
    core::SchedulingConstraintSet* constraint_ptr = nullptr;
    if (!proposal.value->based_on_constraint_set_id.empty()) {
        auto read = memory_service_->get_constraint_set_by_id(proposal.value->based_on_constraint_set_id);
        if (read.ok) { constraint = *read.value; constraint_ptr = &constraint; }
    }
    auto commitments = memory_service_->list_commitments_in_window(candidate.value->earliest_start, candidate.value->latest_end);
    auto windows = memory_service_->list_availability_windows_in_window(candidate.value->earliest_start, candidate.value->latest_end);
    if (!engine_.proposal_still_valid(*proposal.value, *candidate.value, commitments.value.value_or(std::vector<core::ScheduledCommitment>{}), windows.value.value_or(std::vector<core::AvailabilityWindow>{}), constraint_ptr)) {
        return {request.request_id, core::ExecutionStatus::Rejected, descriptor_.module_id, "Proposal is stale or invalid.", {}, core::current_timestamp_utc()};
    }
    const auto now = core::current_timestamp_utc();
    core::ScheduledCommitment commitment{"commitment.from." + proposal.value->proposal_id, candidate.value->related_entity_id, candidate.value->title, candidate.value->description, proposal.value->proposed_start_time, proposal.value->proposed_end_time, proposal.value->timezone, candidate.value->priority, descriptor_.module_id, now, now, 1, core::ScheduleStatus::Scheduled, {{"proposal_id", proposal.value->proposal_id}}};
    memory_service_->upsert_scheduled_commitment(commitment);
    auto committed = *proposal.value;
    committed.status = core::ProposalStatus::Committed;
    committed.version += 1;
    memory_service_->append_proposal(committed);
    core::SchedulingDecisionRecord decision{"decision." + proposal.value->proposal_id, proposal.value->proposal_id, commitment.schedule_item_id, "CommitProposal", now, descriptor_.module_id, "Proposal committed into scheduled commitment.", 1};
    memory_service_->append_decision(decision);
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Proposal committed.", {{"commitment_id", commitment.schedule_item_id}, {"decision_id", decision.decision_id}}, core::current_timestamp_utc()};
}

core::ActionResponse SchedulingCoordinationModule::list_schedule_window(const core::ActionRequest& request) {
    auto commitments = memory_service_->list_commitments_in_window(param(request, "start_time"), param(request, "end_time"));
    auto windows = memory_service_->list_availability_windows_in_window(param(request, "start_time"), param(request, "end_time"));
    auto conflicts = memory_service_->list_conflicts(param(request, "start_time"), param(request, "end_time"), std::nullopt);
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Schedule window listed.", {{"commitment_count", std::to_string(commitments.value.value_or(std::vector<core::ScheduledCommitment>{}).size())}, {"window_count", std::to_string(windows.value.value_or(std::vector<core::AvailabilityWindow>{}).size())}, {"conflict_count", std::to_string(conflicts.value.value_or(std::vector<core::SchedulingConflict>{}).size())}}, core::current_timestamp_utc()};
}

}  // namespace life_orchestrator::coordination
