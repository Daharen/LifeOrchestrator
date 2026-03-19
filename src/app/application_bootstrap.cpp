#include "app/application_bootstrap.hpp"
#include "coordination/scheduling_engine.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace life_orchestrator::app {
namespace {

core::StructuredEvent make_event(core::EventCategory category,
                                 const std::string& message,
                                 const ApplicationBootstrapConfig& config,
                                 std::string capability_id = "application.bootstrap",
                                 core::StringMap fields = {}) {
    fields["application_name"] = config.application_name;
    fields["run_mode"] = to_string(config.run_mode);
    fields["data_root_path"] = config.data_root_path.string();
    return {category,
            core::current_timestamp_utc(),
            "app",
            "app.bootstrap",
            std::move(capability_id),
            std::move(message),
            std::move(fields)};
}

ApplicationRunMode parse_run_mode(const std::string& value) {
    if (value == "status") return ApplicationRunMode::Status;
    if (value == "list-modules") return ApplicationRunMode::ListModules;
    if (value == "schedule-health-check") return ApplicationRunMode::ScheduleHealthCheck;
    if (value == "behavioral-health-check") return ApplicationRunMode::BehavioralHealthCheck;
    if (value == "behavioral-list-backlog") return ApplicationRunMode::BehavioralListBacklog;
    if (value == "behavioral-record-state") return ApplicationRunMode::BehavioralRecordState;
    if (value == "behavioral-list-interventions") return ApplicationRunMode::BehavioralListInterventions;
    if (value == "behavioral-reevaluate-backlog") return ApplicationRunMode::BehavioralReevaluateBacklog;
    if (value == "behavioral-list-reevaluations") return ApplicationRunMode::BehavioralListReevaluations;
    if (value == "behavioral-status") return ApplicationRunMode::BehavioralStatus;
    if (value == "scheduling-generate-candidates") return ApplicationRunMode::SchedulingGenerateCandidates;
    if (value == "scheduling-list-candidates") return ApplicationRunMode::SchedulingListCandidates;
    if (value == "scheduling-generate-proposals") return ApplicationRunMode::SchedulingGenerateProposals;
    if (value == "scheduling-list-proposals") return ApplicationRunMode::SchedulingListProposals;
    if (value == "procedural-health-check") return ApplicationRunMode::ProceduralHealthCheck;
    if (value == "procedural-list-proposals") return ApplicationRunMode::ProceduralListProposals;
    if (value == "procedural-upsert-activity") return ApplicationRunMode::ProceduralUpsertActivity;
    if (value == "procedural-list-activities") return ApplicationRunMode::ProceduralListActivities;
    if (value == "procedural-run-audit") return ApplicationRunMode::ProceduralRunAudit;
    if (value == "procedural-list-audit-runs") return ApplicationRunMode::ProceduralListAuditRuns;
    if (value == "integration-set-provider") return ApplicationRunMode::IntegrationSetProvider;
    if (value == "integration-show-provider") return ApplicationRunMode::IntegrationShowProvider;
    if (value == "integration-list-providers") return ApplicationRunMode::IntegrationListProviders;
    if (value == "integration-test-provider") return ApplicationRunMode::IntegrationTestProvider;
    if (value == "operator-query") return ApplicationRunMode::OperatorQuery;
    if (value == "operator-console") return ApplicationRunMode::OperatorConsole;
    if (value == "help") return ApplicationRunMode::Help;
    if (value == "commands") return ApplicationRunMode::Commands;
    if (value == "aliases") return ApplicationRunMode::Aliases;
    if (value == "suggest") return ApplicationRunMode::Suggest;
    return ApplicationRunMode::BootstrapCheck;
}

std::string run_mode_name(ApplicationRunMode mode) {
    switch (mode) {
        case ApplicationRunMode::Status: return "status";
        case ApplicationRunMode::ListModules: return "list-modules";
        case ApplicationRunMode::ScheduleHealthCheck: return "schedule-health-check";
        case ApplicationRunMode::BehavioralHealthCheck: return "behavioral-health-check";
        case ApplicationRunMode::BehavioralListBacklog: return "behavioral-list-backlog";
        case ApplicationRunMode::BehavioralRecordState: return "behavioral-record-state";
        case ApplicationRunMode::BehavioralListInterventions: return "behavioral-list-interventions";
        case ApplicationRunMode::BehavioralReevaluateBacklog: return "behavioral-reevaluate-backlog";
        case ApplicationRunMode::BehavioralListReevaluations: return "behavioral-list-reevaluations";
        case ApplicationRunMode::BehavioralStatus: return "behavioral-status";
        case ApplicationRunMode::SchedulingGenerateCandidates: return "scheduling-generate-candidates";
        case ApplicationRunMode::SchedulingListCandidates: return "scheduling-list-candidates";
        case ApplicationRunMode::SchedulingGenerateProposals: return "scheduling-generate-proposals";
        case ApplicationRunMode::SchedulingListProposals: return "scheduling-list-proposals";
        case ApplicationRunMode::ProceduralHealthCheck: return "procedural-health-check";
        case ApplicationRunMode::ProceduralListProposals: return "procedural-list-proposals";
        case ApplicationRunMode::ProceduralUpsertActivity: return "procedural-upsert-activity";
        case ApplicationRunMode::ProceduralListActivities: return "procedural-list-activities";
        case ApplicationRunMode::ProceduralRunAudit: return "procedural-run-audit";
        case ApplicationRunMode::ProceduralListAuditRuns: return "procedural-list-audit-runs";
        case ApplicationRunMode::IntegrationSetProvider: return "integration-set-provider";
        case ApplicationRunMode::IntegrationShowProvider: return "integration-show-provider";
        case ApplicationRunMode::IntegrationListProviders: return "integration-list-providers";
        case ApplicationRunMode::IntegrationTestProvider: return "integration-test-provider";
        case ApplicationRunMode::OperatorQuery: return "operator-query";
        case ApplicationRunMode::OperatorConsole: return "operator-console";
        case ApplicationRunMode::Help: return "help";
        case ApplicationRunMode::Commands: return "commands";
        case ApplicationRunMode::Aliases: return "aliases";
        case ApplicationRunMode::Suggest: return "suggest";
        case ApplicationRunMode::BootstrapCheck: return "bootstrap-check";
    }
    return "bootstrap-check";
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string value_after_equals(const std::string& arg) {
    const auto pos = arg.find('=');
    return pos == std::string::npos ? std::string{} : arg.substr(pos + 1);
}

const std::vector<std::string>& command_names() {
    static const std::vector<std::string> commands = {
        "aliases", "behavioral-health-check", "behavioral-list-backlog", "behavioral-list-interventions", "behavioral-list-reevaluations",
        "behavioral-record-state", "behavioral-reevaluate-backlog", "behavioral-status", "bootstrap-check", "commands",
        "help", "integration-list-providers", "integration-set-provider", "integration-show-provider", "integration-test-provider",
        "list-modules", "operator-console", "operator-query", "procedural-health-check", "procedural-list-activities",
        "procedural-list-audit-runs", "procedural-list-proposals", "procedural-run-audit", "procedural-upsert-activity",
        "schedule-health-check", "scheduling-generate-candidates", "scheduling-generate-proposals", "scheduling-list-candidates",
        "scheduling-list-proposals", "status", "suggest"};
    return commands;
}

const std::vector<std::pair<std::string, std::string>>& alias_table() {
    static const std::vector<std::pair<std::string, std::string>> aliases = {
        {"activities", "procedural-list-activities"},
        {"backlog", "behavioral-list-backlog"},
        {"candidates", "scheduling-list-candidates"},
        {"interventions", "behavioral-list-interventions"},
        {"proposals", "procedural-list-proposals"},
        {"schedule-proposals", "scheduling-list-proposals"},
        {"status", "status"}
    };
    return aliases;
}

bool is_command_name(const std::string& value) {
    const auto& commands = command_names();
    return std::find(commands.begin(), commands.end(), value) != commands.end();
}

std::optional<std::string> resolve_alias(const std::string& value) {
    for (const auto& [alias, command] : alias_table()) {
        if (alias == value) return command;
    }
    return std::nullopt;
}

std::vector<std::string> split_console_input(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> tokens;
    std::string token;
    while (input >> token) tokens.push_back(token);
    return tokens;
}

std::string trim_copy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::string provider_config_id_for_name(const std::string& provider_name) { return "provider." + provider_name; }
std::filesystem::path provider_secret_path(const std::filesystem::path& data_root, const std::string& provider_name) {
    return data_root / "config" / "providers" / (provider_name + ".secret");
}
void write_secret_file(const std::filesystem::path& path, const std::string& api_key) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::trunc);
    out << api_key;
}
std::string read_secret_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::string value;
    std::getline(in, value);
    return value;
}
std::string redact_secret(const std::string& value) {
    if (value.empty()) return "unset";
    if (value.size() <= 4) return "****";
    return value.substr(0, 2) + "***" + value.substr(value.size() - 2);
}
std::string sanitize_for_storage(const std::string& value) {
    auto out = value;
    for (char& ch : out) {
        if (ch == ';' || ch == '\n' || ch == '\r') ch = '_';
    }
    return out;
}
std::vector<std::string> deterministic_suggestions(const std::string& partial) {
    std::vector<std::string> exact_commands;
    std::vector<std::string> alias_matches;
    std::vector<std::string> loose_commands;
    for (const auto& command : command_names()) {
        if (command == partial) exact_commands.push_back(command);
        else if (starts_with(command, partial)) exact_commands.push_back(command);
        else if (partial.empty() || command.find(partial) != std::string::npos) loose_commands.push_back(command);
    }
    for (const auto& [alias, command] : alias_table()) {
        if (alias == partial || starts_with(alias, partial) || (!partial.empty() && alias.find(partial) != std::string::npos)) {
            alias_matches.push_back(alias + "=>" + command);
        }
    }
    std::sort(exact_commands.begin(), exact_commands.end());
    std::sort(loose_commands.begin(), loose_commands.end());
    std::sort(alias_matches.begin(), alias_matches.end());
    std::vector<std::string> results;
    results.insert(results.end(), exact_commands.begin(), exact_commands.end());
    results.insert(results.end(), alias_matches.begin(), alias_matches.end());
    results.insert(results.end(), loose_commands.begin(), loose_commands.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());
    return results;
}

