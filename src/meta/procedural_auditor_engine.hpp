#pragma once

#include "core/procedural.hpp"

#include <vector>

namespace life_orchestrator::meta {

class ProceduralAuditorEngine {
public:
    life_orchestrator::core::EffortValueClassification classify(const life_orchestrator::core::ActivityInventoryItem& item) const;
    life_orchestrator::core::OptimizationOpportunityType derive_opportunity_type(const life_orchestrator::core::ActivityInventoryItem& item) const;
    life_orchestrator::core::EnergyRecoveryEstimate estimate_energy_recovery(const life_orchestrator::core::ActivityInventoryItem& item,
                                                                             life_orchestrator::core::OptimizationOpportunityType opportunity_type) const;
    std::vector<life_orchestrator::core::OptimizationProposalRecord> audit(
        const std::vector<life_orchestrator::core::ActivityInventoryItem>& items,
        const life_orchestrator::core::ProceduralAuditRunId& audit_run_id,
        const std::string& source_module_id,
        const life_orchestrator::core::TimestampString& now) const;
};

}  // namespace life_orchestrator::meta
