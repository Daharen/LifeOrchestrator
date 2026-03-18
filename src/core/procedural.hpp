#pragma once

#include "core/contracts.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace life_orchestrator::core {

using ActivityInventoryItemId = std::string;
using ProceduralAuditRunId = std::string;
using OptimizationProposalId = std::string;

using ProceduralAttributes = std::unordered_map<std::string, std::string>;

enum class EffortValueClassification {
    HighEffortLowValue,
    HighEffortHighValue,
    LowEffortLowValue,
    LowEffortHighValue
};

enum class OptimizationOpportunityType {
    Automation,
    Simplification,
    Elimination,
    Delegation,
    StructuralOptimization
};

struct EnergyRecoveryEstimate {
    int recovered_minutes_per_week;
    int recovered_effort_points;
    std::string confidence_label;
};

struct ActivityInventoryItem {
    ActivityInventoryItemId activity_inventory_item_id;
    std::string title;
    std::string description;
    std::string domain_source;
    std::string frequency;
    int duration_minutes;
    int effort_estimate;
    int outcome_value;
    std::string source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
    ProceduralAttributes attributes;
};

struct ProceduralAuditRunRecord {
    ProceduralAuditRunId procedural_audit_run_id;
    std::string source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
    std::size_t activity_count;
    std::size_t generated_proposal_count;
    std::string status;
    std::string summary;
    ProceduralAttributes attributes;
};

struct OptimizationProposalRecord {
    OptimizationProposalId optimization_proposal_id;
    ProceduralAuditRunId procedural_audit_run_id;
    ActivityInventoryItemId activity_inventory_item_id;
    OptimizationOpportunityType opportunity_type;
    EffortValueClassification effort_value_classification;
    EnergyRecoveryEstimate energy_recovery_estimate;
    std::string title;
    std::string rationale;
    std::string source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
    std::string linked_behavioral_proposal_id;
    std::string triage_status;
    std::string triage_decision_id;
    ProceduralAttributes attributes;
};

struct ProceduralMemorySummary {
    std::size_t activity_inventory_count;
    std::size_t audit_run_count;
    std::size_t optimization_proposal_count;
};

std::string to_string(EffortValueClassification value);
std::string to_string(OptimizationOpportunityType value);
EffortValueClassification effort_value_classification_from_string(const std::string& value);
OptimizationOpportunityType optimization_opportunity_type_from_string(const std::string& value);

}  // namespace life_orchestrator::core
