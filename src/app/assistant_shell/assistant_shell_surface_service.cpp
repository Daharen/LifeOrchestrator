#include "app/assistant_shell/assistant_shell_surface_service.h"

#include "app/app_support/action_form_registry.hpp"
#include "app/app_support/artifact_presentation_registry.hpp"
#include "intelligence/intent_router.hpp"
#include "integration/inference/inference_transport_contracts.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>

namespace life_orchestrator::app::assistant_shell {
namespace {

std::string trim_copy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::stringstream input(text);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) lines.push_back(line);
    return lines;
}

std::string value_for_key(const std::string& text, const std::string& key) {
    for (const auto& line : split_lines(text)) {
        if (starts_with(line, key + "=")) return line.substr(key.size() + 1);
    }
    return {};
}

std::vector<std::pair<std::string, std::string>> parse_kv_pairs(const std::string& text) {
    std::vector<std::pair<std::string, std::string>> fields;
    for (const auto& line : split_lines(text)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        fields.push_back({line.substr(0, pos), line.substr(pos + 1)});
    }
    return fields;
}

std::string sanitize_line(std::string value) {
    std::replace(value.begin(), value.end(), '\n', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '|', '/');
    return value;
}

AssistantShellMessage make_text_message(const std::string& id,
                                        const std::string& role,
                                        AssistantShellMessageBlockType type,
                                        const std::string& text) {
    return {id, role, {{type, text, false, std::nullopt, std::nullopt, std::nullopt}}};
}

constexpr std::size_t kMaxShellFieldLength = 240;

void emit_shell_diagnostic(const std::string& marker, const std::string& detail = {}) {
    std::clog << marker;
    if (!detail.empty()) std::clog << " detail=" << sanitize_line(life_orchestrator::integration::inference::sanitize_diagnostic_text(detail, 160));
    std::clog << '\n';
}

std::string sanitize_shell_field(std::string value, std::size_t max_length = kMaxShellFieldLength) {
    value = sanitize_line(trim_copy(value));
    if (value.size() > max_length) value = value.substr(0, max_length) + "...";
    return value;
}

bool try_parse_confidence(const std::string& value, double& parsed) {
    const auto trimmed = trim_copy(value);
    if (trimmed.empty()) { parsed = 0.0; return false; }
    try {
        size_t consumed = 0;
        parsed = std::stod(trimmed, &consumed);
        if (consumed != trimmed.size() || !std::isfinite(parsed)) { parsed = 0.0; return false; }
        if (parsed < 0.0) parsed = 0.0;
        if (parsed > 1.0) parsed = 1.0;
        return true;
    } catch (...) {
        parsed = 0.0;
        return false;
    }
}

bool try_parse_bool(const std::string& value, bool& parsed) {
    const auto normalized = lower_copy(trim_copy(value));
    if (normalized == "true" || normalized == "1" || normalized == "yes") { parsed = true; return true; }
    if (normalized == "false" || normalized == "0" || normalized == "no" || normalized.empty()) { parsed = false; return !normalized.empty(); }
    parsed = false;
    return false;
}

std::vector<std::string> parse_closest_commands(const std::string& value) {
    std::vector<std::string> commands;
    std::stringstream input(value);
    std::string token;
    while (std::getline(input, token, ',')) {
        token = sanitize_shell_field(token, 64);
        if (!token.empty()) commands.push_back(token);
    }
    return commands;
}

std::vector<std::string> parse_command_args(const std::string& value) {
    std::vector<std::string> args;
    std::istringstream in(value);
    std::string token;
    while (in >> token) args.push_back(sanitize_shell_field(token, 120));
    return args;
}

AssistantShellMessage make_failure_message(const std::string& message_id,
                                           const std::string& assistant_text,
                                           const std::string& failure_classification,
                                           const std::string& explanation,
                                           bool provider_used) {
    AssistantShellExecutionSummary exec_summary;
    exec_summary.provider_used = provider_used;
    exec_summary.resolution_path = "constrained_intent_routing";
    exec_summary.selected_route = sanitize_shell_field(failure_classification, 64);
    exec_summary.confidence = 0.0;
    exec_summary.confirmation_required = false;
    exec_summary.explanation = sanitize_shell_field(explanation);
    exec_summary.route_rejection_reason = sanitize_shell_field(failure_classification, 64);
    return {message_id,
            "assistant",
            {{AssistantShellMessageBlockType::AssistantResponse, sanitize_shell_field(assistant_text, 320), false, std::nullopt, std::nullopt, std::nullopt},
             {AssistantShellMessageBlockType::ExecutionSummary, "Thinking (Extended)", true, exec_summary, std::nullopt, std::nullopt}}};
}

}  // namespace

