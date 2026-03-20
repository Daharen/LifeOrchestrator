#include "app/app_support/artifact_presentation_registry.hpp"

#include <algorithm>

namespace life_orchestrator::app {
namespace {
ArtifactActionSpec action(const std::string& id, const std::string& label, const std::string& command) {
    return {id, label, command};
}
ArtifactFieldSpec field(const std::string& key, const std::string& label) { return {key, label}; }

const std::vector<ArtifactPresentationSchema>& registry() {
    static const std::vector<ArtifactPresentationSchema> schemas = {
        {"activity_inventory", "Activity Inventory", {field("title", "Title"), field("domain_source", "Domain"), field("frequency", "Frequency")}, {field("duration_minutes", "Duration Minutes"), field("effort_estimate", "Effort Estimate"), field("outcome_value", "Outcome Value"), field("created_at", "Created At"), field("updated_at", "Updated At"), field("version", "Version")}, {}, "No activity inventory items yet.", {action("create_activity", "Create Activity", "procedural-upsert-activity"), action("run_procedural_audit", "Run Procedural Audit", "procedural-run-audit")}},
        {"procedural_proposals", "Procedural Proposals", {field("proposal_title", "Proposal Title"), field("opportunity_type", "Opportunity Type"), field("triage_status", "Triage Status")}, {field("source_activity_id", "Source Activity"), field("risk_tier", "Risk Tier"), field("time_recovery_minutes", "Time Recovery Minutes"), field("financial_cost_estimate", "Financial Cost Estimate"), field("source_audit_run_id", "Source Audit Run")}, {}, "No procedural optimization proposals yet.", {action("run_procedural_audit", "Run Procedural Audit", "procedural-run-audit"), action("create_activity", "Create Activity", "procedural-upsert-activity")}},
        {"behavioral_backlog", "Behavioral Backlog", {field("item_title", "Title"), field("priority", "Priority"), field("status", "Status")}, {field("source_proposal_id", "Source Proposal"), field("source_audit_run_id", "Source Audit Run"), field("source_activity_id", "Source Activity"), field("rationale", "Rationale")}, {}, "No behavioral backlog items yet.", {action("behavioral_reevaluation", "Behavioral Reevaluation", "behavioral-reevaluate-backlog")}},
        {"behavioral_interventions", "Behavioral Interventions", {field("item_title", "Title"), field("priority", "Priority"), field("status", "Status")}, {field("source_proposal_id", "Source Proposal"), field("source_audit_run_id", "Source Audit Run"), field("source_activity_id", "Source Activity"), field("rationale", "Rationale")}, {}, "No behavioral interventions yet.", {action("record_behavioral_state", "Record Behavioral State", "behavioral-record-state")}},
        {"scheduling_candidates", "Scheduling Candidates", {field("candidate_title", "Candidate Title"), field("recommended_time_of_day", "Recommended Time"), field("recommended_day_span", "Recommended Day Span")}, {field("source_activity_id", "Source Activity"), field("source_intervention_id", "Source Intervention"), field("schedule_density_score", "Schedule Density Score"), field("generated_at", "Generated At")}, {}, "No scheduling candidates yet.", {action("generate_scheduling_candidates", "Generate Scheduling Candidates", "scheduling-generate-candidates"), action("generate_schedule_proposals", "Generate Schedule Proposals", "scheduling-generate-proposals")}},
        {"schedule_proposals", "Schedule Proposals", {field("proposal_title", "Proposal Title"), field("scheduled_day", "Scheduled Day"), field("scheduled_time_of_day", "Scheduled Time")}, {field("source_candidate_id", "Source Candidate"), field("proposal_status", "Proposal Status"), field("generated_at", "Generated At")}, {}, "No schedule proposals yet.", {action("generate_schedule_proposals", "Generate Schedule Proposals", "scheduling-generate-proposals")}},
        {"behavioral_reevaluations", "Behavioral Reevaluations", {field("reevaluated_at", "Reevaluated At"), field("backlog_count", "Backlog Count"), field("intervention_count", "Intervention Count")}, {field("source_state_snapshot_id", "Source State Snapshot"), field("notes_or_rationale", "Notes Or Rationale")}, {}, "No behavioral reevaluation artifacts yet.", {action("behavioral_reevaluation", "Behavioral Reevaluation", "behavioral-reevaluate-backlog")}},
        {"provider_config_summary", "Provider Config Summary", {field("provider_name", "Provider Name"), field("model_name", "Model Name"), field("api_key_redacted", "API Key")}, {field("provider_config_id", "Provider Config ID")}, {}, "No provider configuration is set.", {action("update_provider_configuration", "Provider Configuration Update", "integration-set-provider"), action("provider_readiness_test", "Provider Readiness Test", "integration-test-provider")}}};
    return schemas;
}
}

const std::vector<ArtifactPresentationSchema>& list_artifact_presentation_schemas() { return registry(); }

std::vector<std::string> list_artifact_panel_definition_ids() {
    std::vector<std::string> ids;
    for (const auto& schema : registry()) ids.push_back(schema.artifact_type_key);
    return ids;
}

std::optional<ArtifactPresentationSchema> find_artifact_presentation_schema(const std::string& artifact_type) {
    const auto& schemas = registry();
    const auto it = std::find_if(schemas.begin(), schemas.end(), [&](const auto& schema) { return schema.artifact_type_key == artifact_type; });
    if (it == schemas.end()) return std::nullopt;
    return *it;
}

}  // namespace life_orchestrator::app