bool is_known_global_option(const std::string& value) {
    return starts_with(value, "--data-root=") || starts_with(value, "--timezone=") || starts_with(value, "--command=") || value == "--no-seed-data" || value == "--quiet-startup" || value == "--data-root" || value == "--timezone" || value == "--command";
}

bool is_allowed_command_option(ApplicationRunMode mode, const std::string& key) {
    if (mode == ApplicationRunMode::ProceduralUpsertActivity) {
        static const std::vector<std::string> keys = {"activity-id", "title", "domain-source", "frequency", "duration-minutes", "effort", "outcome-value", "description", "necessity", "cognitive-load", "stress-load", "financial-cost", "attributes-json", "repeatable", "now"};
        return std::find(keys.begin(), keys.end(), key) != keys.end();
    }
    if (mode == ApplicationRunMode::ProceduralRunAudit) {
        return key == "now" || key == "audit-run-id";
    }
    if (mode == ApplicationRunMode::BehavioralRecordState) {
        static const std::vector<std::string> keys = {"available-capacity", "stress-level", "cognitive-load", "motivation", "recovery-status", "sleep-quality", "time-pressure", "notes", "now", "attributes-json"};
        return std::find(keys.begin(), keys.end(), key) != keys.end();
    }
    if (mode == ApplicationRunMode::BehavioralListInterventions) return key == "status" || key == "due-by" || key == "now";
    if (mode == ApplicationRunMode::BehavioralReevaluateBacklog) return key == "now";
    if (mode == ApplicationRunMode::IntegrationSetProvider) return key == "provider-name" || key == "api-key" || key == "model-name";
    if (mode == ApplicationRunMode::IntegrationShowProvider) return key == "provider-name";
    if (mode == ApplicationRunMode::IntegrationTestProvider) return key == "provider-name";
    if (mode == ApplicationRunMode::OperatorQuery) return key == "input";
    if (mode == ApplicationRunMode::Suggest) return key == "input";
    return key == "now";
}

std::string normalize_command_option_key(const std::string& key) {
    if (key == "activity-id") return "activity_inventory_item_id";
    if (key == "domain-source") return "domain_source";
    if (key == "duration-minutes") return "duration_minutes";
    if (key == "effort") return "effort_estimate";
    if (key == "outcome-value") return "outcome_value";
    if (key == "cognitive-load") return "cognitive_load";
    if (key == "stress-load") return "stress_load";
    if (key == "financial-cost") return "financial_cost";
    if (key == "audit-run-id") return "procedural_audit_run_id";
    if (key == "available-capacity") return "available_capacity";
    if (key == "stress-level") return "stress_level";
    if (key == "cognitive-load") return "cognitive_load";
    if (key == "recovery-status") return "recovery_status";
    if (key == "sleep-quality") return "sleep_quality";
    if (key == "time-pressure") return "time_pressure";
    if (key == "due-by") return "due_by";
    if (key == "provider-name") return "provider_name";
    if (key == "api-key") return "api_key";
    if (key == "model-name") return "model_name";
    return key;
}

core::StringMap parse_attributes_json(const std::string& raw) {
    core::StringMap result;
    if (raw.empty()) return result;
    std::string body = raw;
    body.erase(std::remove_if(body.begin(), body.end(), [](unsigned char ch) { return std::isspace(ch); }), body.end());
    if (body.size() < 2 || body.front() != '{' || body.back() != '}') throw std::runtime_error("attributes-json must be a flat JSON object");
    body = body.substr(1, body.size() - 2);
    if (body.empty()) return result;
    std::size_t start = 0;
    while (start < body.size()) {
        auto end = body.find(',', start);
        const auto token = body.substr(start, end == std::string::npos ? body.size() - start : end - start);
        const auto colon = token.find(':');
        if (colon == std::string::npos) throw std::runtime_error("attributes-json entry missing ':'");
        auto key = token.substr(0, colon);
        auto value = token.substr(colon + 1);
        if (key.size() < 2 || key.front() != '"' || key.back() != '"') throw std::runtime_error("attributes-json keys must be strings");
        key = key.substr(1, key.size() - 2);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
        result[key] = value;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return result;
}

void emit_ordered_kv_block(std::ostream& output, const std::vector<std::pair<std::string, std::string>>& fields) {
    for (const auto& [key, value] : fields) output << key << '=' << value << '\n';
}

void emit_kv_block(std::ostream& output, const core::StringMap& fields) {
    std::vector<std::pair<std::string, std::string>> ordered(fields.begin(), fields.end());
    std::sort(ordered.begin(), ordered.end());
    emit_ordered_kv_block(output, ordered);
}

std::string default_if_empty(const std::string& value, const std::string& fallback) {
    return value.empty() ? fallback : value;
}

int safe_parse_int(const std::string& value, int fallback) {
    if (value.empty()) return fallback;
    try {
        return std::stoi(value);
    } catch (...) {
        return fallback;
    }
}

std::string normalize_urgency(const std::string& value) {
    if (value == "Critical" || value == "critical") return "Critical";
    if (value == "High" || value == "high") return "High";
    if (value == "Low" || value == "low") return "Low";
    return "Normal";
}

std::string recommended_time_of_day(const std::string& scheduling_window_hint) {
    if (scheduling_window_hint == "next_24_hours") return "morning";
    if (scheduling_window_hint == "next_3_days") return "midday";
    return "flexible";
}

std::string recommended_day_span(const std::string& scheduling_window_hint) {
    if (scheduling_window_hint == "next_24_hours") return "1";
    if (scheduling_window_hint == "next_3_days") return "3";
    return "7";
}

std::string candidate_id_for_intervention(const core::BehavioralInterventionRecord& intervention) {
    return "candidate." + intervention.intervention_id;
}

std::string date_prefix(const std::string& timestamp) {
    return timestamp.size() >= 10 ? timestamp.substr(0, 10) : std::string{"1970-01-01"};
}

std::string join_timestamp(const std::string& date, const std::string& hhmm) {
    return date + "T" + hhmm + ":00.000Z";
}

std::string preferred_start_hhmm(const std::string& time_of_day) {
    if (time_of_day == "morning") return "09:00";
    if (time_of_day == "midday") return "12:00";
    if (time_of_day == "afternoon") return "15:00";
    if (time_of_day == "evening") return "18:00";
    return "10:00";
}

std::string add_days_same_month(const std::string& timestamp, int days_to_add) {
    if (timestamp.size() < 10) return "1970-01-01T00:00:00.000Z";
    const int day = std::stoi(timestamp.substr(8, 2));
    const int shifted = std::max(1, std::min(28, day + days_to_add));
    std::string updated = timestamp;
    updated.replace(8, 2, (shifted < 10 ? "0" : "") + std::to_string(shifted));
    return updated;
}

core::SchedulingTaskCandidate build_task_candidate_from_record(const core::SchedulingCandidateRecord& candidate) {
    const auto base_date = date_prefix(candidate.created_at);
    const auto start = join_timestamp(base_date, preferred_start_hhmm(default_if_empty(candidate.recommended_time_of_day, "unspecified")));
    const auto span_days = std::max(1, safe_parse_int(candidate.recommended_day_span, candidate.scheduling_window_hint == "next_24_hours" ? 1 : (candidate.scheduling_window_hint == "next_3_days" ? 3 : 7)));
    const auto latest = add_days_same_month(join_timestamp(base_date, "17:00"), span_days - 1);
    return {candidate.candidate_id,
            candidate.source_activity_id,
            candidate.source_intervention_id,
            candidate.rationale,
            std::max(0, candidate.estimated_duration_minutes),
            start,
            latest,
            candidate.urgency == "Critical" ? core::SchedulingPriority::Critical : (candidate.urgency == "High" ? core::SchedulingPriority::High : (candidate.urgency == "Low" ? core::SchedulingPriority::Low : core::SchedulingPriority::Normal)),
            false,
            0,
            0,
            {},
            "coordination.scheduling",
            candidate.created_at,
            candidate.updated_at,
            candidate.version,
            core::ScheduleStatus::Pending};
}

std::string schedule_proposal_artifact_id_for_candidate(const core::SchedulingCandidateRecord& candidate,
                                                        const std::string& start_time) {
    return "schedule_proposal." + candidate.candidate_id + "." + (start_time.empty() ? std::string{"unspecified"} : start_time);
}

core::ScheduleProposalArtifact build_schedule_proposal_artifact(const core::SchedulingCandidateRecord& candidate,
                                                                const std::optional<core::SchedulingProposal>& proposal,
                                                                const std::vector<core::SchedulingConflict>& conflicts,
                                                                const std::string& now) {
    const bool blocked = candidate.status != core::SchedulingCandidateStatus::Candidate || candidate.estimated_duration_minutes <= 0 || !proposal.has_value();
    const bool has_conflict = !conflicts.empty();
    const auto proposal_status = blocked ? core::ScheduleProposalArtifactStatus::Rejected : (has_conflict ? core::ScheduleProposalArtifactStatus::ConflictDetected : core::ScheduleProposalArtifactStatus::Proposed);
    const auto conflict_status = blocked ? core::ScheduleProposalConflictStatus::Blocked : (has_conflict ? core::ScheduleProposalConflictStatus::PotentialConflict : core::ScheduleProposalConflictStatus::None);
    const auto start_time = proposal ? proposal->proposed_start_time : std::string{"unspecified"};
    const auto end_time = proposal ? proposal->proposed_end_time : std::string{"unspecified"};
    return {schedule_proposal_artifact_id_for_candidate(candidate, start_time),
            candidate.candidate_id,
            default_if_empty(candidate.source_intervention_id, "none"),
            default_if_empty(candidate.source_proposal_id, "none"),
            default_if_empty(candidate.source_audit_run_id, "none"),
            default_if_empty(candidate.source_activity_id, "none"),
            start_time,
            end_time,
            proposal ? default_if_empty(proposal->timezone, "UTC") : std::string{"UTC"},
            std::max(0, candidate.estimated_duration_minutes),
            default_if_empty(candidate.scheduling_window_hint, "unspecified"),
            default_if_empty(candidate.recommended_time_of_day, "unspecified"),
            default_if_empty(candidate.rationale, "none"),
            proposal_status,
            conflict_status,
            "coordination.scheduling",
            now,
            now,
            1};
}

core::SchedulingCandidateRecord build_scheduling_candidate(const core::BehavioralInterventionRecord& intervention,
                                                           const std::optional<core::BehavioralBacklogItem>& backlog_item,
                                                           const std::string& now) {
    const auto urgency = normalize_urgency(intervention.priority);
    const auto duration = std::max(0, safe_parse_int(intervention.effort_estimate, 0) * 15);
    const bool has_schedule_signal = intervention.status == "Approved" || intervention.status == "ScheduledPrompt" || intervention.status == "ApprovedForScheduling";
    const bool schedule_prompt = intervention.presentation_mode == core::InterventionPresentationMode::ScheduledPrompt;
    const bool high_urgency = urgency == "Critical" || urgency == "High";
    const auto window_hint = schedule_prompt ? "next_24_hours" : (high_urgency ? "next_3_days" : "unspecified");
    const auto status = has_schedule_signal && (schedule_prompt || high_urgency)
                            ? core::SchedulingCandidateStatus::Candidate
                            : (intervention.status == "Rejected" ? core::SchedulingCandidateStatus::Rejected : core::SchedulingCandidateStatus::Deferred);
    return {candidate_id_for_intervention(intervention),
            intervention.intervention_id,
            default_if_empty(intervention.source_proposal_id, backlog_item ? default_if_empty(backlog_item->source_proposal_id, "none") : "none"),
            default_if_empty(intervention.source_audit_run_id, backlog_item ? default_if_empty(backlog_item->source_audit_run_id, "none") : "none"),
            default_if_empty(intervention.source_activity_id, backlog_item ? default_if_empty(backlog_item->source_activity_id, "none") : "none"),
            duration,
            urgency,
            window_hint,
            recommended_time_of_day(window_hint),
            recommended_day_span(window_hint),
            default_if_empty(intervention.rationale, backlog_item ? default_if_empty(backlog_item->rationale, "none") : "none"),
            status,
            "coordination.scheduling",
            now,
            now,
            1};
}

}  // namespace