AssistantShellSurfaceService::AssistantShellSurfaceService(std::filesystem::path data_root,
                                                           std::filesystem::path working_root,
                                                           std::string environment_data_root)
    : data_root_(std::move(data_root)),
      working_root_(std::move(working_root)),
      environment_data_root_(std::move(environment_data_root)) {}

std::filesystem::path AssistantShellSurfaceService::sessions_root() const {
    return data_root_ / "assistant_shell" / "sessions";
}

std::filesystem::path AssistantShellSurfaceService::session_file_path(const std::string& session_id) const {
    return sessions_root() / (session_id + ".log");
}

std::string AssistantShellSurfaceService::now_string() const {
    return core::current_timestamp_utc();
}

std::string AssistantShellSurfaceService::next_session_id() const {
    return "assistant-shell-" + now_string();
}

AssistantShellSessionSummary AssistantShellSurfaceService::make_session_summary(const std::string& session_id) const {
    const auto now = now_string();
    return {session_id, now, now, "Assistant Session"};
}

std::string AssistantShellSurfaceService::default_session_title(const std::string& first_user_text) const {
    const auto trimmed = trim_copy(first_user_text);
    if (trimmed.empty()) return "Assistant Session";
    return trimmed.substr(0, std::min<std::size_t>(trimmed.size(), 48));
}

AssistantShellStatusSnapshot AssistantShellSurfaceService::build_status_snapshot(const std::string& session_id,
                                                                                  const std::string& last_action_status,
                                                                                  AssistantShellSessionMode mode) const {
    const auto status_result = invoke_application_command({"status", "--data-root=" + data_root_.string(), "--quiet-startup"}, environment_data_root_, working_root_);
    const auto provider_result = invoke_application_command({"integration-list-providers", "--data-root=" + data_root_.string(), "--quiet-startup"}, environment_data_root_, working_root_);
    const auto provider_output = provider_result.standard_output + "\n" + provider_result.standard_error;
    return {status_result.exit_code == 0,
            provider_output.find("provider_count=0") == std::string::npos && provider_result.exit_code == 0,
            load_pending_confirmation(session_id).has_value() ? 1 : 0,
            last_action_status,
            mode};
}

std::optional<AssistantShellArtifactCard> AssistantShellSurfaceService::build_artifact_card(const std::string& artifact_type) const {
    const auto result = invoke_application_command({"artifact.query", "--data-root=" + data_root_.string(), "--quiet-startup", "--artifact-type", artifact_type, "--limit", "1"}, environment_data_root_, working_root_);
    if (result.exit_code != 0) return std::nullopt;
    const auto schema = find_artifact_presentation_schema(artifact_type);
    if (!schema.has_value()) return std::nullopt;
    AssistantShellArtifactCard card;
    card.artifact_type = artifact_type;
    card.artifact_id = value_for_key(result.standard_output, "artifact_id");
    card.title = schema->display_title;
    for (const auto& field : schema->summary_fields) {
        const auto value = value_for_key(result.standard_output, field.field_key);
        if (!value.empty()) card.summary_fields.push_back({field.display_label, value});
    }
    if (card.artifact_id.empty() && card.summary_fields.empty()) return std::nullopt;
    return card;
}

std::vector<AssistantShellToolPanelSection> AssistantShellSurfaceService::build_tool_panel_sections() const {
    std::vector<AssistantShellToolPanelSection> sections;
    auto historical = AssistantShellToolPanelSection{"historical_chats", "Historical Chats", "No prior assistant sessions yet.", {}};
    for (const auto& session : ListSessions()) historical.items.push_back({session.session_id, session.title, session.updated_at, "Ask to reopen or summarize this session."});
    sections.push_back(historical);

    sections.push_back({"links", "Links", "Calendar, reminder, control, and routine links will appear here when available.", {}});

    auto priority = AssistantShellToolPanelSection{"current_ai_priority_lists", "Current AI Priority Lists", "No current priority artifacts are available.", {}};
    if (const auto card = build_artifact_card("behavioral_backlog"); card.has_value()) priority.items.push_back({card->artifact_id, card->title, card->summary_fields.empty() ? std::string{"Backlog artifact ready."} : card->summary_fields.front().second, "Comment on this priority list or ask for revisions."});
    if (const auto card = build_artifact_card("behavioral_interventions"); card.has_value()) priority.items.push_back({card->artifact_id, card->title, card->summary_fields.empty() ? std::string{"Intervention artifact ready."} : card->summary_fields.front().second, "Comment on this priority list or ask for revisions."});
    sections.push_back(priority);

    auto scheduled = AssistantShellToolPanelSection{"scheduled_ai_activities", "Scheduled AI Activities", "No scheduling candidates or schedule proposals are available.", {}};
    if (const auto card = build_artifact_card("scheduling_candidates"); card.has_value()) scheduled.items.push_back({card->artifact_id, card->title, card->summary_fields.empty() ? std::string{"Scheduling candidates ready."} : card->summary_fields.front().second, "Comment on these scheduling candidates or request modifications."});
    if (const auto card = build_artifact_card("schedule_proposals"); card.has_value()) scheduled.items.push_back({card->artifact_id, card->title, card->summary_fields.empty() ? std::string{"Schedule proposals ready."} : card->summary_fields.front().second, "Comment on these schedule proposals or request modifications."});
    sections.push_back(scheduled);

    return sections;
}

