#include "meta/procedural_auditor_engine.hpp"

#include <algorithm>

namespace life_orchestrator::meta {
namespace core = life_orchestrator::core;
namespace {
int to_int_attribute(const core::ProceduralAttributes& attributes, const std::string& key, int fallback) {
    const auto it = attributes.find(key);
    if (it == attributes.end() || it->second.empty()) return fallback;
    return std::stoi(it->second);
}

std::string stable_proposal_id(const core::ActivityInventoryItem& item,
                               core::OptimizationOpportunityType opportunity,
                               core::EffortValueClassification classification) {
    return "proposal." + item.activity_inventory_item_id + "." + core::to_string(opportunity) + "." + core::to_string(classification);
}
}

core::EffortValueClassification ProceduralAuditorEngine::classify(const core::ActivityInventoryItem& item) const {
    const bool high_effort = item.effort_estimate >= 7 || item.duration_minutes >= 60;
    const bool low_value = item.outcome_value <= 4;
    if (high_effort && low_value) return core::EffortValueClassification::HighEffortLowValue;
    if (high_effort) return core::EffortValueClassification::HighEffortHighValue;
    if (low_value) return core::EffortValueClassification::LowEffortLowValue;
    return core::EffortValueClassification::LowEffortHighValue;
}

core::OptimizationOpportunityType ProceduralAuditorEngine::derive_opportunity_type(const core::ActivityInventoryItem& item) const {
    if (item.attributes.contains("repeatable") && item.attributes.at("repeatable") == "1") return core::OptimizationOpportunityType::Automation;
    if (item.outcome_value <= 2) return core::OptimizationOpportunityType::Elimination;
    if (item.effort_estimate >= 8) return core::OptimizationOpportunityType::Delegation;
    if (item.duration_minutes >= 45) return core::OptimizationOpportunityType::StructuralOptimization;
    return core::OptimizationOpportunityType::Simplification;
}

core::EnergyRecoveryEstimate ProceduralAuditorEngine::estimate_energy_recovery(const core::ActivityInventoryItem& item,
                                                                               core::OptimizationOpportunityType opportunity_type) const {
    int multiplier = 1;
    switch (opportunity_type) {
        case core::OptimizationOpportunityType::Automation: multiplier = 4; break;
        case core::OptimizationOpportunityType::Simplification: multiplier = 2; break;
        case core::OptimizationOpportunityType::Elimination: multiplier = 5; break;
        case core::OptimizationOpportunityType::Delegation: multiplier = 3; break;
        case core::OptimizationOpportunityType::StructuralOptimization: multiplier = 3; break;
    }
    return {item.duration_minutes * multiplier, item.effort_estimate * multiplier, multiplier >= 4 ? "high" : "medium"};
}

std::vector<core::OptimizationProposalRecord> ProceduralAuditorEngine::audit(
    const std::vector<core::ActivityInventoryItem>& items,
    const core::ProceduralAuditRunId& audit_run_id,
    const std::string& source_module_id,
    const core::TimestampString& now) const {
    std::vector<core::ActivityInventoryItem> ranked = items;
    std::sort(ranked.begin(), ranked.end(), [&](const auto& a, const auto& b) {
        const auto class_a = classify(a);
        const auto class_b = classify(b);
        if (class_a != class_b) return static_cast<int>(class_a) < static_cast<int>(class_b);
        const auto score_a = (a.effort_estimate * a.duration_minutes) - (a.outcome_value * 10);
        const auto score_b = (b.effort_estimate * b.duration_minutes) - (b.outcome_value * 10);
        if (score_a != score_b) return score_a > score_b;
        return a.activity_inventory_item_id < b.activity_inventory_item_id;
    });

    std::vector<core::OptimizationProposalRecord> out;
    for (const auto& item : ranked) {
        const auto opportunity = derive_opportunity_type(item);
        const auto classification = classify(item);
        const auto recovery = estimate_energy_recovery(item, opportunity);
        const auto necessity = to_int_attribute(item.attributes, "necessity", 3);
        const auto cognitive_load = to_int_attribute(item.attributes, "cognitive_load", item.effort_estimate);
        const auto stress_load = to_int_attribute(item.attributes, "stress_load", std::max(1, item.effort_estimate - 1));
        const auto financial_cost = to_int_attribute(item.attributes, "financial_cost", 0);
        const auto marginal_benefit = std::max(1, (item.outcome_value * 2) - necessity);
        const bool diminishing_returns = item.outcome_value >= 7 && item.effort_estimate <= 4;
        const auto feasibility = opportunity == core::OptimizationOpportunityType::Automation
                                     ? (item.attributes.contains("repeatable") && item.duration_minutes >= 30 ? core::AutomationFeasibility::High : core::AutomationFeasibility::Medium)
                                     : core::AutomationFeasibility::NotApplicable;
        const double reliability = opportunity == core::OptimizationOpportunityType::Automation ? 0.85 : (opportunity == core::OptimizationOpportunityType::Delegation ? 0.72 : 0.78);
        std::string rationale = "Deterministic procedural optimization derived from effort/value analysis.";
        if (opportunity == core::OptimizationOpportunityType::StructuralOptimization || opportunity == core::OptimizationOpportunityType::Simplification) {
            rationale += " Friction reduction rationale: remove repeated context switching and lower operational drag.";
        }
        if (opportunity == core::OptimizationOpportunityType::Delegation) {
            rationale += " Delegation recommendation balances time recovery against explicit financial cost.";
        }
        if (opportunity == core::OptimizationOpportunityType::Automation) {
            rationale += " Automation recommendation includes feasibility and reliability estimates.";
        }
        out.push_back({stable_proposal_id(item, opportunity, classification),
                       audit_run_id,
                       item.activity_inventory_item_id,
                       opportunity,
                       classification,
                       recovery,
                       "Optimize " + item.title,
                       rationale,
                       source_module_id,
                       now,
                       now,
                       1,
                       "",
                       "Pending",
                       "",
                       feasibility,
                       classification == core::EffortValueClassification::HighEffortLowValue ? "High" : "Moderate",
                       reliability,
                       recovery.recovered_minutes_per_week,
                       std::max(1, recovery.recovered_effort_points - cognitive_load),
                       std::max(1, recovery.recovered_effort_points - stress_load),
                       opportunity == core::OptimizationOpportunityType::Delegation ? std::max(10, financial_cost) : financial_cost,
                       marginal_benefit,
                       diminishing_returns,
                       audit_run_id,
                       {{"domain_source", item.domain_source},
                        {"frequency", item.frequency},
                        {"activity_analyzed", item.title},
                        {"estimated_effort_points", std::to_string(item.effort_estimate)},
                        {"estimated_energy_recovery_minutes", std::to_string(recovery.recovered_minutes_per_week)},
                        {"proposed_optimization", core::to_string(opportunity)},
                        {"friction_reduction_rationale", (opportunity == core::OptimizationOpportunityType::StructuralOptimization || opportunity == core::OptimizationOpportunityType::Simplification) ? "Reduce repetitive friction and context switching." : ""}}});
    }
    return out;
}

}  // namespace life_orchestrator::meta