ApplicationRuntime::ApplicationRuntime(const ApplicationBootstrapConfig& bootstrap_config)
    : config(bootstrap_config),
      event_logger(bootstrap_config.events_file_path),
      memory_store(bootstrap_config.data_root_path, &event_logger),
      memory_service(memory_store),
      integration_repository(bootstrap_config.data_root_path),
      scheduling_module(std::make_shared<coordination::SchedulingCoordinationModule>(&memory_service)),
      behavioral_module(std::make_shared<coordination::BehavioralTriageModule>(&memory_service)),
      procedural_module(),
      control_plane(module_registry, event_logger) {
    procedural_module = std::make_shared<meta::ProceduralAuditorModule>(&memory_service, &control_plane);
}

ApplicationBootstrapResult resolve_bootstrap_config(const std::vector<std::string>& args,
                                                    const std::string& environment_data_root,
                                                    const std::filesystem::path& working_root) {
    std::filesystem::path data_root = working_root / "runtime";
    if (!environment_data_root.empty()) data_root = environment_data_root;

    std::string command = "status";
    bool command_set = false;
    bool log_startup_summary = true;
    bool allow_seed_data = true;
    std::string timezone = "UTC";
    core::StringMap command_parameters;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (is_command_name(arg)) {
            command = arg;
            command_set = true;
            continue;
        }
        if (starts_with(arg, "--data-root=")) {
            data_root = value_after_equals(arg);
            continue;
        }
        if (arg == "--data-root") {
            if (i + 1 >= args.size()) return {false, ApplicationExitCode::CommandValidationFailure, "Missing value for --data-root", {}};
            data_root = args[++i];
            continue;
        }
        if (starts_with(arg, "--timezone=")) {
            timezone = value_after_equals(arg);
            continue;
        }
        if (arg == "--timezone") {
            if (i + 1 >= args.size()) return {false, ApplicationExitCode::CommandValidationFailure, "Missing value for --timezone", {}};
            timezone = args[++i];
            continue;
        }
        if (arg == "--no-seed-data") {
            allow_seed_data = false;
            continue;
        }
        if (arg == "--quiet-startup") {
            log_startup_summary = false;
            continue;
        }
        if (starts_with(arg, "--command=")) {
            command = value_after_equals(arg);
            command_set = true;
            continue;
        }
        if (arg == "--command") {
            if (i + 1 >= args.size()) return {false, ApplicationExitCode::CommandValidationFailure, "Missing value for --command", {}};
            command = args[++i];
            command_set = true;
            continue;
        }
        if (starts_with(arg, "--")) {
            if (!command_set) return {false, ApplicationExitCode::CommandValidationFailure, "Command-specific argument provided before command: " + arg, {}};
            auto option = arg.substr(2);
            std::string value = "1";
            const auto eq = option.find('=');
            if (eq != std::string::npos) {
                value = option.substr(eq + 1);
                option = option.substr(0, eq);
            } else if (i + 1 < args.size() && !starts_with(args[i + 1], "--") && !is_command_name(args[i + 1])) {
                value = args[++i];
            }
            const auto mode = parse_run_mode(command);
            if (!is_allowed_command_option(mode, option) && !is_known_global_option(arg)) {
                return {false, ApplicationExitCode::CommandValidationFailure, "Unknown argument: --" + option, {}};
            }
            command_parameters[normalize_command_option_key(option)] = value;
            continue;
        }
        if (!command_set) {
            command = arg;
            command_set = true;
            continue;
        }
        return {false, ApplicationExitCode::CommandValidationFailure, "Unexpected positional argument: " + arg, {}};
    }

    if (!is_command_name(command)) {
        return {false, ApplicationExitCode::CommandValidationFailure, "Unknown command: " + command, {}};
    }

    ApplicationBootstrapConfig config{.application_name = "life_orchestrator_app",
                                      .data_root_path = data_root,
                                      .events_file_path = data_root / "events" / "application.ndjson",
                                      .integration_config_root_path = data_root / "memory" / "integration_configuration",
                                      .memory_root_path = data_root / "memory",
                                      .default_timezone = timezone,
                                      .run_mode = parse_run_mode(command),
                                      .allow_seed_data = allow_seed_data,
                                      .log_startup_summary = log_startup_summary,
                                      .command_parameters = command_parameters};
    return {true, ApplicationExitCode::Success, "ok", config};
}

std::vector<std::shared_ptr<modules::IModule>> build_runtime_modules(ApplicationRuntime& runtime) {
    return {runtime.behavioral_module, runtime.procedural_module, runtime.scheduling_module};
}

ApplicationBootstrapResult initialize_runtime(ApplicationRuntime& runtime) {
    runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapStarted, "Application bootstrap started.", runtime.config));
    try {
        std::filesystem::create_directories(runtime.config.data_root_path);
        std::filesystem::create_directories(runtime.config.memory_root_path);
        std::filesystem::create_directories(runtime.config.integration_config_root_path);
        std::filesystem::create_directories(runtime.config.events_file_path.parent_path());

        const auto memory_result = runtime.memory_store.load_from_disk();
        if (!memory_result.ok) return {false, ApplicationExitCode::BootstrapFailure, memory_result.message, runtime.config};
        const auto integration_load = runtime.integration_repository.load();
        if (!integration_load.ok) return {false, ApplicationExitCode::BootstrapFailure, integration_load.message, runtime.config};
        if (runtime.config.allow_seed_data) {
            const auto manifest_result = runtime.integration_repository.persist_manifest();
            if (!manifest_result.ok) return {false, ApplicationExitCode::BootstrapFailure, manifest_result.message, runtime.config};
        }
        for (const auto& module : build_runtime_modules(runtime)) {
            const auto result = runtime.module_registry.register_module(module);
            if (!result.ok) return {false, ApplicationExitCode::BootstrapFailure, result.message, runtime.config};
        }
        runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapCompleted, "Application bootstrap completed.", runtime.config, "application.bootstrap", {{"registered_module_count", std::to_string(runtime.module_registry.all_modules().size())}}));
        return {true, ApplicationExitCode::Success, "ok", runtime.config};
    } catch (const std::exception& e) {
        runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapFailed, e.what(), runtime.config));
        return {false, ApplicationExitCode::BootstrapFailure, e.what(), runtime.config};
    }
}

