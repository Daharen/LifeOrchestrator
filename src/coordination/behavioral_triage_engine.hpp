#pragma once

#include "core/behavioral.hpp"

#include <vector>

namespace life_orchestrator::coordination {

struct EngineDecisionRecord {
    life_orchestrator::core::BehavioralProposal proposal;
    life_orchestrator::core::BehavioralTriageDecision decision;
    std::optional<life_orchestrator::core::BehavioralBacklogItem> backlog_item;
    std::optional<life_orchestrator::core::BehavioralInterventionRecord> intervention;
};

class BehavioralTriageEngine {
public:
    life_orchestrator::core::BehavioralStateSnapshot effective_snapshot(
        const std::optional<life_orchestrator::core::BehavioralStateSnapshot>& latest) const;

    std::vector<EngineDecisionRecord> triage(
        const std::vector<life_orchestrator::core::BehavioralProposal>& proposals,
        const life_orchestrator::core::BehavioralStateSnapshot& snapshot,
        const std::string& source_module_id,
        const life_orchestrator::core::TimestampString& now) const;
};

}  // namespace life_orchestrator::coordination
