#include "app/app_support/memory/scheduling_repository.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_scheduling_candidates(core::MemoryService& memory_service) {
    std::vector<ArtifactEnvelope> artifacts;
    const auto records = memory_service.list_scheduling_candidate_records();
    if (!records.ok || !records.value) return artifacts;
    for (const auto& item : *records.value) {
        artifacts.push_back({"scheduling_candidates",
                             item.candidate_id,
                             {{"candidate_id", item.candidate_id},
                              {"source_intervention_id", item.source_intervention_id},
                              {"source_proposal_id", item.source_proposal_id},
                              {"source_audit_run_id", item.source_audit_run_id},
                              {"source_activity_id", item.source_activity_id},
                              {"estimated_duration_minutes", std::to_string(item.estimated_duration_minutes)},
                              {"urgency", item.urgency},
                              {"scheduling_window_hint", item.scheduling_window_hint},
                              {"recommended_time_of_day", item.recommended_time_of_day},
                              {"recommended_day_span", item.recommended_day_span},
                              {"rationale", item.rationale},
                              {"status", core::to_string(item.status)},
                              {"source_module_id", item.source_module_id},
                              {"created_at", item.created_at},
                              {"updated_at", item.updated_at},
                              {"version", std::to_string(item.version)}},
                             {}});
    }
    return artifacts;
}

std::vector<ArtifactEnvelope> list_schedule_proposals(core::MemoryService& memory_service) {
    std::vector<ArtifactEnvelope> artifacts;
    const auto records = memory_service.list_schedule_proposal_artifacts();
    if (!records.ok || !records.value) return artifacts;
    for (const auto& item : *records.value) {
        artifacts.push_back({"schedule_proposals",
                             item.schedule_proposal_id,
                             {{"schedule_proposal_id", item.schedule_proposal_id},
                              {"source_candidate_id", item.source_candidate_id},
                              {"source_intervention_id", item.source_intervention_id},
                              {"source_proposal_id", item.source_proposal_id},
                              {"source_audit_run_id", item.source_audit_run_id},
                              {"source_activity_id", item.source_activity_id},
                              {"proposed_start_time", item.proposed_start_time},
                              {"proposed_end_time", item.proposed_end_time},
                              {"timezone", item.timezone},
                              {"duration_minutes", std::to_string(item.duration_minutes)},
                              {"scheduling_window_hint", item.scheduling_window_hint},
                              {"recommended_time_of_day", item.recommended_time_of_day},
                              {"rationale", item.rationale},
                              {"proposal_status", core::to_string(item.proposal_status)},
                              {"conflict_status", core::to_string(item.conflict_status)},
                              {"source_module_id", item.source_module_id},
                              {"created_at", item.created_at},
                              {"updated_at", item.updated_at},
                              {"version", std::to_string(item.version)}},
                             {}});
    }
    return artifacts;
}
}  // namespace life_orchestrator::app::memory