ApplicationExitCode execute_command(ApplicationRuntime& runtime,
                                    std::ostream& output,
                                    std::ostream& error) {
    runtime.event_logger.append(make_event(core::EventCategory::ApplicationCommandStarted, "Application command started.", runtime.config, "application.command"));
    auto complete = [&](ApplicationExitCode code, const std::string& message) {
        runtime.event_logger.append(make_event(code == ApplicationExitCode::Success ? core::EventCategory::ApplicationCommandCompleted : core::EventCategory::ApplicationCommandFailed,
                                               message,
                                               runtime.config,
                                               "application.command",
                                               {{"exit_code", to_string(code)}}));
        return code;
    };

    auto find_provider_record = [&](const std::string& provider_name) -> std::optional<core::IntegrationConfigurationRecord> {
        if (provider_name.empty()) return std::nullopt;
        return runtime.integration_repository.get_by_id(provider_config_id_for_name(provider_name));
    };

    auto emit_provider_record = [&](const core::IntegrationConfigurationRecord& record) {
        const auto secret = read_secret_file(runtime.config.data_root_path / record.credential_reference);
        output << "provider_name=" << record.integration_id << '\n'
               << "display_name=" << record.display_name << '\n'
               << "enabled=" << (record.enabled ? "true" : "false") << '\n'
               << "status=" << core::to_string(record.status) << '\n'
               << "model_name=" << default_if_empty(record.non_secret_settings.contains("model_name") ? record.non_secret_settings.at("model_name") : std::string{}, "unset") << '\n'
               << "credential_storage_mode=" << core::to_string(record.credential_storage_mode) << '\n'
               << "credential_reference=" << record.credential_reference << '\n'
               << "api_key_redacted=" << redact_secret(secret) << '\n'
               << "updated_at=" << record.updated_at << '\n'
               << "version=" << record.version << '\n';
    };

    auto run_stubbed_provider_request = [&](const core::IntegrationConfigurationRecord& record, const std::string& input) -> std::pair<bool, std::string> {
        const auto secret_path = runtime.config.data_root_path / record.credential_reference;
        const auto api_key = read_secret_file(secret_path);
        if (api_key.empty()) return {false, "provider_not_ready=missing_api_key"};
        const auto model_name = record.non_secret_settings.contains("model_name") ? record.non_secret_settings.at("model_name") : std::string{"unset"};
        if (starts_with(api_key, "TEST_") || starts_with(api_key, "TEST") || record.integration_id == "stub") {
            return {true, "provider_response=stubbed\nprovider_name=" + record.integration_id + "\nmodel_name=" + model_name + "\nresponse_text=Stub response for input: " + input + "\n"};
        }
        return {false, "provider_not_ready=unsupported_live_transport"};
    };

    auto resolve_operator_input = [&](const std::string& raw_input, std::ostream& routed_output, std::ostream& routed_error) -> ApplicationExitCode {
        const auto input = trim_copy(raw_input);
        if (input.empty()) {
            routed_error << "operator_query=failed\nmessage=empty_input\n";
            return ApplicationExitCode::CommandValidationFailure;
        }
        const auto tokens = split_console_input(input);
        if (!tokens.empty()) {
            const auto exact = tokens.front();
            const auto alias = resolve_alias(exact);
            if (is_command_name(exact) || alias.has_value()) {
                std::vector<std::string> forwarded;
                forwarded.push_back(alias.value_or(exact));
                for (std::size_t i = 1; i < tokens.size(); ++i) forwarded.push_back(tokens[i]);
                forwarded.push_back("--data-root=" + runtime.config.data_root_path.string());
                forwarded.push_back("--quiet-startup");
                return static_cast<ApplicationExitCode>(run_application(forwarded, routed_output, routed_error, runtime.config.data_root_path.string(), std::filesystem::current_path()));
            }
        }

        const auto providers = runtime.integration_repository.list_all();
        if (providers.empty()) {
            routed_error << "operator_query=failed\nmessage=no_provider_configured\n";
            return ApplicationExitCode::RuntimeOperationFailure;
        }
        const auto& provider = providers.front();
        routed_output << "operator_query=llm_fallback\n";
        routed_output << "provider_request_provider_name=" << provider.integration_id << '\n';
        routed_output << "provider_request_model_name=" << default_if_empty(provider.non_secret_settings.contains("model_name") ? provider.non_secret_settings.at("model_name") : std::string{}, "unset") << '\n';
        auto [ok, response_text] = run_stubbed_provider_request(provider, input);
        if (!ok) {
            routed_error << "operator_query=failed\nmessage=" << response_text << '\n';
            return ApplicationExitCode::RuntimeOperationFailure;
        }
        routed_output << response_text;
        return ApplicationExitCode::Success;
    };

    switch (runtime.config.run_mode) {
        case ApplicationRunMode::Status: {
            const auto summary = runtime.memory_store.get_memory_summary();
            const auto procedural = runtime.memory_service.get_procedural_memory_summary();
            if (!summary.ok || !procedural.ok) {
                error << "error=" << (summary.ok ? procedural.message : summary.message) << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, "Unable to retrieve status summary.");
            }
            output << "application_name=" << runtime.config.application_name << '\n'
                   << "run_mode=" << to_string(runtime.config.run_mode) << '\n'
                   << "data_root_path=" << runtime.config.data_root_path.string() << '\n'
                   << "events_file_path=" << runtime.config.events_file_path.string() << '\n'
                   << "memory_root_path=" << runtime.config.memory_root_path.string() << '\n'
                   << "integration_config_root_path=" << runtime.config.integration_config_root_path.string() << '\n'
                   << "default_timezone=" << runtime.config.default_timezone << '\n'
                   << "registered_module_count=" << runtime.module_registry.all_modules().size() << '\n'
                   << "memory_available=1\n"
                   << "integration_record_count=" << runtime.integration_repository.list_all().size() << '\n'
                   << "configured_provider_count=" << runtime.integration_repository.list_all().size() << '\n'
                   << "activity_inventory_count=" << procedural.value->activity_inventory_count << '\n'
                   << "procedural_audit_run_count=" << procedural.value->audit_run_count << '\n'
                   << "optimization_proposal_count=" << procedural.value->optimization_proposal_count << '\n'
                   << "behavioral_state_snapshot_count=" << summary.value->behavioral_state_snapshot_count << '\n'
                   << "behavioral_backlog_count=" << summary.value->behavioral_backlog_count << '\n'
                   << "behavioral_intervention_count=" << summary.value->behavioral_intervention_count << '\n'
                   << "scheduling_candidate_count=" << summary.value->scheduling_candidate_count << '\n'
                   << "scheduling_proposal_count=" << summary.value->scheduling_proposal_count << '\n'
                   << "behavioral_reevaluation_artifact_count=" << summary.value->behavioral_reevaluation_artifact_count << '\n';
            return complete(ApplicationExitCode::Success, "Status command completed.");
        }
        case ApplicationRunMode::ListModules: {
            auto modules = runtime.module_registry.all_modules();
            std::sort(modules.begin(), modules.end(), [](const auto& a, const auto& b) { return a->descriptor().module_id < b->descriptor().module_id; });
            for (const auto& module : modules) {
                output << "module_id=" << module->descriptor().module_id << '\n';
                auto capabilities = module->descriptor().capabilities;
                std::sort(capabilities.begin(), capabilities.end());
                for (const auto& capability : capabilities) output << "capability=" << capability << '\n';
            }
            return complete(ApplicationExitCode::Success, "List modules command completed.");
        }
        case ApplicationRunMode::BehavioralHealthCheck: {
            const auto high_state = runtime.control_plane.dispatch({"app-behavior-high-state", "behavioral.record_state", "life_orchestrator_app", core::RiskTier::Suggestive, {{"behavioral_state_snapshot_id", "state.app.high"}, {"active_intervention_count", "0"}, {"backlog_count", "0"}, {"schedule_density_score", "0.2"}, {"recent_compliance_rate", "0.9"}, {"recent_failure_frequency", "0.1"}, {"fatigue_score", "0.2"}, {"stress_score", "0.2"}}, core::current_timestamp_utc()});
            const auto approved = runtime.control_plane.dispatch({"app-behavior-triage-high", "behavioral.triage_proposals", "life_orchestrator_app", core::RiskTier::Suggestive, {{"proposal_count", "1"}, {"proposal_id", "proposal.app.approved"}, {"proposal_type", "HabitChange"}, {"title", "Drink water"}, {"priority", "High"}, {"estimated_behavioral_effort", "1"}, {"expected_benefit", "5"}, {"earliest_presentation_time", "2026-03-18T09:00:00.000Z"}}, core::current_timestamp_utc()});
            const auto low_state = runtime.control_plane.dispatch({"app-behavior-low-state", "behavioral.record_state", "life_orchestrator_app", core::RiskTier::Suggestive, {{"behavioral_state_snapshot_id", "state.app.low"}, {"active_intervention_count", "1"}, {"backlog_count", "4"}, {"schedule_density_score", "0.8"}, {"recent_compliance_rate", "0.4"}, {"recent_failure_frequency", "0.7"}, {"fatigue_score", "0.7"}, {"stress_score", "0.8"}}, core::current_timestamp_utc()});
            const auto deferred = runtime.control_plane.dispatch({"app-behavior-triage-low", "behavioral.triage_proposals", "life_orchestrator_app", core::RiskTier::Suggestive, {{"proposal_count", "1"}, {"proposal_id", "proposal.app.deferred"}, {"proposal_type", "AutomationAdoption"}, {"title", "Automate inbox cleanup"}, {"priority", "High"}, {"estimated_behavioral_effort", "8"}, {"expected_benefit", "16"}, {"earliest_presentation_time", "2026-03-19T09:00:00.000Z"}}, core::current_timestamp_utc()});
            const auto interventions = runtime.control_plane.dispatch({"app-behavior-list-interventions", "behavioral.list_next_interventions", "life_orchestrator_app", core::RiskTier::Suggestive, {{"status", "Approved"}}, core::current_timestamp_utc()});
            const auto backlog = runtime.control_plane.dispatch({"app-behavior-list-backlog", "behavioral.list_backlog", "life_orchestrator_app", core::RiskTier::Suggestive, {}, core::current_timestamp_utc()});
            const auto summary = runtime.memory_service.get_behavioral_memory_summary();
            if (high_state.status != core::ExecutionStatus::Succeeded || approved.status != core::ExecutionStatus::Succeeded || low_state.status != core::ExecutionStatus::Succeeded || deferred.status != core::ExecutionStatus::Succeeded || interventions.status != core::ExecutionStatus::Succeeded || backlog.status != core::ExecutionStatus::Succeeded || !summary.ok) {
                error << "behavioral_health_check=failed\n";
                return complete(ApplicationExitCode::RuntimeOperationFailure, "Behavioral health check failed.");
            }
            runtime.memory_store.persist_to_disk();
            output << "behavioral_health_check=ok\n"
                   << "approved_count=" << approved.output_data.at("approved_count") << '\n'
                   << "backlog_count=" << backlog.output_data.at("backlog_count") << '\n'
                   << "intervention_count=" << interventions.output_data.at("intervention_count") << '\n'
                   << "memory_proposal_count=" << summary.value->proposal_count << '\n';
            return complete(ApplicationExitCode::Success, "Behavioral health check completed.");
        }
        case ApplicationRunMode::BehavioralListBacklog: {
            const auto backlog = runtime.memory_service.list_behavioral_backlog_items();
            if (!backlog.ok) {
                error << "behavioral_list_backlog=failed\nmessage=" << backlog.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, backlog.message);
            }
            output << "behavioral_list_backlog=ok\n"
                   << "backlog_count=" << backlog.value->size() << '\n';
            for (const auto& item : *backlog.value) {
                emit_ordered_kv_block(output, {{"item_id", item.backlog_item_id}, {"source_proposal_id", item.source_proposal_id.empty() ? "none" : item.source_proposal_id}, {"source_audit_run_id", item.source_audit_run_id.empty() ? "none" : item.source_audit_run_id}, {"source_activity_id", item.source_activity_id.empty() ? "none" : item.source_activity_id}, {"priority", item.priority.empty() ? "Normal" : item.priority}, {"status", core::to_string(item.status)}, {"effort_estimate", item.effort_estimate.empty() ? "0" : item.effort_estimate}, {"rationale", item.rationale.empty() ? "none" : item.rationale}});
            }
            return complete(ApplicationExitCode::Success, "Behavioral backlog command completed.");
        }
        case ApplicationRunMode::BehavioralRecordState: {
            static const std::vector<std::string> required = {"available_capacity", "stress_level", "cognitive_load", "motivation", "recovery_status"};
            for (const auto& key : required) {
                if (!runtime.config.command_parameters.contains(key) || runtime.config.command_parameters.at(key).empty()) {
                    error << "behavioral_record_state=failed\nmessage=missing_" << key << '\n';
                    return complete(ApplicationExitCode::CommandValidationFailure, "Missing required argument: " + key);
                }
            }
            try {
                for (const auto& [key, value] : parse_attributes_json(runtime.config.command_parameters.contains("attributes_json") ? runtime.config.command_parameters.at("attributes_json") : std::string{})) runtime.config.command_parameters["attribute." + key] = value;
            } catch (const std::exception& e) {
                error << "behavioral_record_state=failed\nmessage=" << e.what() << '\n';
                return complete(ApplicationExitCode::CommandValidationFailure, e.what());
            }
            const auto now = runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : core::current_timestamp_utc();
            const double available = std::stod(runtime.config.command_parameters.at("available_capacity"));
            const double stress = std::stod(runtime.config.command_parameters.at("stress_level"));
            const double cognitive = std::stod(runtime.config.command_parameters.at("cognitive_load"));
            const double motivation = std::stod(runtime.config.command_parameters.at("motivation"));
            const double recovery = std::stod(runtime.config.command_parameters.at("recovery_status"));
            const double sleep = runtime.config.command_parameters.contains("sleep_quality") ? std::stod(runtime.config.command_parameters.at("sleep_quality")) : recovery;
            const double pressure = runtime.config.command_parameters.contains("time_pressure") ? std::stod(runtime.config.command_parameters.at("time_pressure")) : stress;
            runtime.config.command_parameters["behavioral_state_snapshot_id"] = "state." + now + "." + runtime.config.command_parameters.at("available_capacity") + "." + runtime.config.command_parameters.at("stress_level") + "." + runtime.config.command_parameters.at("cognitive_load") + "." + runtime.config.command_parameters.at("motivation") + "." + runtime.config.command_parameters.at("recovery_status");
            runtime.config.command_parameters["captured_at"] = now;
            runtime.config.command_parameters["decision_time"] = now;
            runtime.config.command_parameters["active_intervention_count"] = std::to_string(std::max(0, static_cast<int>(pressure + stress) - static_cast<int>(available)));
            runtime.config.command_parameters["backlog_count"] = std::to_string(std::max(0, static_cast<int>(cognitive + pressure) - static_cast<int>(motivation)));
            runtime.config.command_parameters["schedule_density_score"] = std::to_string(std::min(1.0, pressure / 10.0));
            runtime.config.command_parameters["recent_compliance_rate"] = std::to_string(std::max(0.0, std::min(1.0, (motivation + recovery) / 20.0)));
            runtime.config.command_parameters["recent_failure_frequency"] = std::to_string(std::max(0.0, std::min(1.0, (stress + cognitive) / 20.0)));
            runtime.config.command_parameters["fatigue_score"] = std::to_string(std::max(0.0, std::min(1.0, 1.0 - (sleep / 10.0))));
            runtime.config.command_parameters["stress_score"] = std::to_string(std::max(0.0, std::min(1.0, stress / 10.0)));
            auto response = runtime.control_plane.dispatch({"app-behavioral-record-state", "behavioral.record_state", runtime.config.application_name, core::RiskTier::Suggestive, runtime.config.command_parameters, now});
            if (response.status != core::ExecutionStatus::Succeeded) {
                error << "behavioral_record_state=failed\nmessage=" << response.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, response.message);
            }
            runtime.memory_store.persist_to_disk();
            output << "behavioral_record_state=ok\n"
                   << "snapshot_id=" << response.output_data.at("snapshot_id") << '\n'
                   << "captured_at=" << now << '\n'
                   << "capacity_level=" << response.output_data.at("capacity_level") << '\n';
            return complete(ApplicationExitCode::Success, "Behavioral record state completed.");
        }
        case ApplicationRunMode::BehavioralListInterventions: {
            const auto interventions = runtime.memory_service.list_behavioral_interventions(runtime.config.command_parameters.contains("status") ? runtime.config.command_parameters.at("status") : std::string{"Approved"}, runtime.config.command_parameters.contains("due_by") ? std::optional<core::TimestampString>(runtime.config.command_parameters.at("due_by")) : std::nullopt);
            if (!interventions.ok) {
                error << "behavioral_list_interventions=failed\nmessage=" << interventions.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, interventions.message);
            }
            output << "behavioral_list_interventions=ok\n"
                   << "intervention_count=" << interventions.value->size() << '\n';
            for (const auto& item : *interventions.value) {
                emit_ordered_kv_block(output, {{"item_id", item.intervention_id}, {"source_proposal_id", item.source_proposal_id.empty() ? "none" : item.source_proposal_id}, {"source_audit_run_id", item.source_audit_run_id.empty() ? "none" : item.source_audit_run_id}, {"source_activity_id", item.source_activity_id.empty() ? "none" : item.source_activity_id}, {"priority", item.priority.empty() ? "Normal" : item.priority}, {"status", item.status.empty() ? "none" : item.status}, {"effort_estimate", item.effort_estimate.empty() ? "0" : item.effort_estimate}, {"rationale", item.rationale.empty() ? "none" : item.rationale}});
            }
            return complete(ApplicationExitCode::Success, "Behavioral list interventions completed.");
        }
        case ApplicationRunMode::BehavioralReevaluateBacklog: {
            const auto now = runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : core::current_timestamp_utc();
            const auto response = runtime.control_plane.dispatch({"app-behavioral-reevaluate", "behavioral.reevaluate_backlog", runtime.config.application_name, core::RiskTier::Suggestive, {{"decision_time", now}}, now});
            if (response.status != core::ExecutionStatus::Succeeded) {
                error << "behavioral_reevaluate_backlog=failed\nmessage=" << response.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, response.message);
            }
            runtime.memory_store.persist_to_disk();
            output << "behavioral_reevaluate_backlog=ok\n"
                   << "backlog_count=" << response.output_data.at("backlog_count") << '\n'
                   << "intervention_count=" << response.output_data.at("intervention_count") << '\n'
                   << "reevaluation_artifact_id=" << response.output_data.at("reevaluation_artifact_id") << '\n'
                   << "backlog_items_reordered=" << response.output_data.at("backlog_items_reordered") << '\n'
                   << "interventions_created=" << response.output_data.at("interventions_created") << '\n'
                   << "interventions_reconciled=" << response.output_data.at("interventions_reconciled") << '\n'
                   << "reevaluated_at=" << response.output_data.at("reevaluated_at") << '\n';
            return complete(ApplicationExitCode::Success, "Behavioral reevaluate backlog completed.");
        }
        case ApplicationRunMode::BehavioralListReevaluations: {
            const auto reevaluations = runtime.memory_service.list_behavioral_reevaluation_artifacts();
            if (!reevaluations.ok) {
                error << "behavioral_list_reevaluations=failed\nmessage=" << reevaluations.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, reevaluations.message);
            }
            output << "behavioral_list_reevaluations=ok\n"
                   << "reevaluation_artifact_count=" << reevaluations.value->size() << '\n';
            for (const auto& item : *reevaluations.value) {
                emit_ordered_kv_block(output, {{"reevaluation_artifact_id", item.behavioral_reevaluation_id},
                                               {"reevaluated_at", item.reevaluated_at.empty() ? "none" : item.reevaluated_at},
                                               {"backlog_count", std::to_string(item.backlog_count)},
                                               {"intervention_count", std::to_string(item.intervention_count)},
                                               {"source_state_snapshot_id", item.source_state_snapshot_id.empty() ? "none" : item.source_state_snapshot_id},
                                               {"notes_or_rationale", item.notes_or_rationale.empty() ? "none" : item.notes_or_rationale}});
            }
            return complete(ApplicationExitCode::Success, "Behavioral list reevaluations completed.");
        }
        case ApplicationRunMode::BehavioralStatus: {
            const auto summary = runtime.memory_service.get_behavioral_memory_summary();
            const auto interventions = runtime.memory_service.list_behavioral_interventions("", std::nullopt);
            const auto backlog = runtime.memory_service.list_behavioral_backlog_items();
            const auto decisions = runtime.memory_service.list_behavioral_proposals();
            if (!summary.ok || !interventions.ok || !backlog.ok || !decisions.ok) {
                error << "behavioral_status=failed\n";
                return complete(ApplicationExitCode::RuntimeOperationFailure, "Behavioral status failed.");
            }
            std::size_t approved=0,deferred=0,rejected=0;
            const auto all_decisions = runtime.memory_service.store().list_behavioral_reevaluation_artifacts();
            (void)all_decisions;
            auto recent = runtime.memory_service.store().list_behavioral_proposals();
            (void)recent;
            for (const auto& item : *interventions.value) if (item.status == "Approved") ++approved;
            for (const auto& item : *backlog.value) { if (core::to_string(item.status) == "Rejected") ++rejected; else ++deferred; }
            output << "behavioral_status=ok\n"
                   << "state_snapshot_count=" << summary.value->state_snapshot_count << '\n'
                   << "backlog_count=" << summary.value->backlog_count << '\n'
                   << "intervention_count=" << summary.value->intervention_count << '\n'
                   << "reevaluation_artifact_count=" << summary.value->reevaluation_count << '\n'
                   << "approved_count=" << approved << '\n'
                   << "deferred_count=" << deferred << '\n'
                   << "rejected_count=" << rejected << '\n';
            return complete(ApplicationExitCode::Success, "Behavioral status completed.");
        }
        case ApplicationRunMode::SchedulingGenerateCandidates: {
            const auto interventions = runtime.memory_service.list_behavioral_interventions("", std::nullopt);
            const auto backlog = runtime.memory_service.list_behavioral_backlog_items();
            if (!interventions.ok || !backlog.ok) {
                error << "scheduling_generate_candidates=failed\n";
                return complete(ApplicationExitCode::RuntimeOperationFailure, "Scheduling candidate generation failed.");
            }
            std::unordered_map<std::string, core::BehavioralBacklogItem> backlog_by_proposal_id;
            for (const auto& item : *backlog.value) backlog_by_proposal_id[item.behavioral_proposal_id] = item;
            std::size_t candidate_count = 0;
            std::size_t deferred_count = 0;
            const auto now = runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : "1970-01-01T00:00:00.000Z";
            for (const auto& intervention : *interventions.value) {
                const auto backlog_it = backlog_by_proposal_id.find(intervention.behavioral_proposal_id);
                const auto existing = runtime.memory_service.get_scheduling_candidate_record_by_id(candidate_id_for_intervention(intervention));
                auto record = build_scheduling_candidate(intervention, backlog_it == backlog_by_proposal_id.end() ? std::nullopt : std::optional<core::BehavioralBacklogItem>(backlog_it->second), existing.ok ? existing.value->created_at : now);
                if (existing.ok) {
                    record.created_at = existing.value->created_at;
                    record.version = existing.value->version;
                    if (existing.value->source_intervention_id == record.source_intervention_id &&
                        existing.value->source_proposal_id == record.source_proposal_id &&
                        existing.value->source_audit_run_id == record.source_audit_run_id &&
                        existing.value->source_activity_id == record.source_activity_id &&
                        existing.value->estimated_duration_minutes == record.estimated_duration_minutes &&
                        existing.value->urgency == record.urgency &&
                        existing.value->scheduling_window_hint == record.scheduling_window_hint &&
                        existing.value->recommended_time_of_day == record.recommended_time_of_day &&
                        existing.value->recommended_day_span == record.recommended_day_span &&
                        existing.value->rationale == record.rationale &&
                        existing.value->status == record.status) {
                        record.updated_at = existing.value->updated_at;
                    } else {
                        record.version = existing.value->version + 1;
                    }
                }
                const auto persist = runtime.memory_service.upsert_scheduling_candidate_record(record);
                if (!persist.ok) {
                    error << "scheduling_generate_candidates=failed\nmessage=" << persist.message << '\n';
                    return complete(ApplicationExitCode::RuntimeOperationFailure, persist.message);
                }
                if (record.status == core::SchedulingCandidateStatus::Candidate) ++candidate_count; else ++deferred_count;
            }
            runtime.memory_store.persist_to_disk();
            output << "scheduling_generate_candidates=ok\n"
                   << "intervention_count=" << interventions.value->size() << '\n'
                   << "candidate_count=" << candidate_count << '\n'
                   << "deferred_count=" << deferred_count << '\n';
            return complete(ApplicationExitCode::Success, "Scheduling candidate generation completed.");
        }
        case ApplicationRunMode::SchedulingListCandidates: {
            const auto candidates = runtime.memory_service.list_scheduling_candidate_records();
            if (!candidates.ok) {
                error << "scheduling_list_candidates=failed\nmessage=" << candidates.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, candidates.message);
            }
            output << "scheduling_list_candidates=ok\n"
                   << "candidate_count=" << candidates.value->size() << '\n';
            for (const auto& candidate : *candidates.value) {
                emit_ordered_kv_block(output, {{"candidate_id", candidate.candidate_id},
                                               {"source_intervention_id", default_if_empty(candidate.source_intervention_id, "none")},
                                               {"source_proposal_id", default_if_empty(candidate.source_proposal_id, "none")},
                                               {"source_audit_run_id", default_if_empty(candidate.source_audit_run_id, "none")},
                                               {"source_activity_id", default_if_empty(candidate.source_activity_id, "none")},
                                               {"estimated_duration_minutes", std::to_string(candidate.estimated_duration_minutes)},
                                               {"urgency", default_if_empty(candidate.urgency, "Normal")},
                                               {"scheduling_window_hint", default_if_empty(candidate.scheduling_window_hint, "unspecified")},
                                               {"recommended_time_of_day", default_if_empty(candidate.recommended_time_of_day, "unspecified")},
                                               {"recommended_day_span", default_if_empty(candidate.recommended_day_span, "unspecified")},
                                               {"rationale", default_if_empty(candidate.rationale, "none")},
                                               {"status", core::to_string(candidate.status)}});
            }
            return complete(ApplicationExitCode::Success, "Scheduling list candidates completed.");
        }
        case ApplicationRunMode::SchedulingGenerateProposals: {
            const auto candidates = runtime.memory_service.list_scheduling_candidate_records();
            if (!candidates.ok) {
                error << "scheduling_generate_proposals=failed\nmessage=" << candidates.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, candidates.message);
            }
            coordination::SchedulingEngine engine;
            const auto now = runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : "1970-01-01T00:00:00.000Z";
            std::size_t proposal_count = 0;
            std::size_t conflict_count = 0;
            for (const auto& candidate : *candidates.value) {
                const auto task_candidate = build_task_candidate_from_record(candidate);
                core::SchedulingConstraintSet constraint_set{"constraint.schedule_proposal.internal", 16, 0, true, {}, {}, {}, "coordination.scheduling", candidate.created_at, candidate.updated_at, 1};
                std::vector<core::SchedulingConflict> conflicts;
                auto generated = engine.generate_proposals(task_candidate, {}, {}, &constraint_set, "coordination.scheduling", candidate.updated_at, &conflicts);
                auto artifact = build_schedule_proposal_artifact(candidate, generated.empty() ? std::nullopt : std::optional<core::SchedulingProposal>(generated.front()), conflicts, now);
                const auto existing = runtime.memory_service.get_schedule_proposal_artifact_by_id(artifact.schedule_proposal_id);
                if (existing.ok) {
                    artifact.created_at = existing.value->created_at;
                    artifact.version = existing.value->version;
                    if (existing.value->source_candidate_id == artifact.source_candidate_id &&
                        existing.value->source_intervention_id == artifact.source_intervention_id &&
                        existing.value->source_proposal_id == artifact.source_proposal_id &&
                        existing.value->source_audit_run_id == artifact.source_audit_run_id &&
                        existing.value->source_activity_id == artifact.source_activity_id &&
                        existing.value->proposed_start_time == artifact.proposed_start_time &&
                        existing.value->proposed_end_time == artifact.proposed_end_time &&
                        existing.value->timezone == artifact.timezone &&
                        existing.value->duration_minutes == artifact.duration_minutes &&
                        existing.value->scheduling_window_hint == artifact.scheduling_window_hint &&
                        existing.value->recommended_time_of_day == artifact.recommended_time_of_day &&
                        existing.value->rationale == artifact.rationale &&
                        existing.value->proposal_status == artifact.proposal_status &&
                        existing.value->conflict_status == artifact.conflict_status) {
                        artifact.updated_at = existing.value->updated_at;
                    } else {
                        artifact.version = existing.value->version + 1;
                    }
                }
                const auto persist = runtime.memory_service.upsert_schedule_proposal_artifact(artifact);
                if (!persist.ok) {
                    error << "scheduling_generate_proposals=failed\nmessage=" << persist.message << '\n';
                    return complete(ApplicationExitCode::RuntimeOperationFailure, persist.message);
                }
                if (artifact.conflict_status != core::ScheduleProposalConflictStatus::None) ++conflict_count;
                ++proposal_count;
            }
            runtime.memory_store.persist_to_disk();
            output << "scheduling_generate_proposals=ok\n"
                   << "candidate_count=" << candidates.value->size() << '\n'
                   << "proposal_count=" << proposal_count << '\n'
                   << "conflict_count=" << conflict_count << '\n';
            return complete(ApplicationExitCode::Success, "Scheduling proposal generation completed.");
        }
        case ApplicationRunMode::SchedulingListProposals: {
            const auto proposals = runtime.memory_service.list_schedule_proposal_artifacts();
            if (!proposals.ok) {
                error << "scheduling_list_proposals=failed\nmessage=" << proposals.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, proposals.message);
            }
            output << "scheduling_list_proposals=ok\n"
                   << "proposal_count=" << proposals.value->size() << '\n';
            for (const auto& proposal : *proposals.value) {
                emit_ordered_kv_block(output, {{"schedule_proposal_id", proposal.schedule_proposal_id},
                                               {"source_candidate_id", default_if_empty(proposal.source_candidate_id, "none")},
                                               {"source_intervention_id", default_if_empty(proposal.source_intervention_id, "none")},
                                               {"source_proposal_id", default_if_empty(proposal.source_proposal_id, "none")},
                                               {"source_audit_run_id", default_if_empty(proposal.source_audit_run_id, "none")},
                                               {"source_activity_id", default_if_empty(proposal.source_activity_id, "none")},
                                               {"proposed_start_time", default_if_empty(proposal.proposed_start_time, "unspecified")},
                                               {"proposed_end_time", default_if_empty(proposal.proposed_end_time, "unspecified")},
                                               {"timezone", default_if_empty(proposal.timezone, "UTC")},
                                               {"duration_minutes", std::to_string(proposal.duration_minutes)},
                                               {"scheduling_window_hint", default_if_empty(proposal.scheduling_window_hint, "unspecified")},
                                               {"recommended_time_of_day", default_if_empty(proposal.recommended_time_of_day, "unspecified")},
                                               {"rationale", default_if_empty(proposal.rationale, "none")},
                                               {"proposal_status", core::to_string(proposal.proposal_status)},
                                               {"conflict_status", core::to_string(proposal.conflict_status)}});
            }
            return complete(ApplicationExitCode::Success, "Scheduling list proposals completed.");
        }
        case ApplicationRunMode::ProceduralHealthCheck: {
            const auto state = runtime.control_plane.dispatch({"app-procedural-state", "behavioral.record_state", "life_orchestrator_app", core::RiskTier::Suggestive, {{"behavioral_state_snapshot_id", "state.procedural.health"}, {"captured_at", "2026-03-18T09:00:00.000Z"}, {"active_intervention_count", "0"}, {"backlog_count", "0"}, {"schedule_density_score", "0.2"}, {"recent_compliance_rate", "0.9"}, {"recent_failure_frequency", "0.1"}, {"fatigue_score", "0.2"}, {"stress_score", "0.2"}, {"decision_time", "2026-03-18T09:00:00.000Z"}}, "2026-03-18T09:00:00.000Z"});
            const auto response = runtime.control_plane.dispatch({"app-procedural-health-check", "procedural.health_check", "life_orchestrator_app", core::RiskTier::Suggestive, {{"now", "2026-03-18T09:00:00.000Z"}}, "2026-03-18T09:00:00.000Z"});
            if (state.status != core::ExecutionStatus::Succeeded || response.status != core::ExecutionStatus::Succeeded) {
                error << "procedural_health_check=failed\n";
                return complete(ApplicationExitCode::RuntimeOperationFailure, "Procedural health check failed.");
            }
            runtime.memory_store.persist_to_disk();
            output << "procedural_health_check=ok\n"
                   << "proposal_count=" << response.output_data.at("proposal_count") << '\n'
                   << "triaged_count=" << response.output_data.at("triaged_count") << '\n'
                   << "first_proposal_id=" << response.output_data.at("first_proposal_id") << '\n';
            return complete(ApplicationExitCode::Success, "Procedural health check completed.");
        }
        case ApplicationRunMode::ProceduralListProposals: {
            const auto proposals = runtime.memory_service.list_optimization_proposal_records();
            if (!proposals.ok) {
                error << "procedural_list_proposals=failed\nmessage=" << proposals.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, proposals.message);
            }
            output << "procedural_list_proposals=ok\n"
                   << "proposal_count=" << proposals.value->size() << '\n';
            for (const auto& proposal : *proposals.value) {
                emit_ordered_kv_block(output, {{"proposal_id", proposal.optimization_proposal_id},
                                               {"source_audit_run_id", proposal.source_audit_run_id},
                                               {"opportunity_type", core::to_string(proposal.opportunity_type)},
                                               {"effort_value_classification", core::to_string(proposal.effort_value_classification)},
                                               {"triage_status", proposal.triage_status},
                                               {"risk_tier", proposal.risk_tier},
                                               {"automation_feasibility", core::to_string(proposal.automation_feasibility)},
                                               {"reliability_estimate", std::to_string(proposal.reliability_estimate)},
                                               {"time_recovery_minutes", std::to_string(proposal.time_recovery_minutes)},
                                               {"cognitive_recovery_score", std::to_string(proposal.cognitive_recovery_score)},
                                               {"stress_recovery_score", std::to_string(proposal.stress_recovery_score)},
                                               {"financial_cost_estimate", std::to_string(proposal.financial_cost_estimate)},
                                               {"marginal_benefit_score", std::to_string(proposal.marginal_benefit_score)},
                                               {"diminishing_return_flag", proposal.diminishing_return_flag ? "true" : "false"}});
            }
            return complete(ApplicationExitCode::Success, "Procedural list proposals completed.");
        }
        case ApplicationRunMode::ProceduralUpsertActivity: {
            try {
                for (const auto& [key, value] : parse_attributes_json(runtime.config.command_parameters.contains("attributes_json") ? runtime.config.command_parameters.at("attributes_json") : std::string{})) {
                    runtime.config.command_parameters["attribute." + key] = value;
                }
            } catch (const std::exception& e) {
                error << "procedural_upsert_activity=failed\nmessage=" << e.what() << '\n';
                return complete(ApplicationExitCode::CommandValidationFailure, e.what());
            }
            static const std::vector<std::string> required = {"activity_inventory_item_id", "title", "domain_source", "frequency", "duration_minutes", "effort_estimate", "outcome_value"};
            for (const auto& key : required) {
                if (!runtime.config.command_parameters.contains(key) || runtime.config.command_parameters.at(key).empty()) {
                    error << "procedural_upsert_activity=failed\nmessage=missing_" << key << '\n';
                    return complete(ApplicationExitCode::CommandValidationFailure, "Missing required argument: " + key);
                }
            }
            auto response = runtime.control_plane.dispatch({"app-procedural-upsert-activity", "procedural.upsert_activity", runtime.config.application_name, core::RiskTier::Suggestive, runtime.config.command_parameters, runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : core::current_timestamp_utc()});
            if (response.status != core::ExecutionStatus::Succeeded) {
                error << "procedural_upsert_activity=failed\nmessage=" << response.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, response.message);
            }
            runtime.memory_store.persist_to_disk();
            output << "procedural_upsert_activity=ok\n"
                   << "activity_id=" << response.output_data.at("activity_inventory_item_id") << '\n'
                   << "version=" << response.output_data.at("version") << '\n';
            return complete(ApplicationExitCode::Success, "Procedural activity upsert completed.");
        }
        case ApplicationRunMode::ProceduralListActivities: {
            const auto activities = runtime.memory_service.list_activity_inventory_items();
            if (!activities.ok) {
                error << "procedural_list_activities=failed\nmessage=" << activities.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, activities.message);
            }
            output << "procedural_list_activities=ok\n"
                   << "activity_count=" << activities.value->size() << '\n';
            for (const auto& activity : *activities.value) {
                emit_kv_block(output, {{"activity_id", activity.activity_inventory_item_id},
                                       {"title", activity.title},
                                       {"domain_source", activity.domain_source},
                                       {"frequency", activity.frequency},
                                       {"duration_minutes", std::to_string(activity.duration_minutes)},
                                       {"effort", std::to_string(activity.effort_estimate)},
                                       {"outcome_value", std::to_string(activity.outcome_value)}});
            }
            return complete(ApplicationExitCode::Success, "Procedural activity list completed.");
        }
        case ApplicationRunMode::ProceduralRunAudit: {
            const auto recent_states = runtime.memory_service.list_recent_behavioral_state_snapshots(1);
            if (!recent_states.ok || !recent_states.value || recent_states.value->empty()) {
                runtime.control_plane.dispatch({"app-procedural-audit-state", "behavioral.record_state", runtime.config.application_name, core::RiskTier::Suggestive, {{"behavioral_state_snapshot_id", "state.procedural.run"}, {"captured_at", runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : "2026-03-18T09:00:00.000Z"}, {"active_intervention_count", "0"}, {"backlog_count", "0"}, {"schedule_density_score", "0.2"}, {"recent_compliance_rate", "0.9"}, {"recent_failure_frequency", "0.1"}, {"fatigue_score", "0.2"}, {"stress_score", "0.2"}, {"decision_time", runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : "2026-03-18T09:00:00.000Z"}}, runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : "2026-03-18T09:00:00.000Z"});
            }
            const auto activity_count = runtime.memory_service.list_activity_inventory_items();
            auto response = runtime.control_plane.dispatch({"app-procedural-run-audit", "procedural.audit_inventory", runtime.config.application_name, core::RiskTier::Suggestive, runtime.config.command_parameters, runtime.config.command_parameters.contains("now") ? runtime.config.command_parameters.at("now") : core::current_timestamp_utc()});
            if (!activity_count.ok || response.status != core::ExecutionStatus::Succeeded) {
                error << "procedural_run_audit=failed\nmessage=" << (activity_count.ok ? response.message : activity_count.message) << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, activity_count.ok ? response.message : activity_count.message);
            }
            runtime.memory_store.persist_to_disk();
            output << "procedural_run_audit=ok\n"
                   << "activity_count=" << activity_count.value->size() << '\n'
                   << "audit_run_id=" << response.output_data.at("procedural_audit_run_id") << '\n'
                   << "proposal_count=" << response.output_data.at("proposal_count") << '\n'
                   << "triaged_count=" << response.output_data.at("triaged_count") << '\n';
            return complete(ApplicationExitCode::Success, "Procedural audit run completed.");
        }
        case ApplicationRunMode::ProceduralListAuditRuns: {
            const auto runs = runtime.memory_service.list_procedural_audit_runs();
            if (!runs.ok) {
                error << "procedural_list_audit_runs=failed\nmessage=" << runs.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, runs.message);
            }
            output << "procedural_list_audit_runs=ok\n"
                   << "audit_run_count=" << runs.value->size() << '\n';
            for (const auto& run : *runs.value) {
                emit_kv_block(output, {{"audit_run_id", run.procedural_audit_run_id},
                                       {"activity_count", std::to_string(run.activity_count)},
                                       {"proposal_count", std::to_string(run.generated_proposal_count)},
                                       {"started_at", run.created_at},
                                       {"completed_at", run.updated_at},
                                       {"status", run.status}});
            }
            return complete(ApplicationExitCode::Success, "Procedural audit run listing completed.");
        }
        case ApplicationRunMode::IntegrationSetProvider: {
            static const std::vector<std::string> required = {"provider_name", "api_key", "model_name"};
            for (const auto& key : required) {
                if (!runtime.config.command_parameters.contains(key) || runtime.config.command_parameters.at(key).empty()) {
                    error << "integration_set_provider=failed\nmessage=missing_" << key << '\n';
                    return complete(ApplicationExitCode::CommandValidationFailure, "Missing required argument: " + key);
                }
            }
            const auto now = core::current_timestamp_utc();
            const auto provider_name = sanitize_for_storage(runtime.config.command_parameters.at("provider_name"));
            const auto secret_relative = std::filesystem::relative(provider_secret_path(runtime.config.data_root_path, provider_name), runtime.config.data_root_path).string();
            write_secret_file(provider_secret_path(runtime.config.data_root_path, provider_name), runtime.config.command_parameters.at("api_key"));
            auto existing = find_provider_record(provider_name);
            core::IntegrationConfigurationRecord record{.integration_config_id = provider_config_id_for_name(provider_name),
                                                        .integration_id = provider_name,
                                                        .display_name = provider_name,
                                                        .enabled = true,
                                                        .status = core::IntegrationStatus::Enabled,
                                                        .capability_visibility = {"operator-query", "integration-test-provider"},
                                                        .connection_diagnostics = {},
                                                        .credential_storage_mode = core::CredentialStorageMode::ExternalSecretReference,
                                                        .credential_reference = secret_relative,
                                                        .non_secret_settings = {{"model_name", sanitize_for_storage(runtime.config.command_parameters.at("model_name"))}},
                                                        .created_at = existing ? existing->created_at : now,
                                                        .updated_at = now,
                                                        .version = existing ? existing->version + 1 : 1};
            const auto upsert_result = runtime.integration_repository.upsert(record);
            if (!upsert_result.ok || !runtime.integration_repository.persist_manifest().ok) {
                error << "integration_set_provider=failed\nmessage=" << upsert_result.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, upsert_result.message);
            }
            output << "integration_set_provider=ok\n"
                   << "provider_name=" << provider_name << '\n'
                   << "model_name=" << record.non_secret_settings.at("model_name") << '\n'
                   << "credential_reference=" << record.credential_reference << '\n'
                   << "api_key_redacted=" << redact_secret(runtime.config.command_parameters.at("api_key")) << '\n'
                   << "version=" << record.version << '\n';
            return complete(ApplicationExitCode::Success, "Integration provider configured.");
        }
        case ApplicationRunMode::IntegrationShowProvider: {
            const auto provider_name = runtime.config.command_parameters.contains("provider_name") ? runtime.config.command_parameters.at("provider_name") : std::string{};
            const auto record = find_provider_record(provider_name);
            if (!record) {
                error << "integration_show_provider=failed\nmessage=provider_not_found\n";
                return complete(ApplicationExitCode::RuntimeOperationFailure, "Provider not found.");
            }
            output << "integration_show_provider=ok\n";
            emit_provider_record(*record);
            return complete(ApplicationExitCode::Success, "Integration provider shown.");
        }
        case ApplicationRunMode::IntegrationListProviders: {
            const auto providers = runtime.integration_repository.list_all();
            output << "integration_list_providers=ok\n"
                   << "provider_count=" << providers.size() << '\n';
            for (const auto& record : providers) {
                const auto secret = read_secret_file(runtime.config.data_root_path / record.credential_reference);
                emit_ordered_kv_block(output, {{"provider_name", record.integration_id},
                                               {"enabled", record.enabled ? "true" : "false"},
                                               {"status", core::to_string(record.status)},
                                               {"model_name", default_if_empty(record.non_secret_settings.contains("model_name") ? record.non_secret_settings.at("model_name") : std::string{}, "unset")},
                                               {"api_key_redacted", redact_secret(secret)}});
            }
            return complete(ApplicationExitCode::Success, "Integration providers listed.");
        }
        case ApplicationRunMode::IntegrationTestProvider: {
            const auto provider_name = runtime.config.command_parameters.contains("provider_name") ? runtime.config.command_parameters.at("provider_name") : (runtime.integration_repository.list_all().empty() ? std::string{} : runtime.integration_repository.list_all().front().integration_id);
            const auto record = find_provider_record(provider_name);
            if (!record) {
                error << "integration_test_provider=failed\nmessage=provider_not_found\n";
                return complete(ApplicationExitCode::RuntimeOperationFailure, "Provider not found.");
            }
            auto [ok, response_text] = run_stubbed_provider_request(*record, "integration readiness check");
            if (!ok) {
                error << "integration_test_provider=failed\nmessage=" << response_text << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, response_text);
            }
            output << "integration_test_provider=ok\nprovider_name=" << record->integration_id << '\n'
                   << "model_name=" << default_if_empty(record->non_secret_settings.contains("model_name") ? record->non_secret_settings.at("model_name") : std::string{}, "unset") << '\n'
                   << "ready=true\ntransport=stub\n";
            return complete(ApplicationExitCode::Success, "Integration provider readiness succeeded.");
        }
        case ApplicationRunMode::OperatorQuery: {
            const auto input = runtime.config.command_parameters.contains("input") ? runtime.config.command_parameters.at("input") : std::string{};
            return complete(resolve_operator_input(input, output, error), "Operator query completed.");
        }
        case ApplicationRunMode::OperatorConsole: {
            output << "operator_console=ok\n"
                   << "prompt=life-orchestrator>\n";
            std::string line;
            while (std::getline(std::cin, line)) {
                const auto trimmed = trim_copy(line);
                if (trimmed == "exit" || trimmed == "quit") {
                    output << "operator_console=bye\n";
                    return complete(ApplicationExitCode::Success, "Operator console exited.");
                }
                if (trimmed.empty()) {
                    output << "prompt=life-orchestrator>\n";
                    continue;
                }
                std::ostringstream loop_out;
                std::ostringstream loop_err;
                const auto code = resolve_operator_input(trimmed, loop_out, loop_err);
                output << loop_out.str();
                error << loop_err.str();
                output << "prompt=life-orchestrator>\n";
                if (code != ApplicationExitCode::Success) error << "operator_console_last_exit_code=" << to_string(code) << '\n';
            }
            return complete(ApplicationExitCode::Success, "Operator console exited on EOF.");
        }
        case ApplicationRunMode::Help:
        case ApplicationRunMode::Commands: {
            output << (runtime.config.run_mode == ApplicationRunMode::Help ? "help=ok\n" : "commands=ok\n")
                   << "command_count=" << command_names().size() << '\n';
            for (const auto& command : command_names()) output << "command=" << command << '\n';
            return complete(ApplicationExitCode::Success, "Command list emitted.");
        }
        case ApplicationRunMode::Aliases: {
            output << "aliases=ok\n"
                   << "alias_count=" << alias_table().size() << '\n';
            for (const auto& [alias, command] : alias_table()) output << "alias=" << alias << ";command=" << command << '\n';
            return complete(ApplicationExitCode::Success, "Aliases emitted.");
        }
        case ApplicationRunMode::Suggest: {
            const auto partial = runtime.config.command_parameters.contains("input") ? runtime.config.command_parameters.at("input") : std::string{};
            const auto suggestions = deterministic_suggestions(partial);
            output << "suggest=ok\n"
                   << "input=" << partial << '\n'
                   << "suggestion_count=" << suggestions.size() << '\n';
            for (const auto& suggestion : suggestions) output << "suggestion=" << suggestion << '\n';
            return complete(ApplicationExitCode::Success, "Suggestions emitted.");
        }
        case ApplicationRunMode::BootstrapCheck: {
            const bool event_log_exists = std::filesystem::exists(runtime.config.events_file_path);
            output << "bootstrap_check=ok\n"
                   << "data_root_exists=" << std::filesystem::exists(runtime.config.data_root_path) << '\n'
                   << "memory_root_exists=" << std::filesystem::exists(runtime.config.memory_root_path) << '\n'
                   << "integration_root_exists=" << std::filesystem::exists(runtime.config.integration_config_root_path) << '\n'
                   << "event_log_exists=" << event_log_exists << '\n';
            return complete(ApplicationExitCode::Success, "Bootstrap check completed.");
        }
        case ApplicationRunMode::ScheduleHealthCheck: {
            core::AvailabilityWindow window{"app.health.window", "Health Window", "2026-03-18T09:00:00.000Z", "2026-03-18T17:00:00.000Z", runtime.config.default_timezone, "focus", "none", runtime.scheduling_module->descriptor().module_id, core::current_timestamp_utc(), core::current_timestamp_utc(), 1};
            runtime.memory_service.upsert_availability_window(window);
            const auto response = runtime.control_plane.dispatch({"app-health-check", "scheduling.propose_time_blocks", "life_orchestrator_app", core::RiskTier::Suggestive, {{"schedule_item_id", "task.app.health"}, {"title", "Health Check Task"}, {"related_entity_id", "entity.app"}, {"estimated_duration_minutes", "30"}, {"earliest_start", "2026-03-18T09:00:00.000Z"}, {"latest_end", "2026-03-18T17:00:00.000Z"}}, core::current_timestamp_utc()});
            if (response.status != core::ExecutionStatus::Succeeded) {
                error << "schedule_health_check=failed\nmessage=" << response.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, response.message);
            }
            runtime.memory_store.persist_to_disk();
            output << "schedule_health_check=ok\n"
                   << "responding_module_id=" << response.responding_module_id << '\n'
                   << "proposal_count=" << response.output_data.at("proposal_count") << '\n'
                   << "first_proposal_id=" << response.output_data.at("first_proposal_id") << '\n';
            return complete(ApplicationExitCode::Success, "Schedule health check completed.");
        }
    }

    error << "error=unsupported_command\n";
    return complete(ApplicationExitCode::RuntimeOperationFailure, "Unsupported command.");
}

int run_application(const std::vector<std::string>& args,
                    std::ostream& output,
                    std::ostream& error,
                    const std::string& environment_data_root,
                    const std::filesystem::path& working_root) {
    const auto resolved = resolve_bootstrap_config(args, environment_data_root, working_root);
    if (!resolved.ok) {
        error << "error=" << resolved.message << '\n';
        return static_cast<int>(resolved.exit_code);
    }

    ApplicationRuntime runtime(resolved.config);
    const auto init = initialize_runtime(runtime);
    if (!init.ok) {
        error << "error=" << init.message << '\n';
        return static_cast<int>(init.exit_code);
    }

    if (runtime.config.log_startup_summary) output << "bootstrap=ok\n";
    return static_cast<int>(execute_command(runtime, output, error));
}

std::string to_string(ApplicationRunMode mode) { return run_mode_name(mode); }
std::string to_string(ApplicationExitCode code) {
    switch (code) {
        case ApplicationExitCode::Success: return "0";
        case ApplicationExitCode::BootstrapFailure: return "1";
        case ApplicationExitCode::CommandValidationFailure: return "2";
        case ApplicationExitCode::RuntimeOperationFailure: return "3";
    }
    return "3";
}

}  // namespace life_orchestrator::app
