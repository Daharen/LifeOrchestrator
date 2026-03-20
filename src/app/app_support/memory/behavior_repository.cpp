#include "app/app_support/memory/behavior_repository.hpp"

#include "core/behavioral.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_behavioral_backlog(core::MemoryService& memory_service) {
    std::vector<ArtifactEnvelope> artifacts;
    const auto records = memory_service.list_behavioral_backlog_items();
    if (!records.ok || !records.value) return artifacts;
    for (const auto& item : *records.value) {
        artifacts.push_back({"behavioral_backlog",
                             item.backlog_item_id,
                             {{"backlog_item_id", item.backlog_item_id},
                              {"behavioral_proposal_id", item.behavioral_proposal_id},
                              {"status", core::to_string(item.status)},
                              {"deferred_reason", item.deferred_reason},
                              {"first_deferred_at", item.first_deferred_at},
                              {"last_reconsidered_at", item.last_reconsidered_at.value_or("")},
                              {"reconsider_after", item.reconsider_after.value_or("")},
                              {"source_module_id", item.source_module_id},
                              {"version", std::to_string(item.version)},
                              {"source_proposal_id", item.source_proposal_id},
                              {"source_audit_run_id", item.source_audit_run_id},
                              {"source_activity_id", item.source_activity_id},
                              {"priority", item.priority},
                              {"effort_estimate", item.effort_estimate},
                              {"rationale", item.rationale}},
                             {}});
    }
    return artifacts;
}

std::vector<ArtifactEnvelope> list_behavioral_interventions(core::MemoryService& memory_service) {
    std::vector<ArtifactEnvelope> artifacts;
    const auto records = memory_service.list_behavioral_interventions("", std::nullopt);
    if (!records.ok || !records.value) return artifacts;
    for (const auto& item : *records.value) {
        artifacts.push_back({"behavioral_interventions",
                             item.intervention_id,
                             {{"intervention_id", item.intervention_id},
                              {"behavioral_proposal_id", item.behavioral_proposal_id},
                              {"behavioral_decision_id", item.behavioral_decision_id},
                              {"title", item.title},
                              {"presentation_mode", core::to_string(item.presentation_mode)},
                              {"scheduled_for", item.scheduled_for.value_or("")},
                              {"created_at", item.created_at},
                              {"status", item.status},
                              {"source_module_id", item.source_module_id},
                              {"version", std::to_string(item.version)},
                              {"source_proposal_id", item.source_proposal_id},
                              {"source_audit_run_id", item.source_audit_run_id},
                              {"source_activity_id", item.source_activity_id},
                              {"priority", item.priority},
                              {"effort_estimate", item.effort_estimate},
                              {"rationale", item.rationale}},
                             {}});
    }
    return artifacts;
}

std::vector<ArtifactEnvelope> list_behavioral_reevaluations(core::MemoryService& memory_service) {
    std::vector<ArtifactEnvelope> artifacts;
    const auto records = memory_service.list_behavioral_reevaluation_artifacts();
    if (!records.ok || !records.value) return artifacts;
    for (const auto& item : *records.value) {
        core::StringMap backlog_ids;
        for (std::size_t i = 0; i < item.reevaluated_backlog_item_ids.size(); ++i) backlog_ids["item_" + std::to_string(i)] = item.reevaluated_backlog_item_ids[i];
        core::StringMap intervention_ids;
        for (std::size_t i = 0; i < item.intervention_ids.size(); ++i) intervention_ids["item_" + std::to_string(i)] = item.intervention_ids[i];
        artifacts.push_back({"behavioral_reevaluations",
                             item.behavioral_reevaluation_id,
                             {{"behavioral_reevaluation_id", item.behavioral_reevaluation_id},
                              {"reevaluated_at", item.reevaluated_at},
                              {"source_module_id", item.source_module_id},
                              {"backlog_count", std::to_string(item.backlog_count)},
                              {"intervention_count", std::to_string(item.intervention_count)},
                              {"source_state_snapshot_id", item.source_state_snapshot_id},
                              {"notes_or_rationale", item.notes_or_rationale},
                              {"version", std::to_string(item.version)}},
                             {{"reevaluated_backlog_item_ids", backlog_ids}, {"intervention_ids", intervention_ids}}});
    }
    return artifacts;
}
}  // namespace life_orchestrator::app::memory
