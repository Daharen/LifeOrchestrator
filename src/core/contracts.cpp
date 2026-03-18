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

    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(time_point.time_since_epoch()) % 1000;
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << millis.count()
        << 'Z';
    return out.str();
}

}  // namespace

#define LO_SWITCH_TO_STRING(name, value, ...) \
std::string to_string(name value) { \
    switch (value) { __VA_ARGS__ } \
    return "Unknown"; \
}

LO_SWITCH_TO_STRING(ModuleClass, value,
    case ModuleClass::CoreInfrastructure: return "CoreInfrastructure";
    case ModuleClass::Coordination: return "Coordination";
    case ModuleClass::Domain: return "Domain";
    case ModuleClass::Intelligence: return "Intelligence";
    case ModuleClass::Evolution: return "Evolution";
    case ModuleClass::Integration: return "Integration";)

LO_SWITCH_TO_STRING(RiskTier, value,
    case RiskTier::Informational: return "Informational";
    case RiskTier::Suggestive: return "Suggestive";
    case RiskTier::BehavioralRecommendation: return "BehavioralRecommendation";
    case RiskTier::ExternalSystemInteraction: return "ExternalSystemInteraction";
    case RiskTier::HighRiskTransaction: return "HighRiskTransaction";)

LO_SWITCH_TO_STRING(ExecutionStatus, value,
    case ExecutionStatus::Succeeded: return "Succeeded";
    case ExecutionStatus::Failed: return "Failed";
    case ExecutionStatus::Rejected: return "Rejected";
    case ExecutionStatus::NotFound: return "NotFound";
    case ExecutionStatus::InvalidRequest: return "InvalidRequest";)

LO_SWITCH_TO_STRING(EventCategory, value,
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
    case EventCategory::SchedulingOperationStarted: return "SchedulingOperationStarted";
    case EventCategory::SchedulingOperationCompleted: return "SchedulingOperationCompleted";
    case EventCategory::SchedulingOperationFailed: return "SchedulingOperationFailed";
    case EventCategory::SchedulingConflictDetected: return "SchedulingConflictDetected";
    case EventCategory::SchedulingProposalGenerated: return "SchedulingProposalGenerated";
    case EventCategory::SchedulingProposalCommitted: return "SchedulingProposalCommitted";
    case EventCategory::BehavioralStateRecorded: return "BehavioralStateRecorded";
    case EventCategory::BehavioralProposalTriaged: return "BehavioralProposalTriaged";
    case EventCategory::BehavioralDecisionPersisted: return "BehavioralDecisionPersisted";
    case EventCategory::BehavioralBacklogUpdated: return "BehavioralBacklogUpdated";
    case EventCategory::BehavioralInterventionScheduled: return "BehavioralInterventionScheduled";
    case EventCategory::ApplicationBootstrapStarted: return "ApplicationBootstrapStarted";
    case EventCategory::ApplicationBootstrapCompleted: return "ApplicationBootstrapCompleted";
    case EventCategory::ApplicationBootstrapFailed: return "ApplicationBootstrapFailed";
    case EventCategory::ApplicationCommandStarted: return "ApplicationCommandStarted";
    case EventCategory::ApplicationCommandCompleted: return "ApplicationCommandCompleted";
    case EventCategory::ApplicationCommandFailed: return "ApplicationCommandFailed";)

LO_SWITCH_TO_STRING(ScheduleItemType, value,
    case ScheduleItemType::Commitment: return "Commitment";
    case ScheduleItemType::TaskCandidate: return "TaskCandidate";
    case ScheduleItemType::AvailabilityWindow: return "AvailabilityWindow";
    case ScheduleItemType::Proposal: return "Proposal";
    case ScheduleItemType::DecisionRecord: return "DecisionRecord";)

LO_SWITCH_TO_STRING(ScheduleStatus, value,
    case ScheduleStatus::Pending: return "Pending";
    case ScheduleStatus::Scheduled: return "Scheduled";
    case ScheduleStatus::Completed: return "Completed";
    case ScheduleStatus::Cancelled: return "Cancelled";
    case ScheduleStatus::Rejected: return "Rejected";)

LO_SWITCH_TO_STRING(ConflictType, value,
    case ConflictType::Overlap: return "Overlap";
    case ConflictType::OutsideAvailability: return "OutsideAvailability";
    case ConflictType::DurationInsufficient: return "DurationInsufficient";
    case ConflictType::DependencyViolation: return "DependencyViolation";
    case ConflictType::InvalidWindow: return "InvalidWindow";)

LO_SWITCH_TO_STRING(SchedulingPriority, value,
    case SchedulingPriority::Low: return "Low";
    case SchedulingPriority::Normal: return "Normal";
    case SchedulingPriority::High: return "High";
    case SchedulingPriority::Critical: return "Critical";)

LO_SWITCH_TO_STRING(ProposalStatus, value,
    case ProposalStatus::Proposed: return "Proposed";
    case ProposalStatus::Accepted: return "Accepted";
    case ProposalStatus::Rejected: return "Rejected";
    case ProposalStatus::Expired: return "Expired";
    case ProposalStatus::Committed: return "Committed";)

LO_SWITCH_TO_STRING(SchedulingOperationType, value,
    case SchedulingOperationType::AddCommitment: return "AddCommitment";
    case SchedulingOperationType::DetectConflicts: return "DetectConflicts";
    case SchedulingOperationType::ProposeTimeBlocks: return "ProposeTimeBlocks";
    case SchedulingOperationType::CommitProposal: return "CommitProposal";
    case SchedulingOperationType::ListScheduleWindow: return "ListScheduleWindow";)

TimestampString current_timestamp_utc() {
    return format_timestamp<std::chrono::system_clock>(std::chrono::system_clock::now());
}

}  // namespace life_orchestrator::core
