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
    RiskCheckPerformed,
    MemoryWriteStarted,
    MemoryWriteCompleted,
    MemoryWriteFailed,
    MemoryReadPerformed,
    MemoryQueryPerformed,
    MemoryLoadStarted,
    MemoryLoadCompleted,
    MemoryLoadFailed,
    SchedulingOperationStarted,
    SchedulingOperationCompleted,
    SchedulingOperationFailed,
    SchedulingConflictDetected,
    SchedulingProposalGenerated,
    SchedulingProposalCommitted
};

using ModuleId = std::string;
using CapabilityId = std::string;
using RequestId = std::string;
using TimestampString = std::string;
using ScheduleItemId = std::string;
using ProposalId = std::string;
using ConstraintSetId = std::string;
using WindowId = std::string;
using ScheduleDecisionId = std::string;

using StringMap = std::unordered_map<std::string, std::string>;

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
    StringMap parameters;
    TimestampString created_at;
};

struct ActionResponse {
    RequestId request_id;
    ExecutionStatus status;
    std::string responding_module_id;
    std::string message;
    StringMap output_data;
    TimestampString completed_at;
};

struct StructuredEvent {
    EventCategory category;
    TimestampString occurred_at;
    std::string request_id;
    std::string module_id;
    std::string capability_id;
    std::string message;
    StringMap fields;
};

enum class ScheduleItemType {
    Commitment,
    TaskCandidate,
    AvailabilityWindow,
    Proposal,
    DecisionRecord
};

enum class ScheduleStatus {
    Pending,
    Scheduled,
    Completed,
    Cancelled,
    Rejected
};

enum class ConflictType {
    Overlap,
    OutsideAvailability,
    DurationInsufficient,
    DependencyViolation,
    InvalidWindow
};

enum class SchedulingPriority {
    Low,
    Normal,
    High,
    Critical
};

enum class ProposalStatus {
    Proposed,
    Accepted,
    Rejected,
    Expired,
    Committed
};

enum class SchedulingOperationType {
    AddCommitment,
    DetectConflicts,
    ProposeTimeBlocks,
    CommitProposal,
    ListScheduleWindow
};

struct ScheduledCommitment {
    ScheduleItemId schedule_item_id;
    std::string related_entity_id;
    std::string title;
    std::string description;
    TimestampString start_time;
    TimestampString end_time;
    std::string timezone;
    SchedulingPriority priority;
    std::string source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
    ScheduleStatus status;
    StringMap attributes;
};

struct SchedulingTaskCandidate {
    ScheduleItemId schedule_item_id;
    std::string related_entity_id;
    std::string title;
    std::string description;
    int estimated_duration_minutes;
    TimestampString earliest_start;
    TimestampString latest_end;
    SchedulingPriority priority;
    bool splittable;
    int required_buffer_before_minutes;
    int required_buffer_after_minutes;
    std::vector<ScheduleItemId> dependency_ids;
    std::string source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
    ScheduleStatus status;
};

struct AvailabilityWindow {
    WindowId window_id;
    std::string title;
    TimestampString start_time;
    TimestampString end_time;
    std::string timezone;
    std::string availability_type;
    std::string recurrence_placeholder;
    std::string source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
};

struct SchedulingConstraintSet {
    ConstraintSetId constraint_set_id;
    int max_commitments_per_day;
    int minimum_gap_minutes;
    bool working_hours_only;
    std::vector<WindowId> allowed_window_ids;
    std::vector<WindowId> blocked_window_ids;
    std::vector<std::string> preference_tags;
    std::string source_module_id;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
};

struct SchedulingConflict {
    std::string conflict_id;
    ConflictType conflict_type;
    ScheduleItemId primary_schedule_item_id;
    ScheduleItemId secondary_schedule_item_id;
    std::string message;
    TimestampString detected_at;
    std::string source_module_id;
    StringMap fields;
};

struct SchedulingProposal {
    ProposalId proposal_id;
    ScheduleItemId related_task_candidate_id;
    TimestampString proposed_start_time;
    TimestampString proposed_end_time;
    std::string timezone;
    int proposal_rank;
    std::string rationale;
    ConstraintSetId based_on_constraint_set_id;
    TimestampString generated_at;
    std::string source_module_id;
    std::uint64_t version;
    ProposalStatus status;
};

struct SchedulingDecisionRecord {
    ScheduleDecisionId decision_id;
    ProposalId proposal_id;
    ScheduleItemId resulting_commitment_id;
    std::string decision_type;
    TimestampString decided_at;
    std::string source_module_id;
    std::string summary;
    std::uint64_t version;
};

std::string to_string(ModuleClass value);
std::string to_string(RiskTier value);
std::string to_string(ExecutionStatus value);
std::string to_string(EventCategory value);
std::string to_string(ScheduleItemType value);
std::string to_string(ScheduleStatus value);
std::string to_string(ConflictType value);
std::string to_string(SchedulingPriority value);
std::string to_string(ProposalStatus value);
std::string to_string(SchedulingOperationType value);
TimestampString current_timestamp_utc();

}  // namespace life_orchestrator::core
