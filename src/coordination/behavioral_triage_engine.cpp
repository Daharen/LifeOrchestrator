#include "coordination/behavioral_triage_engine.hpp"

#include <algorithm>

namespace life_orchestrator::coordination {
namespace core = life_orchestrator::core;

core::BehavioralStateSnapshot BehavioralTriageEngine::effective_snapshot(
    const std::optional<core::BehavioralStateSnapshot>& latest) const {
    auto snapshot = latest.value_or(core::default_behavioral_state_snapshot());
    snapshot.behavioral_capacity_level = core::derive_behavioral_capacity_level(snapshot);
    snapshot.psychological_state_level = core::derive_psychological_state_level(snapshot);
    return snapshot;
}

std::vector<EngineDecisionRecord> BehavioralTriageEngine::triage(
    const std::vector<core::BehavioralProposal>& proposals,
    const core::BehavioralStateSnapshot& raw_snapshot,
    const std::string& source_module_id,
    const core::TimestampString& now) const {
    auto snapshot = effective_snapshot(raw_snapshot);
    auto ranked = proposals;
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b) {
        const auto prio_a = core::priority_rank(a.priority);
        const auto prio_b = core::priority_rank(b.priority);
        const auto roi_a = a.expected_benefit / std::max(1.0, a.estimated_behavioral_effort);
        const auto roi_b = b.expected_benefit / std::max(1.0, b.estimated_behavioral_effort);
        const auto time_a = a.latest_relevant_time.value_or(a.earliest_presentation_time.value_or("9999-12-31T23:59:59.999Z"));
        const auto time_b = b.latest_relevant_time.value_or(b.earliest_presentation_time.value_or("9999-12-31T23:59:59.999Z"));
        if (prio_a != prio_b) return prio_a > prio_b;
        if (roi_a != roi_b) return roi_a > roi_b;
        if (time_a != time_b) return time_a < time_b;
        return a.behavioral_proposal_id < b.behavioral_proposal_id;
    });

    std::vector<EngineDecisionRecord> out;
    int approved = snapshot.active_intervention_count;
    const int cap = core::active_intervention_cap(snapshot.behavioral_capacity_level);
    const int effort_gate = core::effort_gate_for_capacity(snapshot.behavioral_capacity_level);

    for (const auto& proposal : ranked) {
        const double roi = proposal.expected_benefit / std::max(1.0, proposal.estimated_behavioral_effort);
        core::BehavioralDecisionType type = core::BehavioralDecisionType::Approved;
        std::string gate_reason = "within_capacity";
        std::string priority_reason = "priority=" + core::to_string(proposal.priority) + ",roi=" + std::to_string(roi);
        std::optional<core::BehavioralBacklogItem> backlog;
        std::optional<core::BehavioralInterventionRecord> intervention;
        const auto source_proposal_id = proposal.attributes.contains("source_proposal_id") ? proposal.attributes.at("source_proposal_id") : proposal.behavioral_proposal_id;
        const auto source_audit_run_id = proposal.attributes.contains("source_audit_run_id") ? proposal.attributes.at("source_audit_run_id") : "none";
        const auto source_activity_id = proposal.attributes.contains("source_activity_id") ? proposal.attributes.at("source_activity_id") : "none";
        const auto rationale = proposal.description.empty() ? gate_reason + ":" + proposal.title : proposal.description;

        if (proposal.latest_relevant_time && *proposal.latest_relevant_time < now) {
            type = core::BehavioralDecisionType::Rejected;
            gate_reason = "latest_relevant_time_passed";
        } else if (proposal.estimated_behavioral_effort > effort_gate || approved >= cap) {
            const bool valuable = roi >= 1.5 || core::priority_rank(proposal.priority) >= core::priority_rank(core::BehavioralPriority::High);
            type = valuable ? core::BehavioralDecisionType::Deferred : core::BehavioralDecisionType::Backlogged;
            gate_reason = proposal.estimated_behavioral_effort > effort_gate ? "effort_exceeds_capacity_gate" : "active_intervention_cap_reached";
            backlog = core::BehavioralBacklogItem{"backlog." + proposal.behavioral_proposal_id,
                                                  proposal.behavioral_proposal_id,
                                                  core::BacklogStatus::Pending,
                                                  gate_reason,
                                                  now,
                                                  std::nullopt,
                                                  proposal.earliest_presentation_time,
                                                  source_module_id,
                                                  1,
                                                  source_proposal_id,
                                                  source_audit_run_id,
                                                  source_activity_id,
                                                  core::to_string(proposal.priority),
                                                  std::to_string(static_cast<int>(proposal.estimated_behavioral_effort)),
                                                  rationale};
        } else {
            ++approved;
            intervention = core::BehavioralInterventionRecord{"intervention." + proposal.behavioral_proposal_id,
                                                              proposal.behavioral_proposal_id,
                                                              "decision." + proposal.behavioral_proposal_id,
                                                              proposal.title,
                                                              proposal.presentation_mode,
                                                              proposal.earliest_presentation_time,
                                                              now,
                                                              "Approved",
                                                              source_module_id,
                                                              1,
                                                              source_proposal_id,
                                                              source_audit_run_id,
                                                              source_activity_id,
                                                              core::to_string(proposal.priority),
                                                              std::to_string(static_cast<int>(proposal.estimated_behavioral_effort)),
                                                              rationale};
        }

        out.push_back({proposal,
                       core::BehavioralTriageDecision{"decision." + proposal.behavioral_proposal_id,
                                                      proposal.behavioral_proposal_id,
                                                      type,
                                                      roi,
                                                      gate_reason,
                                                      priority_reason,
                                                      proposal.earliest_presentation_time,
                                                      intervention ? std::optional<core::InterventionId>(intervention->intervention_id) : std::nullopt,
                                                      now,
                                                      source_module_id,
                                                      gate_reason + ":" + proposal.title,
                                                      1},
                       backlog,
                       intervention});
    }
    return out;
}

}  // namespace life_orchestrator::coordination
