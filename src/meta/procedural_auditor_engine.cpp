#include "meta/procedural_auditor_engine.hpp"

#include <algorithm>

namespace life_orchestrator::meta {
namespace core = life_orchestrator::core;

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
        out.push_back({"proposal." + item.activity_inventory_item_id,
                       audit_run_id,
                       item.activity_inventory_item_id,
                       opportunity,
                       classification,
                       recovery,
                       "Optimize " + item.title,
                       "Deterministic procedural optimization derived from effort/value analysis.",
                       source_module_id,
                       now,
                       now,
                       1,
                       "",
                       "Pending",
                       "",
                       {{"domain_source", item.domain_source}, {"frequency", item.frequency}}});
    }
    return out;
}

}  // namespace life_orchestrator::meta
