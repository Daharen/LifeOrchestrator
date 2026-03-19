#include "core/behavioral.hpp"

namespace life_orchestrator::core {
namespace {
#define LO_BEHAVIOR_SWITCH(type, value_name, ...) \
std::string to_string(type value_name) { switch (value_name) { __VA_ARGS__ } return "Unknown"; }
}

LO_BEHAVIOR_SWITCH(BehavioralProposalType, value,
    case BehavioralProposalType::HabitChange: return "HabitChange";
    case BehavioralProposalType::Reminder: return "Reminder";
    case BehavioralProposalType::RoutineAdjustment: return "RoutineAdjustment";
    case BehavioralProposalType::ReflectionPrompt: return "ReflectionPrompt";
    case BehavioralProposalType::SchedulingAction: return "SchedulingAction";
    case BehavioralProposalType::AutomationAdoption: return "AutomationAdoption";
    case BehavioralProposalType::GoalAlignmentPrompt: return "GoalAlignmentPrompt";)
LO_BEHAVIOR_SWITCH(BehavioralPriority, value,
    case BehavioralPriority::Low: return "Low";
    case BehavioralPriority::Normal: return "Normal";
    case BehavioralPriority::High: return "High";
    case BehavioralPriority::Critical: return "Critical";)
LO_BEHAVIOR_SWITCH(BehavioralDecisionType, value,
    case BehavioralDecisionType::Approved: return "Approved";
    case BehavioralDecisionType::Deferred: return "Deferred";
    case BehavioralDecisionType::Backlogged: return "Backlogged";
    case BehavioralDecisionType::Rejected: return "Rejected";)
LO_BEHAVIOR_SWITCH(BehavioralCapacityLevel, value,
    case BehavioralCapacityLevel::Low: return "Low";
    case BehavioralCapacityLevel::Medium: return "Medium";
    case BehavioralCapacityLevel::High: return "High";
    case BehavioralCapacityLevel::Recovery: return "Recovery";)
LO_BEHAVIOR_SWITCH(PsychologicalStateLevel, value,
    case PsychologicalStateLevel::Stable: return "Stable";
    case PsychologicalStateLevel::Stressed: return "Stressed";
    case PsychologicalStateLevel::Fatigued: return "Fatigued";
    case PsychologicalStateLevel::Overloaded: return "Overloaded";
    case PsychologicalStateLevel::Recovery: return "Recovery";)
LO_BEHAVIOR_SWITCH(InterventionPresentationMode, value,
    case InterventionPresentationMode::SilentLog: return "SilentLog";
    case InterventionPresentationMode::SuggestivePrompt: return "SuggestivePrompt";
    case InterventionPresentationMode::ScheduledPrompt: return "ScheduledPrompt";
    case InterventionPresentationMode::ImmediatePrompt: return "ImmediatePrompt";)
LO_BEHAVIOR_SWITCH(BacklogStatus, value,
    case BacklogStatus::Pending: return "Pending";
    case BacklogStatus::Reconsidered: return "Reconsidered";
    case BacklogStatus::Approved: return "Approved";
    case BacklogStatus::Rejected: return "Rejected";
    case BacklogStatus::Expired: return "Expired";)
LO_BEHAVIOR_SWITCH(BehavioralOperationType, value,
    case BehavioralOperationType::RecordState: return "RecordState";
    case BehavioralOperationType::TriageProposals: return "TriageProposals";
    case BehavioralOperationType::ListBacklog: return "ListBacklog";
    case BehavioralOperationType::ReevaluateBacklog: return "ReevaluateBacklog";
    case BehavioralOperationType::ListNextInterventions: return "ListNextInterventions";
    case BehavioralOperationType::Status: return "Status";)

BehavioralStateSnapshot default_behavioral_state_snapshot() {
    return {"behavioral-state.default",
            "1970-01-01T00:00:00.000Z",
            "coordination.behavioral_triage",
            0,
            0,
            0.25,
            0.8,
            0.1,
            0.2,
            0.2,
            BehavioralCapacityLevel::Medium,
            PsychologicalStateLevel::Stable,
            "Default deterministic fallback snapshot.",
            1, {}};
}

BehavioralCapacityLevel derive_behavioral_capacity_level(const BehavioralStateSnapshot& snapshot) {
    const bool recovery = snapshot.active_intervention_count >= 3 || snapshot.schedule_density_score >= 0.9 ||
                          snapshot.fatigue_score >= 0.85 || snapshot.stress_score >= 0.85;
    if (recovery) return BehavioralCapacityLevel::Recovery;
    const bool low = snapshot.backlog_count >= 5 || snapshot.schedule_density_score >= 0.7 ||
                     snapshot.recent_compliance_rate < 0.45 || snapshot.recent_failure_frequency >= 0.55 ||
                     snapshot.fatigue_score >= 0.6 || snapshot.stress_score >= 0.65;
    if (low) return BehavioralCapacityLevel::Low;
    const bool high = snapshot.active_intervention_count == 0 && snapshot.backlog_count <= 1 &&
                      snapshot.schedule_density_score <= 0.4 && snapshot.recent_compliance_rate >= 0.75 &&
                      snapshot.recent_failure_frequency <= 0.25 && snapshot.fatigue_score <= 0.35 &&
                      snapshot.stress_score <= 0.35;
    if (high) return BehavioralCapacityLevel::High;
    return BehavioralCapacityLevel::Medium;
}

PsychologicalStateLevel derive_psychological_state_level(const BehavioralStateSnapshot& snapshot) {
    if (snapshot.fatigue_score >= 0.85 || snapshot.stress_score >= 0.85) return PsychologicalStateLevel::Overloaded;
    if (snapshot.fatigue_score >= 0.7) return PsychologicalStateLevel::Fatigued;
    if (snapshot.stress_score >= 0.6) return PsychologicalStateLevel::Stressed;
    if (snapshot.behavioral_capacity_level == BehavioralCapacityLevel::Recovery) return PsychologicalStateLevel::Recovery;
    return PsychologicalStateLevel::Stable;
}

int active_intervention_cap(BehavioralCapacityLevel level) {
    switch (level) {
        case BehavioralCapacityLevel::Low: return 1;
        case BehavioralCapacityLevel::Medium: return 2;
        case BehavioralCapacityLevel::High: return 3;
        case BehavioralCapacityLevel::Recovery: return 0;
    }
    return 1;
}

int effort_gate_for_capacity(BehavioralCapacityLevel level) {
    switch (level) {
        case BehavioralCapacityLevel::Recovery: return 0;
        case BehavioralCapacityLevel::Low: return 2;
        case BehavioralCapacityLevel::Medium: return 5;
        case BehavioralCapacityLevel::High: return 10;
    }
    return 2;
}

int priority_rank(BehavioralPriority priority) {
    switch (priority) {
        case BehavioralPriority::Critical: return 4;
        case BehavioralPriority::High: return 3;
        case BehavioralPriority::Normal: return 2;
        case BehavioralPriority::Low: return 1;
    }
    return 0;
}

BehavioralProposalType behavioral_proposal_type_from_string(const std::string& value) {
    if (value == "HabitChange") return BehavioralProposalType::HabitChange;
    if (value == "Reminder") return BehavioralProposalType::Reminder;
    if (value == "RoutineAdjustment") return BehavioralProposalType::RoutineAdjustment;
    if (value == "ReflectionPrompt") return BehavioralProposalType::ReflectionPrompt;
    if (value == "SchedulingAction") return BehavioralProposalType::SchedulingAction;
    if (value == "AutomationAdoption") return BehavioralProposalType::AutomationAdoption;
    return BehavioralProposalType::GoalAlignmentPrompt;
}
BehavioralPriority behavioral_priority_from_string(const std::string& value) {
    if (value == "Low") return BehavioralPriority::Low;
    if (value == "High") return BehavioralPriority::High;
    if (value == "Critical") return BehavioralPriority::Critical;
    return BehavioralPriority::Normal;
}
BehavioralDecisionType behavioral_decision_type_from_string(const std::string& value) {
    if (value == "Approved") return BehavioralDecisionType::Approved;
    if (value == "Deferred") return BehavioralDecisionType::Deferred;
    if (value == "Backlogged") return BehavioralDecisionType::Backlogged;
    return BehavioralDecisionType::Rejected;
}
BehavioralCapacityLevel behavioral_capacity_level_from_string(const std::string& value) {
    if (value == "Low") return BehavioralCapacityLevel::Low;
    if (value == "High") return BehavioralCapacityLevel::High;
    if (value == "Recovery") return BehavioralCapacityLevel::Recovery;
    return BehavioralCapacityLevel::Medium;
}
PsychologicalStateLevel psychological_state_level_from_string(const std::string& value) {
    if (value == "Stressed") return PsychologicalStateLevel::Stressed;
    if (value == "Fatigued") return PsychologicalStateLevel::Fatigued;
    if (value == "Overloaded") return PsychologicalStateLevel::Overloaded;
    if (value == "Recovery") return PsychologicalStateLevel::Recovery;
    return PsychologicalStateLevel::Stable;
}
InterventionPresentationMode intervention_presentation_mode_from_string(const std::string& value) {
    if (value == "SilentLog") return InterventionPresentationMode::SilentLog;
    if (value == "ScheduledPrompt") return InterventionPresentationMode::ScheduledPrompt;
    if (value == "ImmediatePrompt") return InterventionPresentationMode::ImmediatePrompt;
    return InterventionPresentationMode::SuggestivePrompt;
}
BacklogStatus backlog_status_from_string(const std::string& value) {
    if (value == "Reconsidered") return BacklogStatus::Reconsidered;
    if (value == "Approved") return BacklogStatus::Approved;
    if (value == "Rejected") return BacklogStatus::Rejected;
    if (value == "Expired") return BacklogStatus::Expired;
    return BacklogStatus::Pending;
}
BehavioralOperationType behavioral_operation_type_from_string(const std::string& value) {
    if (value == "RecordState") return BehavioralOperationType::RecordState;
    if (value == "ListBacklog") return BehavioralOperationType::ListBacklog;
    if (value == "ReevaluateBacklog") return BehavioralOperationType::ReevaluateBacklog;
    if (value == "ListNextInterventions") return BehavioralOperationType::ListNextInterventions;
    if (value == "Status") return BehavioralOperationType::Status;
    return BehavioralOperationType::TriageProposals;
}

}  // namespace life_orchestrator::core