void AssistantShellSurfaceService::persist_session(const AssistantShellSessionSummary& summary,
                                                   const std::vector<AssistantShellMessage>& messages,
                                                   const AssistantShellStatusSnapshot& status,
                                                   const std::optional<PendingConfirmationState>& pending_confirmation,
                                                   const AssistantShellPendingAttachmentState& attachments,
                                                   const AssistantShellProviderOperationalState& provider_state) const {
    std::filesystem::create_directories(sessions_root());
    std::ofstream out(session_file_path(summary.session_id), std::ios::trunc);
    out << "session_id=" << sanitize_line(summary.session_id) << '\n';
    out << "created_at=" << sanitize_line(summary.created_at) << '\n';
    out << "updated_at=" << sanitize_line(summary.updated_at) << '\n';
    out << "title=" << sanitize_line(summary.title) << '\n';
    out << "runtime_available=" << (status.runtime_available ? "1" : "0") << '\n';
    out << "provider_ready=" << (status.provider_ready ? "1" : "0") << '\n';
    out << "pending_confirmation_count=" << status.pending_confirmation_count << '\n';
    out << "last_action_status=" << sanitize_line(status.last_action_status) << '\n';
    out << "session_mode=" << to_string(status.session_mode) << '\n';
    out << "last_provider_test_state=" << sanitize_line(provider_state.last_provider_test_state) << '\n';
    out << "last_provider_remediation_guidance_state=" << sanitize_line(provider_state.last_provider_remediation_guidance_state) << '\n';
    out << "last_configured_active_provider_summary=" << sanitize_line(provider_state.last_configured_active_provider_summary) << '\n';
    for (const auto& attachment : attachments.attachments) {
        out << "attachment|" << sanitize_line(attachment.local_path) << '|'
            << sanitize_line(attachment.display_name) << '|'
            << sanitize_line(attachment.size_bytes) << '|'
            << sanitize_line(attachment.attachment_state) << '\n';
    }
    if (pending_confirmation.has_value()) {
        out << "pending_confirmation_id=" << sanitize_line(pending_confirmation->confirmation_id) << '\n';
        out << "pending_confirmation_lineage=" << sanitize_line(pending_confirmation->lineage) << '\n';
        out << "pending_confirmation_args=";
        for (std::size_t i = 0; i < pending_confirmation->execution_args.size(); ++i) {
            if (i > 0) out << ' ';
            out << sanitize_line(pending_confirmation->execution_args[i]);
        }
        out << '\n';
    }
    for (const auto& message : messages) {
        for (const auto& block : message.blocks) {
            out << "message|" << sanitize_line(message.message_id) << '|' << sanitize_line(message.role) << '|' << to_string(block.type) << '|' << sanitize_line(block.text) << '\n';
        }
    }
}

AssistantShellPendingAttachmentState AssistantShellSurfaceService::LoadPendingAttachments(const std::string& session_id) const {
    AssistantShellPendingAttachmentState state;
    std::ifstream in(session_file_path(session_id));
    std::string line;
    while (std::getline(in, line)) {
        if (!starts_with(line, "attachment|")) continue;
        std::stringstream input(line);
        std::string segment;
        std::vector<std::string> parts;
        while (std::getline(input, segment, '|')) parts.push_back(segment);
        if (parts.size() >= 5) state.attachments.push_back({parts[1], parts[2], parts[3], parts[4]});
    }
    return state;
}

