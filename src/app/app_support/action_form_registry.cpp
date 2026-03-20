#include "app/app_support/action_form_registry.hpp"

#include <algorithm>
#include <unordered_map>

namespace life_orchestrator::app {
namespace {
ActionFormFieldSpec field(const std::string& field_id,
                          const std::string& label,
                          std::vector<std::string> accepted_flags,
                          const bool required,
                          const std::string& help_text,
                          const std::string& example_value,
                          const std::string& input_kind = "text",
                          std::vector<ActionFormFieldOptionSpec> options = {},
                          std::vector<ActionFormFieldVisibilityRule> visibility_rules = {}) {
    return {field_id, label, std::move(accepted_flags), required, help_text, example_value, input_kind, std::move(options), std::move(visibility_rules)};
}

const std::vector<ActionFormSpec>& registry() {
    static const std::vector<ActionFormSpec> specs = {
        {"create_activity", "Create Activity", "procedural-upsert-activity", {field("activity_inventory_item_id", "Activity ID", {"--activity-id"}, true, "Stable activity identifier.", "activity.focus"), field("title", "Title", {"--title"}, true, "Human title shown in inventory.", "Focus block"), field("domain_source", "Domain Source", {"--domain-source"}, true, "Owning domain or area.", "planning"), field("frequency", "Frequency", {"--frequency"}, true, "Cadence for the activity.", "daily"), field("duration_minutes", "Duration Minutes", {"--duration-minutes"}, true, "Estimated duration in minutes.", "45"), field("effort_estimate", "Effort Estimate", {"--effort-estimate", "--effort"}, true, "Relative effort estimate.", "5"), field("outcome_value", "Outcome Value", {"--outcome-value"}, true, "Expected value or payoff.", "7"), field("repeatable", "Repeatable", {"--repeatable"}, false, "Optional repeatability indicator.", "1"), field("attributes_json", "Attributes JSON", {"--attributes-json"}, false, "Optional flat JSON attribute map.", "{\"necessity\":\"2\"}"), field("now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z")}, "procedural-upsert-activity --activity-id activity.focus --title FocusBlock --domain-source planning --frequency daily --duration-minutes 45 --effort-estimate 5 --outcome-value 7", {"activity_inventory"}},
        {"record_behavioral_state", "Record Behavioral State", "behavioral-record-state", {field("available_capacity", "Available Capacity", {"--available-capacity"}, true, "Capacity level from 0-10.", "8"), field("stress_level", "Stress Level", {"--stress-level"}, true, "Stress level from 0-10.", "2"), field("cognitive_load", "Cognitive Load", {"--cognitive-load"}, true, "Current cognitive load.", "3"), field("motivation", "Motivation", {"--motivation-level", "--motivation"}, true, "Motivation level.", "7"), field("recovery_status", "Recovery Status", {"--recovery-status"}, true, "Recovery readiness.", "8"), field("sleep_quality", "Sleep Quality", {"--sleep-quality"}, false, "Optional sleep quality override.", "8"), field("time_pressure", "Time Pressure", {"--time-pressure"}, false, "Optional time pressure override.", "2"), field("notes", "Notes", {"--notes"}, false, "Optional operator notes.", "steady"), field("attributes_json", "Attributes JSON", {"--attributes-json"}, false, "Optional flat JSON attribute map.", "{\"operator\":\"console\"}"), field("now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z")}, "behavioral-record-state --available-capacity 8 --stress-level 2 --cognitive-load 3 --motivation-level 7 --recovery-status 8", {"behavioral_backlog", "behavioral_interventions"}},
        {"run_procedural_audit", "Run Procedural Audit", "procedural-run-audit", {field("procedural_audit_run_id", "Audit Run ID", {"--audit-run-id"}, false, "Optional stable audit lineage id.", "audit.manual"), field("now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z")}, "procedural-run-audit --now 2026-03-19T09:00:00.000Z", {"procedural_proposals"}},
        {"behavioral_reevaluation", "Behavioral Reevaluation", "behavioral-reevaluate-backlog", {field("now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T10:00:00.000Z")}, "behavioral-reevaluate-backlog --now 2026-03-19T10:00:00.000Z", {"behavioral_reevaluations", "behavioral_backlog", "behavioral_interventions"}},
        {"generate_scheduling_candidates", "Generate Scheduling Candidates", "scheduling-generate-candidates", {field("now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z")}, "scheduling-generate-candidates --now 2026-03-19T09:00:00.000Z", {"scheduling_candidates"}},
        {"generate_schedule_proposals", "Generate Schedule Proposals", "scheduling-generate-proposals", {field("now", "Timestamp", {"--now"}, false, "Deterministic execution timestamp.", "2026-03-19T09:00:00.000Z")}, "scheduling-generate-proposals --now 2026-03-19T09:00:00.000Z", {"schedule_proposals"}},
        {"update_provider_configuration", "Configure Provider", "integration-set-provider",
         {field("provider_name", "Provider Name", {"--provider-name"}, true, "Configured provider key.", "openai", "dropdown", {{"openai", "OpenAI"}, {"stub", "Stub"}}),
          field("model_name", "Model Name", {"--model-name"}, true, "Configured model identifier.", "gpt-5"),
          field("secret_source", "Secret Source", {"--secret-source"}, true, "Choose whether to save a local API key, use an environment variable, or reuse an existing local secret reference.", "env", "dropdown", {{"direct", "Direct API Key"}, {"env", "Environment Variable"}, {"existing", "Existing Stored Secret Ref"}}),
          field("api_key", "API Key", {"--api-key"}, false, "Provider secret; GUI should never redisplay raw value.", "", "password", {}, {{"secret_source", "direct"}}),
          field("env_var_name", "Environment Variable", {"--env-var"}, false, "Environment variable to resolve at runtime.", "OPENAI_API_KEY", "text", {}, {{"secret_source", "env"}}),
          field("existing_secret_reference", "Existing Secret Reference", {"--secret-ref"}, false, "Reuse an existing local secret file reference when the key is already stored locally.", "config/providers/openai.secret", "text", {}, {{"secret_source", "existing"}})},
         "integration-set-provider --provider-name openai --model-name gpt-5 --secret-source env --env-var OPENAI_API_KEY", {"provider_config_summary"}},
        {"provider_readiness_test", "Provider Readiness Test", "integration-test-provider", {field("provider_name", "Provider Name", {"--provider-name"}, false, "Optional provider to probe; defaults to configured provider.", "openai")}, "integration-test-provider --provider-name openai", {"provider_config_summary"}}};
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
        bool visible = true;
        for (const auto& rule : field.visibility_rules) {
            const auto it = by_field_id.find(rule.controlling_field_id);
            const auto actual = it == by_field_id.end() ? std::string{} : it->second;
            if (actual != rule.expected_value) {
                visible = false;
                break;
            }
        }
        if (!visible) continue;
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
