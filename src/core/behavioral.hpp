#pragma once

#include "core/contracts.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace life_orchestrator::core {

using BehavioralProposalId = std::string;
using BehavioralDecisionId = std::string;
using BacklogItemId = std::string;
using BehavioralStateSnapshotId = std::string;
using InterventionId = std::string;
using BehavioralReevaluationId = std::string;

using BehavioralAttributes = std::unordered_map<std::string, std::string>;

// Sprint 4 behavioral triage rule baseline:
// 1. Compute behavioral ROI as expected_benefit / max(1, estimated_behavioral_effort).
// 2. Derive or read the latest capacity level and map it to a fixed active-intervention cap.
// 3. Rank proposals by priority desc, ROI desc, earliest relevant time asc, stable id asc.
// 4. Reject expired proposals, defer valuable but over-capacity proposals, and backlog low-ROI over-capacity proposals.
// 5. Approve only the highest-ranked proposals that fit both the effort gate and remaining active capacity.

enum class BehavioralProposalType {
    HabitChange,
    Reminder,
    RoutineAdjustment,
    ReflectionPrompt,
    SchedulingAction,
    AutomationAdoption,
    GoalAlignmentPrompt
};

enum class BehavioralPriority { Low, Normal, High, Critical };
enum class BehavioralDecisionType { Approved, Deferred, Backlogged, Rejected };
enum class BehavioralCapacityLevel { Low, Medium, High, Recovery };
enum class PsychologicalStateLevel { Stable, Stressed, Fatigued, Overloaded, Recovery };
enum class InterventionPresentationMode { SilentLog, SuggestivePrompt, ScheduledPrompt, ImmediatePrompt };
enum class BacklogStatus { Pending, Reconsidered, Approved, Rejected, Expired };
enum class BehavioralOperationType { RecordState, TriageProposals, ListBacklog, ReevaluateBacklog, ListNextInterventions, Status };

struct BehavioralProposal {
    BehavioralProposalId behavioral_proposal_id;
    BehavioralProposalType proposal_type;
    std::string title;
    std::string description;
    std::string source_module_id;
    std::vector<std::string> related_entity_ids;
    BehavioralPriority priority;
    double estimated_behavioral_effort;
    double expected_benefit;
    int expected_time_cost_minutes;
    InterventionPresentationMode presentation_mode;
    std::optional<TimestampString> earliest_presentation_time;
    std::optional<TimestampString> latest_relevant_time;
    TimestampString created_at;
    TimestampString updated_at;
    std::uint64_t version;
    BehavioralAttributes attributes;
};

struct BehavioralStateSnapshot {
    BehavioralStateSnapshotId behavioral_state_snapshot_id;
    TimestampString captured_at;
    std::string source_module_id;
    int active_intervention_count;
    int backlog_count;
    double schedule_density_score;
    double recent_compliance_rate;
    double recent_failure_frequency;
    double fatigue_score;
    double stress_score;
    BehavioralCapacityLevel behavioral_capacity_level;
    PsychologicalStateLevel psychological_state_level;
    std::string notes;
    std::uint64_t version;
    BehavioralAttributes attributes;
};

struct BehavioralTriageDecision {
    BehavioralDecisionId behavioral_decision_id;
    BehavioralProposalId behavioral_proposal_id;
    BehavioralDecisionType decision_type;
    double behavioral_roi_score;
    std::string capacity_gate_reason;
    std::string priority_reason;
    std::optional<TimestampString> scheduled_for;
    std::optional<InterventionId> created_intervention_id;
    TimestampString decided_at;
    std::string source_module_id;
    std::string summary;
    std::uint64_t version;
};

struct BehavioralBacklogItem {
    BacklogItemId backlog_item_id;
    BehavioralProposalId behavioral_proposal_id;
    BacklogStatus status;
    std::string deferred_reason;
    TimestampString first_deferred_at;
    std::optional<TimestampString> last_reconsidered_at;
    std::optional<TimestampString> reconsider_after;
    std::string source_module_id;
    std::uint64_t version;
    std::string source_proposal_id;
    std::string source_audit_run_id;
    std::string source_activity_id;
    std::string priority;
    std::string effort_estimate;
    std::string rationale;
};

struct BehavioralInterventionRecord {
    InterventionId intervention_id;
    BehavioralProposalId behavioral_proposal_id;
    BehavioralDecisionId behavioral_decision_id;
    std::string title;
    InterventionPresentationMode presentation_mode;
    std::optional<TimestampString> scheduled_for;
    TimestampString created_at;
    std::string status;
    std::string source_module_id;
    std::uint64_t version;
    std::string source_proposal_id;
    std::string source_audit_run_id;
    std::string source_activity_id;
    std::string priority;
    std::string effort_estimate;
    std::string rationale;
};

struct BehavioralReevaluationArtifact {
    BehavioralReevaluationId behavioral_reevaluation_id;
    TimestampString reevaluated_at;
    std::string source_module_id;
    std::size_t backlog_count;
    std::size_t intervention_count;
    std::string source_state_snapshot_id;
    std::string notes_or_rationale;
    std::vector<BacklogItemId> reevaluated_backlog_item_ids;
    std::vector<InterventionId> intervention_ids;
    std::uint64_t version;
};

struct BehavioralMemorySummary {
    std::size_t proposal_count;
    std::size_t state_snapshot_count;
    std::size_t decision_count;
    std::size_t backlog_count;
    std::size_t intervention_count;
    std::size_t reevaluation_count;
};

BehavioralStateSnapshot default_behavioral_state_snapshot();
BehavioralCapacityLevel derive_behavioral_capacity_level(const BehavioralStateSnapshot& snapshot);
PsychologicalStateLevel derive_psychological_state_level(const BehavioralStateSnapshot& snapshot);
int active_intervention_cap(BehavioralCapacityLevel level);
int effort_gate_for_capacity(BehavioralCapacityLevel level);
int priority_rank(BehavioralPriority priority);

std::string to_string(BehavioralProposalType value);
std::string to_string(BehavioralPriority value);
std::string to_string(BehavioralDecisionType value);
std::string to_string(BehavioralCapacityLevel value);
std::string to_string(PsychologicalStateLevel value);
std::string to_string(InterventionPresentationMode value);
std::string to_string(BacklogStatus value);
std::string to_string(BehavioralOperationType value);

BehavioralProposalType behavioral_proposal_type_from_string(const std::string& value);
BehavioralPriority behavioral_priority_from_string(const std::string& value);
BehavioralDecisionType behavioral_decision_type_from_string(const std::string& value);
BehavioralCapacityLevel behavioral_capacity_level_from_string(const std::string& value);
PsychologicalStateLevel psychological_state_level_from_string(const std::string& value);
InterventionPresentationMode intervention_presentation_mode_from_string(const std::string& value);
BacklogStatus backlog_status_from_string(const std::string& value);
BehavioralOperationType behavioral_operation_type_from_string(const std::string& value);

}  // namespace life_orchestrator::core