AssistantShellProviderOperationalState AssistantShellSurfaceService::load_provider_operational_state(const std::string& session_id) const {
    AssistantShellProviderOperationalState state;
    std::ifstream in(session_file_path(session_id));
    std::string line;
    while (std::getline(in, line)) {
        if (starts_with(line, "last_provider_test_state=")) state.last_provider_test_state = line.substr(25);
        else if (starts_with(line, "last_provider_remediation_guidance_state=")) state.last_provider_remediation_guidance_state = line.substr(39);
        else if (starts_with(line, "last_configured_active_provider_summary=")) state.last_configured_active_provider_summary = line.substr(38);
    }
    return state;
}

std::vector<AssistantShellMessage> AssistantShellSurfaceService::load_session_messages(const std::string& session_id) const {
    std::ifstream in(session_file_path(session_id));
    std::vector<AssistantShellMessage> messages;
    std::string line;
    while (std::getline(in, line)) {
        if (!starts_with(line, "message|")) continue;
        std::stringstream input(line);
        std::string segment;
        std::vector<std::string> parts;
        while (std::getline(input, segment, '|')) parts.push_back(segment);
        if (parts.size() < 5) continue;
        AssistantShellMessageBlockType type = AssistantShellMessageBlockType::AssistantResponse;
        if (parts[3] == "user_text") type = AssistantShellMessageBlockType::UserText;
        else if (parts[3] == "execution_summary") type = AssistantShellMessageBlockType::ExecutionSummary;
        else if (parts[3] == "confirmation") type = AssistantShellMessageBlockType::Confirmation;
        else if (parts[3] == "artifact_card") type = AssistantShellMessageBlockType::ArtifactCard;
        else if (parts[3] == "status_notice") type = AssistantShellMessageBlockType::StatusNotice;
        messages.push_back({parts[1], parts[2], {{type, parts[4], false, std::nullopt, std::nullopt, std::nullopt}}});
    }
    return messages;
}

std::optional<AssistantShellSurfaceService::PendingConfirmationState> AssistantShellSurfaceService::load_pending_confirmation(const std::string& session_id) const {
    std::ifstream in(session_file_path(session_id));
    PendingConfirmationState pending;
    bool saw_id = false;
    std::string line;
    while (std::getline(in, line)) {
        if (starts_with(line, "pending_confirmation_id=")) {
            pending.confirmation_id = line.substr(24);
            saw_id = true;
        } else if (starts_with(line, "pending_confirmation_lineage=")) {
            pending.lineage = line.substr(29);
        } else if (starts_with(line, "pending_confirmation_args=")) {
            std::istringstream args(line.substr(26));
            std::string token;
            while (args >> token) pending.execution_args.push_back(token);
        }
    }
    if (!saw_id) return std::nullopt;
    return pending;
}

std::vector<AssistantShellSessionSummary> AssistantShellSurfaceService::ListSessions() const {
    std::vector<AssistantShellSessionSummary> sessions;
    if (!std::filesystem::exists(sessions_root())) return sessions;
    for (const auto& entry : std::filesystem::directory_iterator(sessions_root())) {
        if (!entry.is_regular_file()) continue;
        std::ifstream in(entry.path());
        AssistantShellSessionSummary summary;
        std::string line;
        while (std::getline(in, line)) {
            if (starts_with(line, "session_id=")) summary.session_id = line.substr(11);
            else if (starts_with(line, "created_at=")) summary.created_at = line.substr(11);
            else if (starts_with(line, "updated_at=")) summary.updated_at = line.substr(11);
            else if (starts_with(line, "title=")) summary.title = line.substr(6);
        }
        if (!summary.session_id.empty()) sessions.push_back(summary);
    }
    std::sort(sessions.begin(), sessions.end(), [](const auto& a, const auto& b) { return a.updated_at > b.updated_at; });
    return sessions;
}

std::optional<AssistantShellStatusSnapshot> AssistantShellSurfaceService::LoadLastStatus(const std::string& session_id) const {
    std::ifstream in(session_file_path(session_id));
    if (!in.good()) return std::nullopt;
    AssistantShellStatusSnapshot status;
    std::string line;
    while (std::getline(in, line)) {
        if (starts_with(line, "runtime_available=")) status.runtime_available = line.substr(18) == "1";
        else if (starts_with(line, "provider_ready=")) status.provider_ready = line.substr(15) == "1";
        else if (starts_with(line, "pending_confirmation_count=")) status.pending_confirmation_count = std::stoi(line.substr(27));
        else if (starts_with(line, "last_action_status=")) status.last_action_status = line.substr(19);
        else if (starts_with(line, "session_mode=")) status.session_mode = line.substr(13) == "extended" ? AssistantShellSessionMode::Extended : AssistantShellSessionMode::Concise;
    }
    return status;
}


