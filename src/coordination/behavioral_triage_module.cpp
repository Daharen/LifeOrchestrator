#include "coordination/behavioral_triage_module.hpp"

#include <algorithm>
#include <sstream>

namespace life_orchestrator::coordination {
namespace {
std::string param(const life_orchestrator::core::ActionRequest& request, const std::string& key, const std::string& fallback = "") {
    auto it = request.parameters.find(key);
    return it == request.parameters.end() ? fallback : it->second;
}
double to_double(const std::string& value, double fallback) { return value.empty() ? fallback : std::stod(value); }
int to_int(const std::string& value, int fallback) { return value.empty() ? fallback : std::stoi(value); }
std::vector<std::string> split(const std::string& raw) {
    std::vector<std::string> out; std::stringstream ss(raw); std::string item; while (std::getline(ss, item, '|')) if (!item.empty()) out.push_back(item); return out;
}
}
namespace core = life_orchestrator::core;

BehavioralTriageModule::BehavioralTriageModule(core::MemoryService* memory_service)
    : descriptor_{"coordination.behavioral_triage", "Behavioral Triage Module", core::ModuleClass::Coordination,
                  "Deterministic behavioral demand governance across proposals.",
                  {"behavioral.record_state", "behavioral.triage_proposals", "behavioral.list_backlog", "behavioral.reevaluate_backlog", "behavioral.list_next_interventions"},
                  "Explicit key-value behavioral triage parameters.",
                  "Structured ids, capacity, decisions, backlog, and intervention summaries.",
                  "Memory-backed behavioral governance state.", {}, core::RiskTier::Suggestive},
      memory_service_(memory_service) {}

const core::ModuleDescriptor& BehavioralTriageModule::descriptor() const { return descriptor_; }
bool BehavioralTriageModule::supports_capability(const core::CapabilityId& capability_id) const { return std::find(descriptor_.capabilities.begin(), descriptor_.capabilities.end(), capability_id) != descriptor_.capabilities.end(); }

core::ActionResponse BehavioralTriageModule::execute(const core::ActionRequest& request) {
    memory_service_->log_module_execution_episode(descriptor_.module_id, request.capability_id, "Behavioral triage operation started.", {});
    if (request.capability_id == "behavioral.record_state") return record_state(request);
    if (request.capability_id == "behavioral.triage_proposals") return triage_proposals(request);
    if (request.capability_id == "behavioral.list_backlog") return list_backlog(request);
    if (request.capability_id == "behavioral.reevaluate_backlog") return reevaluate_backlog(request);
    if (request.capability_id == "behavioral.list_next_interventions") return list_next_interventions(request);
    return {request.request_id, core::ExecutionStatus::NotFound, descriptor_.module_id, "Capability not supported.", {}, core::current_timestamp_utc()};
}

core::ActionResponse BehavioralTriageModule::record_state(const core::ActionRequest& request) {
    auto snapshot = core::BehavioralStateSnapshot{param(request, "behavioral_state_snapshot_id", "state." + request.request_id),
                                                  param(request, "captured_at", core::current_timestamp_utc()),
                                                  descriptor_.module_id,
                                                  to_int(param(request, "active_intervention_count"), 0),
                                                  to_int(param(request, "backlog_count"), 0),
                                                  to_double(param(request, "schedule_density_score"), 0.25),
                                                  to_double(param(request, "recent_compliance_rate"), 0.8),
                                                  to_double(param(request, "recent_failure_frequency"), 0.1),
                                                  to_double(param(request, "fatigue_score"), 0.2),
                                                  to_double(param(request, "stress_score"), 0.2),
                                                  core::BehavioralCapacityLevel::Medium,
                                                  core::PsychologicalStateLevel::Stable,
                                                  param(request, "notes"),
                                                  1};
    snapshot.behavioral_capacity_level = param(request, "behavioral_capacity_level").empty() ? core::derive_behavioral_capacity_level(snapshot) : core::behavioral_capacity_level_from_string(param(request, "behavioral_capacity_level"));
    snapshot.psychological_state_level = param(request, "psychological_state_level").empty() ? core::derive_psychological_state_level(snapshot) : core::psychological_state_level_from_string(param(request, "psychological_state_level"));
    auto write = memory_service_->append_behavioral_state_snapshot(snapshot);
    if (!write.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, write.message, {}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Behavioral state recorded.", {{"snapshot_id", snapshot.behavioral_state_snapshot_id}, {"capacity_level", core::to_string(snapshot.behavioral_capacity_level)}}, core::current_timestamp_utc()};
}

core::ActionResponse BehavioralTriageModule::triage_proposals(const core::ActionRequest& request, bool backlog_only) {
    std::vector<core::BehavioralProposal> proposals;
    const auto now = core::current_timestamp_utc();
    const auto proposal_count = std::max(1, to_int(param(request, "proposal_count"), 1));
    for (int i = 0; i < proposal_count; ++i) {
        const auto suffix = proposal_count == 1 ? std::string{} : std::to_string(i + 1);
        const auto proposal_id = param(request, "proposal_id" + suffix, "proposal." + request.request_id + (suffix.empty() ? "" : "." + suffix));
        core::BehavioralProposal proposal{proposal_id,
                                          core::behavioral_proposal_type_from_string(param(request, "proposal_type" + suffix, "Reminder")),
                                          param(request, "title" + suffix, "Untitled Proposal"),
                                          param(request, "description" + suffix),
                                          descriptor_.module_id,
                                          split(param(request, "related_entity_ids" + suffix)),
                                          core::behavioral_priority_from_string(param(request, "priority" + suffix, "Normal")),
                                          to_double(param(request, "estimated_behavioral_effort" + suffix), 1.0),
                                          to_double(param(request, "expected_benefit" + suffix), 1.0),
                                          to_int(param(request, "expected_time_cost_minutes" + suffix), 15),
                                          core::intervention_presentation_mode_from_string(param(request, "presentation_mode" + suffix, "SuggestivePrompt")),
                                          param(request, "earliest_presentation_time" + suffix).empty() ? std::nullopt : std::optional<core::TimestampString>(param(request, "earliest_presentation_time" + suffix)),
                                          param(request, "latest_relevant_time" + suffix).empty() ? std::nullopt : std::optional<core::TimestampString>(param(request, "latest_relevant_time" + suffix)),
                                          now,
                                          now,
                                          1,
                                          {}};
        if (!backlog_only) {
            auto write = memory_service_->append_behavioral_proposal(proposal);
            if (!write.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, write.message, {}, core::current_timestamp_utc()};
        }
        proposals.push_back(proposal);
    }
    auto snapshots = memory_service_->list_recent_behavioral_state_snapshots(1);
    const auto snapshot = engine_.effective_snapshot(snapshots.ok && snapshots.value && !snapshots.value->empty() ? std::optional<core::BehavioralStateSnapshot>(snapshots.value->front()) : std::nullopt);
    const auto decisions = engine_.triage(proposals, snapshot, descriptor_.module_id, now);
    int approved = 0, deferred = 0, backlogged = 0, rejected = 0;
    std::string first_approved, first_backlog;
    for (const auto& item : decisions) {
        memory_service_->append_behavioral_decision(item.decision);
        if (item.backlog_item) memory_service_->upsert_behavioral_backlog_item(*item.backlog_item);
        if (item.intervention) memory_service_->append_behavioral_intervention(*item.intervention);
        switch (item.decision.decision_type) {
            case core::BehavioralDecisionType::Approved: ++approved; if (first_approved.empty()) first_approved = item.proposal.behavioral_proposal_id; break;
            case core::BehavioralDecisionType::Deferred: ++deferred; if (first_backlog.empty()) first_backlog = item.proposal.behavioral_proposal_id; break;
            case core::BehavioralDecisionType::Backlogged: ++backlogged; if (first_backlog.empty()) first_backlog = item.proposal.behavioral_proposal_id; break;
            case core::BehavioralDecisionType::Rejected: ++rejected; break;
        }
    }
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Behavioral proposals triaged.", {{"approved_count", std::to_string(approved)}, {"deferred_count", std::to_string(deferred)}, {"backlogged_count", std::to_string(backlogged)}, {"rejected_count", std::to_string(rejected)}, {"first_approved_proposal_id", first_approved}, {"first_backlog_proposal_id", first_backlog}, {"capacity_level", core::to_string(snapshot.behavioral_capacity_level)}}, core::current_timestamp_utc()};
}

core::ActionResponse BehavioralTriageModule::list_backlog(const core::ActionRequest& request) {
    auto backlog = memory_service_->list_behavioral_backlog_items();
    if (!backlog.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, backlog.message, {}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Behavioral backlog listed.", {{"backlog_count", std::to_string(backlog.value->size())}, {"first_backlog_item_id", backlog.value->empty() ? std::string{} : backlog.value->front().backlog_item_id}}, core::current_timestamp_utc()};
}

core::ActionResponse BehavioralTriageModule::reevaluate_backlog(const core::ActionRequest& request) {
    auto backlog = memory_service_->list_behavioral_backlog_items();
    if (!backlog.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, backlog.message, {}, core::current_timestamp_utc()};
    std::vector<core::BehavioralProposal> eligible;
    const auto now = core::current_timestamp_utc();
    for (const auto& item : *backlog.value) {
        if (item.reconsider_after && *item.reconsider_after > now) continue;
        auto proposal = memory_service_->get_behavioral_proposal_by_id(item.behavioral_proposal_id);
        if (proposal.ok) eligible.push_back(*proposal.value);
    }
    auto snapshots = memory_service_->list_recent_behavioral_state_snapshots(1);
    const auto snapshot = engine_.effective_snapshot(snapshots.ok && snapshots.value && !snapshots.value->empty() ? std::optional<core::BehavioralStateSnapshot>(snapshots.value->front()) : std::nullopt);
    const auto decisions = engine_.triage(eligible, snapshot, descriptor_.module_id, now);
    int promoted = 0;
    for (const auto& item : decisions) {
        memory_service_->append_behavioral_decision(item.decision);
        auto backlog_item = memory_service_->get_behavioral_backlog_item_by_proposal_id(item.proposal.behavioral_proposal_id);
        if (backlog_item.ok) {
            auto updated = *backlog_item.value;
            updated.last_reconsidered_at = now;
            updated.version += 1;
            if (item.decision.decision_type == core::BehavioralDecisionType::Approved) {
                updated.status = core::BacklogStatus::Approved;
                ++promoted;
            } else if (item.decision.decision_type == core::BehavioralDecisionType::Rejected) {
                updated.status = core::BacklogStatus::Rejected;
            } else {
                updated.status = core::BacklogStatus::Reconsidered;
            }
            memory_service_->upsert_behavioral_backlog_item(updated);
        }
        if (item.intervention) memory_service_->append_behavioral_intervention(*item.intervention);
    }
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Behavioral backlog reevaluated.", {{"eligible_count", std::to_string(eligible.size())}, {"promoted_count", std::to_string(promoted)}}, core::current_timestamp_utc()};
}

core::ActionResponse BehavioralTriageModule::list_next_interventions(const core::ActionRequest& request) {
    auto interventions = memory_service_->list_behavioral_interventions(param(request, "status", "Approved"), param(request, "due_by").empty() ? std::nullopt : std::optional<core::TimestampString>(param(request, "due_by")));
    if (!interventions.ok) return {request.request_id, core::ExecutionStatus::Failed, descriptor_.module_id, interventions.message, {}, core::current_timestamp_utc()};
    return {request.request_id, core::ExecutionStatus::Succeeded, descriptor_.module_id, "Behavioral interventions listed.", {{"intervention_count", std::to_string(interventions.value->size())}, {"first_intervention_id", interventions.value->empty() ? std::string{} : interventions.value->front().intervention_id}}, core::current_timestamp_utc()};
}

}  // namespace life_orchestrator::coordination
