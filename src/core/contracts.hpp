#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace life_orchestrator::core {

enum class ModuleClass {
    CoreInfrastructure,
    Coordination,
    Domain,
    Intelligence,
    Evolution,
    Integration
};

enum class RiskTier : std::uint8_t {
    Informational = 0,
    Suggestive = 1,
    BehavioralRecommendation = 2,
    ExternalSystemInteraction = 3,
    HighRiskTransaction = 4
};

enum class ExecutionStatus {
    Succeeded,
    Failed,
    Rejected,
    NotFound,
    InvalidRequest
};

enum class EventCategory {
    ModuleRegistered,
    ModuleRegistrationRejected,
    RequestReceived,
    RequestValidated,
    RequestRejected,
    DispatchStarted,
    DispatchCompleted,
    DispatchFailed,
    RiskCheckPerformed
};

using ModuleId = std::string;
using CapabilityId = std::string;
using RequestId = std::string;
using TimestampString = std::string;

struct ModuleDescriptor {
    ModuleId module_id;
    std::string module_name;
    ModuleClass module_class;
    std::string capability_description;
    std::vector<CapabilityId> capabilities;
    std::string input_schema_description;
    std::string output_schema_description;
    std::string state_representation_description;
    std::vector<ModuleId> dependencies;
    RiskTier risk_tier;
};

struct ActionRequest {
    RequestId request_id;
    CapabilityId capability_id;
    std::string origin;
    RiskTier requested_risk_tier;
    std::unordered_map<std::string, std::string> parameters;
    TimestampString created_at;
};

struct ActionResponse {
    RequestId request_id;
    ExecutionStatus status;
    std::string responding_module_id;
    std::string message;
    std::unordered_map<std::string, std::string> output_data;
    TimestampString completed_at;
};

struct StructuredEvent {
    EventCategory category;
    TimestampString occurred_at;
    std::string request_id;
    std::string module_id;
    std::string capability_id;
    std::string message;
    std::unordered_map<std::string, std::string> fields;
};

std::string to_string(ModuleClass value);
std::string to_string(RiskTier value);
std::string to_string(ExecutionStatus value);
std::string to_string(EventCategory value);
TimestampString current_timestamp_utc();

}  // namespace life_orchestrator::core