life_orchestrator::app::ApplicationInvocationResult AssistantShellSurfaceService::RunCommand(const std::vector<std::string>& args) const {
    auto forwarded = args;
    forwarded.push_back("--data-root=" + data_root_.string());
    forwarded.push_back("--quiet-startup");
    return invoke_application_command(forwarded, environment_data_root_, working_root_);
}

AssistantShellStartupSnapshot AssistantShellSurfaceService::StartOrResumeSession(const std::optional<std::string>& session_id) {
    const auto id = session_id.value_or(next_session_id());
    auto summary = make_session_summary(id);
    auto messages = load_session_messages(id);
    const auto loaded_status = LoadLastStatus(id);
    auto status = loaded_status.value_or(build_status_snapshot(id, "Ready", AssistantShellSessionMode::Concise));
    if (messages.empty()) {
        std::ostringstream greeting;
        greeting << "Hello. Runtime is " << (status.runtime_available ? "available" : "unavailable")
                 << ", provider is " << (status.provider_ready ? "ready" : "not configured")
                 << ", and session mode is " << to_string(status.session_mode) << ".";
        messages.push_back(make_text_message("startup-greeting", "assistant", AssistantShellMessageBlockType::AssistantResponse, greeting.str()));
        messages.push_back(make_text_message("startup-status", "system", AssistantShellMessageBlockType::StatusNotice, "Use the composer to ask for a command, artifact summary, or guided action."));
        persist_session(summary, messages, status, std::nullopt, {}, {});
    }
    if (summary.title == "Assistant Session" && messages.size() > 2) summary.title = "Assistant Session";
    return {summary, status, {"Ask Life Orchestrator to help with the next step.", true, true}, LoadPendingAttachments(id), load_provider_operational_state(id), messages, build_tool_panel_sections()};
}

