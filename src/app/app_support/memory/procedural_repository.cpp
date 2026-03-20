#include "app/app_support/memory/procedural_repository.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_procedural_proposals(core::MemoryService& memory_service) {
    std::vector<ArtifactEnvelope> artifacts;
    const auto records = memory_service.list_optimization_proposal_records();
    if (!records.ok || !records.value) return artifacts;
    for (const auto& item : *records.value) {
        ArtifactEnvelope artifact{"procedural_proposals", item.optimization_proposal_id, {}, {}};
        artifact.fields = {{"optimization_proposal_id", item.optimization_proposal_id},
                           {"procedural_audit_run_id", item.procedural_audit_run_id},
                           {"activity_inventory_item_id", item.activity_inventory_item_id},
                           {"opportunity_type", core::to_string(item.opportunity_type)},
                           {"effort_value_classification", core::to_string(item.effort_value_classification)},
                           {"title", item.title},
                           {"rationale", item.rationale},
                           {"source_module_id", item.source_module_id},
                           {"created_at", item.created_at},
                           {"updated_at", item.updated_at},
                           {"version", std::to_string(item.version)},
                           {"linked_behavioral_proposal_id", item.linked_behavioral_proposal_id},
                           {"triage_status", item.triage_status},
                           {"triage_decision_id", item.triage_decision_id},
                           {"automation_feasibility", core::to_string(item.automation_feasibility)},
                           {"risk_tier", item.risk_tier},
                           {"reliability_estimate", std::to_string(item.reliability_estimate)},
                           {"time_recovery_minutes", std::to_string(item.time_recovery_minutes)},
                           {"cognitive_recovery_score", std::to_string(item.cognitive_recovery_score)},
                           {"stress_recovery_score", std::to_string(item.stress_recovery_score)},
                           {"financial_cost_estimate", std::to_string(item.financial_cost_estimate)},
                           {"marginal_benefit_score", std::to_string(item.marginal_benefit_score)},
                           {"diminishing_return_flag", item.diminishing_return_flag ? "true" : "false"},
                           {"source_audit_run_id", item.source_audit_run_id}};
        artifact.nested_metadata = {{"attributes", item.attributes},
                                    {"energy_recovery_estimate",
                                     {{"recovered_minutes_per_week", std::to_string(item.energy_recovery_estimate.recovered_minutes_per_week)},
                                      {"recovered_effort_points", std::to_string(item.energy_recovery_estimate.recovered_effort_points)},
                                      {"confidence_label", item.energy_recovery_estimate.confidence_label}}}};
        artifacts.push_back(std::move(artifact));
    }
    return artifacts;
}
}  // namespace life_orchestrator::app::memory
