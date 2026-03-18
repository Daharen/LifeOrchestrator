#include "core/procedural.hpp"

namespace life_orchestrator::core {
namespace {
#define LO_PROCEDURAL_SWITCH(type, value_name, ...) std::string to_string(type value_name) { switch (value_name) { __VA_ARGS__ } return "Unknown"; }
}

LO_PROCEDURAL_SWITCH(EffortValueClassification, value,
    case EffortValueClassification::HighEffortLowValue: return "HighEffortLowValue";
    case EffortValueClassification::HighEffortHighValue: return "HighEffortHighValue";
    case EffortValueClassification::LowEffortLowValue: return "LowEffortLowValue";
    case EffortValueClassification::LowEffortHighValue: return "LowEffortHighValue";)

LO_PROCEDURAL_SWITCH(OptimizationOpportunityType, value,
    case OptimizationOpportunityType::Automation: return "Automation";
    case OptimizationOpportunityType::Simplification: return "Simplification";
    case OptimizationOpportunityType::Elimination: return "Elimination";
    case OptimizationOpportunityType::Delegation: return "Delegation";
    case OptimizationOpportunityType::StructuralOptimization: return "StructuralOptimization";)

EffortValueClassification effort_value_classification_from_string(const std::string& value) {
    if (value == "HighEffortHighValue") return EffortValueClassification::HighEffortHighValue;
    if (value == "LowEffortLowValue") return EffortValueClassification::LowEffortLowValue;
    if (value == "LowEffortHighValue") return EffortValueClassification::LowEffortHighValue;
    return EffortValueClassification::HighEffortLowValue;
}

OptimizationOpportunityType optimization_opportunity_type_from_string(const std::string& value) {
    if (value == "Simplification") return OptimizationOpportunityType::Simplification;
    if (value == "Elimination") return OptimizationOpportunityType::Elimination;
    if (value == "Delegation") return OptimizationOpportunityType::Delegation;
    if (value == "StructuralOptimization") return OptimizationOpportunityType::StructuralOptimization;
    return OptimizationOpportunityType::Automation;
}

}  // namespace life_orchestrator::core