AssistantShellSubmissionResult AssistantShellSurfaceService::SubmitUserText(const AssistantShellSubmissionRequest& request) {
    emit_shell_diagnostic("assistant_shell_submit_begin");
    auto summary = make_session_summary(request.session_id);
    auto messages = load_session_messages(request.session_id);
    messages.push_back(make_text_message("user-" + now_string(), "user", AssistantShellMessageBlockType::UserText, sanitize_shell_field(request.user_text, 320)));

    auto append_and_persist = [&](const AssistantShellMessage& assistant_message,
                                  const AssistantShellStatusSnapshot& status,
                                  const std::optional<PendingConfirmationState>& pending,
                                  AssistantShellProviderOperationalState provider_state = {}) {
        messages.push_back(assistant_message);
        summary.updated_at = now_string();
        if (summary.title == "Assistant Session") summary.title = default_session_title(request.user_text);
        persist_session(summary, messages, status, pending, {request.attachments}, provider_state.last_provider_test_state.empty() && provider_state.last_provider_remediation_guidance_state.empty() && provider_state.last_configured_active_provider_summary.empty() ? load_provider_operational_state(request.session_id) : provider_state);
    };

    try {
        const auto command_result = invoke_application_command({"operator-query", "--data-root=" + data_root_.string(), "--quiet-startup", "--input", request.user_text}, environment_data_root_, working_root_);
        const auto combined = command_result.standard_output + "\n" + command_result.standard_error;
        emit_shell_diagnostic("assistant_shell_provider_result_received", combined);

        AssistantShellExecutionSummary exec_summary;
        exec_summary.provider_used = combined.find("provider_request_provider_name=") != std::string::npos;

        if (combined.find("operator_query=failed\nmessage=no_provider_configured") != std::string::npos) {
            exec_summary.resolution_path = "constrained_intent_routing";
            exec_summary.selected_route = "provider_unavailable";
            exec_summary.confidence = 0.0;
            exec_summary.confirmation_required = false;
            exec_summary.explanation = "Natural-language interpretation needs provider transport, so the shell returned a remediation path instead.";
            AssistantShellMessage assistant{"assistant-" + now_string(),
                                            "assistant",
                                            {{AssistantShellMessageBlockType::AssistantResponse, "I can still run exact commands, but provider-backed interpretation is not configured yet. Configure a provider to enable broader routing.", false, std::nullopt, std::nullopt, std::nullopt},
                                             {AssistantShellMessageBlockType::ExecutionSummary, "Thinking (Extended)", true, exec_summary, std::nullopt, std::nullopt}}};
            auto status = build_status_snapshot(request.session_id, "Provider setup required", AssistantShellSessionMode::Concise);
            append_and_persist(assistant, status, std::nullopt);
            emit_shell_diagnostic("assistant_shell_provider_result_parsed", "provider_unavailable");
            return {true, request.session_id, {messages.back()}, status, build_tool_panel_sections(), std::nullopt, request.attachments, load_provider_operational_state(request.session_id)};
        }

        if (command_result.exit_code == 0 && combined.find("operator_query=failed") == std::string::npos) {
            exec_summary.resolution_path = exec_summary.provider_used ? "constrained_intent_routing" : "exact_command_resolution";
            exec_summary.selected_route = sanitize_shell_field(value_for_key(combined, exec_summary.provider_used ? "matched_command" : "intent_route_command"), 96);
            if (exec_summary.selected_route.empty()) exec_summary.selected_route = sanitize_shell_field(value_for_key(combined, "matched_command"), 96);
            std::string failure_classification = sanitize_shell_field(value_for_key(combined, "operator_query_failure_class"), 96);
            if (exec_summary.provider_used && exec_summary.selected_route.empty()) failure_classification = "provider_output_incomplete";

            exec_summary.normalized_mode = sanitize_shell_field(value_for_key(combined, "normalized_mode"), 48);
            exec_summary.raw_mode = sanitize_shell_field(value_for_key(combined, "raw_mode"), 48);
            exec_summary.raw_matched_command = sanitize_shell_field(value_for_key(combined, "raw_matched_command"), 96);
            exec_summary.normalized_matched_command = sanitize_shell_field(value_for_key(combined, "normalized_matched_command"), 96);
            exec_summary.route_acceptance_result = sanitize_shell_field(value_for_key(combined, "route_acceptance_result"), 96);
            exec_summary.route_rejection_reason = sanitize_shell_field(value_for_key(combined, "route_rejection_reason"), 96);

            const auto confidence_text = value_for_key(combined, "normalized_confidence");
            if (exec_summary.provider_used && !try_parse_confidence(confidence_text, exec_summary.confidence)) {
                if (failure_classification.empty()) failure_classification = "provider_output_invalid";
            } else if (!exec_summary.provider_used) {
                exec_summary.confidence = 1.0;
            }

            const auto requires_confirmation_text = value_for_key(combined, "normalized_requires_confirmation");
            if (exec_summary.provider_used && !try_parse_bool(requires_confirmation_text, exec_summary.confirmation_required) && !requires_confirmation_text.empty()) {
                if (failure_classification.empty()) failure_classification = "provider_output_invalid";
            }

            exec_summary.explanation = exec_summary.provider_used
                                           ? sanitize_shell_field("raw_mode=" + exec_summary.raw_mode + "; normalized_mode=" + exec_summary.normalized_mode +
                                                                      "; raw_command=" + exec_summary.raw_matched_command + "; normalized_command=" + exec_summary.normalized_matched_command +
                                                                      "; acceptance=" + exec_summary.route_acceptance_result +
                                                                      (exec_summary.route_rejection_reason.empty() ? std::string{} : "; rejection=" + exec_summary.route_rejection_reason),
                                                                  240)
                                           : "The shell matched an exact command or alias through the authoritative helper surface first.";
            if (exec_summary.provider_used && exec_summary.explanation.empty()) exec_summary.explanation = "Provider output omitted a reasoning summary.";

            auto closest_commands = parse_closest_commands(value_for_key(combined, "closest_commands"));
            for (const auto& command : closest_commands) exec_summary.artifact_refreshes.push_back("closest_command:" + command);
            for (const auto& field : parse_kv_pairs(combined)) {
                if (starts_with(field.first, "refreshed_artifact_type")) exec_summary.artifact_refreshes.push_back(sanitize_shell_field(field.second, 64));
            }

            const auto user_message = sanitize_shell_field(value_for_key(combined, "user_facing_message"), 320);
            const auto canonical = sanitize_shell_field(value_for_key(combined, "canonical_command"), 96);
            const auto args_text = value_for_key(combined, "args");
            const auto response_text = !user_message.empty() ? user_message : (!canonical.empty() ? "Completed " + canonical + "." : exec_summary.provider_used ? "The provider returned an incomplete shell response." : "Completed the requested action.");

            if (exec_summary.provider_used && user_message.empty() && failure_classification.empty()) failure_classification = "provider_output_incomplete";
            if (!failure_classification.empty()) {
                auto provider_state = load_provider_operational_state(request.session_id);
                provider_state.last_provider_test_state = exec_summary.provider_used ? "provider_output_rejected" : provider_state.last_provider_test_state;
                provider_state.last_provider_remediation_guidance_state = failure_classification;
                provider_state.last_configured_active_provider_summary = sanitize_shell_field(value_for_key(combined, "provider_request_provider_name"), 96);
                auto assistant = make_failure_message("assistant-" + now_string(),
                                                      response_text,
                                                      failure_classification,
                                                      "The live provider result could not be safely normalized, so the shell rendered a stable failure instead of continuing.",
                                                      exec_summary.provider_used);
                auto status = build_status_snapshot(request.session_id, "Provider output needs review", AssistantShellSessionMode::Concise);
                append_and_persist(assistant, status, std::nullopt, provider_state);
                emit_shell_diagnostic("assistant_shell_submit_failed", failure_classification);
                return {false, request.session_id, {messages.back()}, status, build_tool_panel_sections(), std::nullopt, request.attachments, provider_state};
            }

            emit_shell_diagnostic("assistant_shell_provider_result_parsed", exec_summary.selected_route);
            AssistantShellMessage assistant{"assistant-" + now_string(), "assistant", {}};
            assistant.blocks.push_back({AssistantShellMessageBlockType::AssistantResponse, response_text, false, std::nullopt, std::nullopt, std::nullopt});

            std::optional<PendingConfirmationState> pending;
            std::optional<AssistantShellConfirmationRequest> confirmation;
            if (exec_summary.confirmation_required) {
                auto args = parse_command_args(args_text);
                if (!args.empty()) {
                    pending = PendingConfirmationState{"confirm-" + now_string(), args, exec_summary.selected_route};
                    confirmation = AssistantShellConfirmationRequest{pending->confirmation_id,
                                                                    "Confirm action",
                                                                    response_text,
                                                                    pending->execution_args,
                                                                    pending->lineage};
                    assistant.blocks.push_back({AssistantShellMessageBlockType::Confirmation, "Confirmation required before execution.", false, std::nullopt, confirmation, std::nullopt});
                    emit_shell_diagnostic("assistant_shell_confirmation_created", exec_summary.selected_route);
                } else {
                    exec_summary.confirmation_required = false;
                }
            }
            assistant.blocks.push_back({AssistantShellMessageBlockType::ExecutionSummary, "Thinking (Extended)", true, exec_summary, std::nullopt, std::nullopt});
            if (const auto provider_card = build_artifact_card("provider_config_summary"); provider_card.has_value()) {
                if (exec_summary.selected_route == "integration-set-provider" || exec_summary.selected_route == "integration-list-providers" || combined.find("provider_name=") != std::string::npos) {
                    assistant.blocks.push_back({AssistantShellMessageBlockType::ArtifactCard, provider_card->title, false, std::nullopt, std::nullopt, *provider_card});
                }
            }

            auto status = build_status_snapshot(request.session_id, exec_summary.confirmation_required ? "Awaiting confirmation" : "Completed", AssistantShellSessionMode::Concise);
            if (pending.has_value()) status.pending_confirmation_count = 1;
            auto provider_state = load_provider_operational_state(request.session_id);
            provider_state.last_provider_test_state = exec_summary.provider_used ? "provider_transport_used" : provider_state.last_provider_test_state;
            provider_state.last_provider_remediation_guidance_state = exec_summary.provider_used ? std::string{} : "exact_command_or_remediation";
            provider_state.last_configured_active_provider_summary = sanitize_shell_field(value_for_key(combined, "provider_request_provider_name"), 96);
            append_and_persist(assistant, status, pending, provider_state);
            return {true, request.session_id, {messages.back()}, status, build_tool_panel_sections(), confirmation, request.attachments, provider_state};
        }

        auto assistant = make_failure_message("assistant-" + now_string(),
                                              "I couldn't complete that request. Try an exact command, an alias, or open Help for command discovery.",
                                              sanitize_shell_field(value_for_key(combined, "operator_query_failure_class"), 96).empty() ? "provider_output_unrecognized" : sanitize_shell_field(value_for_key(combined, "operator_query_failure_class"), 96),
                                              "No exact command or supported interpreted route could be completed.",
                                              combined.find("provider_request_provider_name=") != std::string::npos);
        auto status = build_status_snapshot(request.session_id, "Needs clarification", AssistantShellSessionMode::Concise);
        append_and_persist(assistant, status, std::nullopt);
        emit_shell_diagnostic("assistant_shell_submit_failed", "provider_output_unrecognized");
        return {false, request.session_id, {messages.back()}, status, build_tool_panel_sections(), std::nullopt, request.attachments, load_provider_operational_state(request.session_id)};
    } catch (const std::exception& ex) {
        emit_shell_diagnostic("assistant_shell_submit_failed", ex.what());
        auto assistant = make_failure_message("assistant-" + now_string(),
                                              "The assistant shell hit an internal error while processing that request.",
                                              "provider_output_invalid",
                                              ex.what(),
                                              false);
        auto status = build_status_snapshot(request.session_id, "Submission failed", AssistantShellSessionMode::Concise);
        append_and_persist(assistant, status, std::nullopt);
        return {false, request.session_id, {messages.back()}, status, build_tool_panel_sections(), std::nullopt, request.attachments, load_provider_operational_state(request.session_id)};
    } catch (...) {
        emit_shell_diagnostic("assistant_shell_submit_failed", "unknown_exception");
        auto assistant = make_failure_message("assistant-" + now_string(),
                                              "The assistant shell hit an unknown internal error while processing that request.",
                                              "provider_output_invalid",
                                              "unknown_exception",
                                              false);
        auto status = build_status_snapshot(request.session_id, "Submission failed", AssistantShellSessionMode::Concise);
        append_and_persist(assistant, status, std::nullopt);
        return {false, request.session_id, {messages.back()}, status, build_tool_panel_sections(), std::nullopt, request.attachments, load_provider_operational_state(request.session_id)};
    }
}

