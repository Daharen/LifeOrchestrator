#include "app/app_support/action_form_registry.hpp"

#include <algorithm>
#include <unordered_map>


namespace life_orchestrator::app {
namespace {
const std::vector<ActionFormSpec>& registry() {
    static const std::vector<ActionFormSpec> specs = {
        {"create_activity", "Create Activity", "procedural-upsert-activity", {{"activity_inventory_item_id", "Activity ID", {"--activity-id"}, true, "Stable activity identifier.", "activity.focus"}, {"title", "Title", {"--title"}, true, "Human title shown in inventory.", "Focus block"}, {"domain_source", "Domain Source", {"--domain-source"}, true, "Owning domain or area.", "planning"}, {"frequency", "Frequency", {"--frequency"}, true, "Cadence for the activity.", "daily"}, {"duration_minutes", "Duration Minutes", {"--duration-minutes"}, true, "Estimated duration in minutes.", "45"}, {"effort_estimate", "Effort Estimate", {"--effort-estimate", "--effort"}, true, "Relative effort estimate.", "5"}, {"outcome_value", "Outcome Value", {"--outcome-value"}, true, "Expected value or payoff.", "7"}, {"repeatable", "Repeatable", {"--repeatable"}, false, "Optional repeatability indicator.", "1"}, {"attributes_json", "Attributes JSON", {"--attributes-json"}, false, "Optional flat JSON attribute map.", "{\"necessity\":\"2\"}"}, {"now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z"}}, "procedural-upsert-activity --activity-id activity.focus --title FocusBlock --domain-source planning --frequency daily --duration-minutes 45 --effort-estimate 5 --outcome-value 7", {"activity_inventory"}},
        {"record_behavioral_state", "Record Behavioral State", "behavioral-record-state", {{"available_capacity", "Available Capacity", {"--available-capacity"}, true, "Capacity level from 0-10.", "8"}, {"stress_level", "Stress Level", {"--stress-level"}, true, "Stress level from 0-10.", "2"}, {"cognitive_load", "Cognitive Load", {"--cognitive-load"}, true, "Current cognitive load.", "3"}, {"motivation", "Motivation", {"--motivation-level", "--motivation"}, true, "Motivation level.", "7"}, {"recovery_status", "Recovery Status", {"--recovery-status"}, true, "Recovery readiness.", "8"}, {"sleep_quality", "Sleep Quality", {"--sleep-quality"}, false, "Optional sleep quality override.", "8"}, {"time_pressure", "Time Pressure", {"--time-pressure"}, false, "Optional time pressure override.", "2"}, {"notes", "Notes", {"--notes"}, false, "Optional operator notes.", "steady"}, {"attributes_json", "Attributes JSON", {"--attributes-json"}, false, "Optional flat JSON attribute map.", "{\"operator\":\"console\"}"}, {"now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z"}}, "behavioral-record-state --available-capacity 8 --stress-level 2 --cognitive-load 3 --motivation-level 7 --recovery-status 8", {"behavioral_backlog", "behavioral_interventions"}},
        {"run_procedural_audit", "Run Procedural Audit", "procedural-run-audit", {{"procedural_audit_run_id", "Audit Run ID", {"--audit-run-id"}, false, "Optional stable audit lineage id.", "audit.manual"}, {"now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z"}}, "procedural-run-audit --now 2026-03-19T09:00:00.000Z", {"procedural_proposals"}},
        {"behavioral_reevaluation", "Behavioral Reevaluation", "behavioral-reevaluate-backlog", {{"now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T10:00:00.000Z"}}, "behavioral-reevaluate-backlog --now 2026-03-19T10:00:00.000Z", {"behavioral_reevaluations", "behavioral_backlog", "behavioral_interventions"}},
        {"generate_scheduling_candidates", "Generate Scheduling Candidates", "scheduling-generate-candidates", {{"now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z"}}, "scheduling-generate-candidates --now 2026-03-19T09:00:00.000Z", {"scheduling_candidates"}},
        {"generate_schedule_proposals", "Generate Schedule Proposals", "scheduling-generate-proposals", {{"now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z"}}, "scheduling-generate-proposals --now 2026-03-19T09:00:00.000Z", {"schedule_proposals"}},
        {"update_provider_configuration", "Provider Configuration Update", "integration-set-provider", {{"provider_name", "Provider Name", {"--provider-name"}, true, "Configured provider key.", "openai"}, {"api_key", "API Key", {"--api-key"}, true, "Provider secret; GUI should never redisplay raw value.", "TEST_KEY_123"}, {"model_name", "Model Name", {"--model-name"}, true, "Configured model identifier.", "gpt-5"}}, "integration-set-provider --provider-name openai --api-key TEST_KEY_123 --model-name gpt-5", {"provider_config_summary"}},
        {"provider_readiness_test", "Provider Readiness Test", "integration-test-provider", {{"provider_name", "Provider Name", {"--provider-name"}, false, "Optional provider to probe; defaults to configured provider.", "openai"}}, "integration-test-provider --provider-name openai", {"provider_config_summary"}}};
    return specs;
}
}

const std::vector<ActionFormSpec>& list_action_form_specs() { return registry(); }

std::vector<std::string> list_action_form_ids() {
    std::vector<std::string> ids;
    for (const auto& spec : registry()) ids.push_back(spec.action_id);
    return ids;
}

std::optional<ActionFormSpec> find_action_form_spec_by_id(const std::string& action_id) {
    const auto& specs = registry();
    const auto it = std::find_if(specs.begin(), specs.end(), [&](const auto& spec) { return spec.action_id == action_id; });
    if (it == specs.end()) return std::nullopt;
    return *it;
}

std::optional<ActionFormSpec> find_action_form_spec_by_command_target(const std::string& command_target) {
    const auto& specs = registry();
    const auto it = std::find_if(specs.begin(), specs.end(), [&](const auto& spec) { return spec.canonical_command_target == command_target; });
    if (it == specs.end()) return std::nullopt;
    return *it;
}

ActionFormSubmissionBuildResult build_action_form_submission_args(const ActionFormSpec& spec,
                                                                  const std::vector<ActionFormSubmissionField>& values) {
    std::unordered_map<std::string, std::string> by_field_id;
    for (const auto& value : values) by_field_id[value.field_id] = value.value;

    ActionFormSubmissionBuildResult result;
    result.args.push_back(spec.canonical_command_target);
    for (const auto& field : spec.input_fields) {
        const auto it = by_field_id.find(field.field_id);
        const auto field_value = it == by_field_id.end() ? std::string{} : it->second;
        if (field_value.empty()) {
            if (field.required) result.empty_required_field_ids.push_back(field.field_id);
            continue;
        }
        if (field.accepted_flags.empty()) continue;
        result.args.push_back(field.accepted_flags.front());
        result.args.push_back(field_value);
    }
    return result;
}


}  // namespace life_orchestrator::app
