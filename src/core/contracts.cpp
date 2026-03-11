#include "core/contracts.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace life_orchestrator::core {
namespace {

template <typename Clock>
TimestampString format_timestamp(typename Clock::time_point time_point) {
    const auto time_t = Clock::to_time_t(time_point);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &time_t);
#else
    gmtime_r(&time_t, &tm);
#endif

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            time_point.time_since_epoch()) %
                        1000;
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3)
        << std::setfill('0') << millis.count() << 'Z';
    return out.str();
}

}  // namespace

std::string to_string(ModuleClass value) {
    switch (value) {
        case ModuleClass::CoreInfrastructure: return "CoreInfrastructure";
        case ModuleClass::Coordination: return "Coordination";
        case ModuleClass::Domain: return "Domain";
        case ModuleClass::Intelligence: return "Intelligence";
        case ModuleClass::Evolution: return "Evolution";
        case ModuleClass::Integration: return "Integration";
    }
    return "Unknown";
}

std::string to_string(RiskTier value) {
    switch (value) {
        case RiskTier::Informational: return "Informational";
        case RiskTier::Suggestive: return "Suggestive";
        case RiskTier::BehavioralRecommendation: return "BehavioralRecommendation";
        case RiskTier::ExternalSystemInteraction: return "ExternalSystemInteraction";
        case RiskTier::HighRiskTransaction: return "HighRiskTransaction";
    }
    return "Unknown";
}

std::string to_string(ExecutionStatus value) {
    switch (value) {
        case ExecutionStatus::Succeeded: return "Succeeded";
        case ExecutionStatus::Failed: return "Failed";
        case ExecutionStatus::Rejected: return "Rejected";
        case ExecutionStatus::NotFound: return "NotFound";
        case ExecutionStatus::InvalidRequest: return "InvalidRequest";
    }
    return "Unknown";
}

std::string to_string(EventCategory value) {
    switch (value) {
        case EventCategory::ModuleRegistered: return "ModuleRegistered";
        case EventCategory::ModuleRegistrationRejected: return "ModuleRegistrationRejected";
        case EventCategory::RequestReceived: return "RequestReceived";
        case EventCategory::RequestValidated: return "RequestValidated";
        case EventCategory::RequestRejected: return "RequestRejected";
        case EventCategory::DispatchStarted: return "DispatchStarted";
        case EventCategory::DispatchCompleted: return "DispatchCompleted";
        case EventCategory::DispatchFailed: return "DispatchFailed";
        case EventCategory::RiskCheckPerformed: return "RiskCheckPerformed";
        case EventCategory::MemoryWriteStarted: return "MemoryWriteStarted";
        case EventCategory::MemoryWriteCompleted: return "MemoryWriteCompleted";
        case EventCategory::MemoryWriteFailed: return "MemoryWriteFailed";
        case EventCategory::MemoryReadPerformed: return "MemoryReadPerformed";
        case EventCategory::MemoryQueryPerformed: return "MemoryQueryPerformed";
        case EventCategory::MemoryLoadStarted: return "MemoryLoadStarted";
        case EventCategory::MemoryLoadCompleted: return "MemoryLoadCompleted";
        case EventCategory::MemoryLoadFailed: return "MemoryLoadFailed";
    }
    return "Unknown";
}

TimestampString current_timestamp_utc() {
    return format_timestamp<std::chrono::system_clock>(std::chrono::system_clock::now());
}

}  // namespace life_orchestrator::core