AssistantShellConfirmationResult AssistantShellSurfaceService::ResolveConfirmation(const std::string& session_id,
                                                                                   const std::string& confirmation_id,
                                                                                   bool accepted) {
    const auto pending = load_pending_confirmation(session_id);
    if (!pending.has_value() || pending->confirmation_id != confirmation_id) {
        return {false, confirmation_id, "No matching confirmation request was found.", std::nullopt};
    }
    auto summary = make_session_summary(session_id);
    auto messages = load_session_messages(session_id);
    AssistantShellExecutionSummary exec_summary{"confirmation_resolution", pending->lineage, 1.0, false, false, {}, "The shell executed the previously confirmed authoritative route."};
    std::string assistant_message;
    if (accepted) {
        auto args = pending->execution_args;
        args.push_back("--data-root=" + data_root_.string());
        args.push_back("--quiet-startup");
        const auto result = invoke_application_command(args, environment_data_root_, working_root_);
        assistant_message = result.exit_code == 0 ? "Confirmed and executed through the authoritative runtime path." : "Confirmation was accepted, but execution failed.";
    } else {
        assistant_message = "Confirmation declined. No runtime action was executed.";
    }
    messages.push_back({"assistant-confirm-" + now_string(), "assistant", {{AssistantShellMessageBlockType::AssistantResponse, assistant_message, false, std::nullopt, std::nullopt, std::nullopt}, {AssistantShellMessageBlockType::ExecutionSummary, "Thinking (Extended)", true, exec_summary, std::nullopt, std::nullopt}}});
    summary.updated_at = now_string();
    auto status = build_status_snapshot(session_id, accepted ? "Confirmed" : "Cancelled", AssistantShellSessionMode::Concise);
    persist_session(summary, messages, status, std::nullopt, LoadPendingAttachments(session_id), load_provider_operational_state(session_id));
    return {accepted, confirmation_id, assistant_message, exec_summary};
}

AssistantShellPendingAttachmentState AssistantShellSurfaceService::AddAttachment(const AssistantShellAttachmentAddRequest& request) {
    auto summary = make_session_summary(request.session_id);
    auto messages = load_session_messages(request.session_id);
    auto status = LoadLastStatus(request.session_id).value_or(build_status_snapshot(request.session_id, "Ready", AssistantShellSessionMode::Concise));
    auto attachments = LoadPendingAttachments(request.session_id);
    attachments.attachments.push_back({request.local_path, request.display_name.empty() ? request.local_path : request.display_name, request.size_bytes, "attached"});
    persist_session(summary, messages, status, load_pending_confirmation(request.session_id), attachments, load_provider_operational_state(request.session_id));
    return attachments;
}

AssistantShellPendingAttachmentState AssistantShellSurfaceService::RemoveAttachment(const AssistantShellAttachmentRemoveRequest& request) {
    auto summary = make_session_summary(request.session_id);
    auto messages = load_session_messages(request.session_id);
    auto status = LoadLastStatus(request.session_id).value_or(build_status_snapshot(request.session_id, "Ready", AssistantShellSessionMode::Concise));
    auto attachments = LoadPendingAttachments(request.session_id);
    attachments.attachments.erase(std::remove_if(attachments.attachments.begin(), attachments.attachments.end(),
                                                 [&](const auto& item) { return item.local_path == request.local_path; }),
                                  attachments.attachments.end());
    persist_session(summary, messages, status, load_pending_confirmation(request.session_id), attachments, load_provider_operational_state(request.session_id));
    return attachments;
}

}  // namespace life_orchestrator::app::assistant_shell
