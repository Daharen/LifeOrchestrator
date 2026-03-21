#pragma once

#include <optional>
#include <string>
#include <vector>

namespace life_orchestrator::app::assistant_shell {

enum class AssistantShellMessageBlockType {
    UserText,
    AssistantResponse,
    ExecutionSummary,
    Confirmation,
    ArtifactCard,
    StatusNotice
};

enum class AssistantShellSessionMode {
    Concise,
    Extended
};

struct AssistantShellExecutionSummary {
    std::string resolution_path;
    std::string selected_route;
    double confidence = 0.0;
    bool confirmation_required = false;
    bool provider_used = false;
    std::vector<std::string> artifact_refreshes;
    std::string explanation;
    std::string raw_mode;
    std::string normalized_mode;
    std::string raw_matched_command;
    std::string normalized_matched_command;
    std::string route_acceptance_result;
    std::string route_rejection_reason;
    std::string effective_canonical_provider_name;
    std::string effective_model_name;
    std::string effective_secret_source;
    std::string effective_data_root;
};

struct AssistantShellArtifactCard {
    std::string artifact_type;
    std::string artifact_id;
    std::string title;
    std::vector<std::pair<std::string, std::string>> summary_fields;
};

struct AssistantShellConfirmationRequest {
    std::string confirmation_id;
    std::string title;
    std::string prompt;
    std::vector<std::string> execution_args;
    std::string lineage;
};

struct AssistantShellConfirmationResult {
    bool accepted = false;
    std::string confirmation_id;
    std::string assistant_message;
    std::optional<AssistantShellExecutionSummary> execution_summary;
};

struct AssistantShellMessageBlock {
    AssistantShellMessageBlockType type = AssistantShellMessageBlockType::AssistantResponse;
    std::string text;
    bool expanded = false;
    std::optional<AssistantShellExecutionSummary> execution_summary;
    std::optional<AssistantShellConfirmationRequest> confirmation_request;
    std::optional<AssistantShellArtifactCard> artifact_card;
};

struct AssistantShellMessage {
    std::string message_id;
    std::string role;
    std::vector<AssistantShellMessageBlock> blocks;
};

struct AssistantShellSessionSummary {
    std::string session_id;
    std::string created_at;
    std::string updated_at;
    std::string title;
};

struct AssistantShellToolPanelItem {
    std::string item_id;
    std::string title;
    std::string subtitle;
    std::string comment_prompt;
};

struct AssistantShellToolPanelSection {
    std::string section_id;
    std::string title;
    std::string empty_state;
    std::vector<AssistantShellToolPanelItem> items;
};


struct AssistantShellAttachmentReference {
    std::string local_path;
    std::string display_name;
    std::string size_bytes;
    std::string attachment_state;
};

struct AssistantShellPendingAttachmentState {
    std::vector<AssistantShellAttachmentReference> attachments;
};

struct AssistantShellAttachmentAddRequest {
    std::string session_id;
    std::string local_path;
    std::string display_name;
    std::string size_bytes;
};

struct AssistantShellAttachmentRemoveRequest {
    std::string session_id;
    std::string local_path;
};

struct AssistantShellProviderOperationalState {
    std::string last_provider_test_state;
    std::string last_provider_remediation_guidance_state;
    std::string last_configured_active_provider_summary;
};

struct AssistantShellComposerState {
    std::string placeholder_text;
    bool can_submit = true;
    bool attachment_enabled = false;
};

struct AssistantShellStatusSnapshot {
    bool runtime_available = false;
    bool provider_ready = false;
    int pending_confirmation_count = 0;
    std::string last_action_status;
    AssistantShellSessionMode session_mode = AssistantShellSessionMode::Concise;
};

struct AssistantShellStartupSnapshot {
    AssistantShellSessionSummary session;
    AssistantShellStatusSnapshot status;
    AssistantShellComposerState composer;
    AssistantShellPendingAttachmentState pending_attachments;
    AssistantShellProviderOperationalState provider_state;
    std::vector<AssistantShellMessage> initial_messages;
    std::vector<AssistantShellToolPanelSection> tool_panel_sections;
};

struct AssistantShellSubmissionRequest {
    std::string session_id;
    std::string user_text;
    std::vector<AssistantShellAttachmentReference> attachments;
};

struct AssistantShellSubmissionResult {
    bool ok = false;
    std::string session_id;
    std::vector<AssistantShellMessage> appended_messages;
    AssistantShellStatusSnapshot status;
    std::vector<AssistantShellToolPanelSection> tool_panel_sections;
    std::optional<AssistantShellConfirmationRequest> pending_confirmation;
    AssistantShellPendingAttachmentState pending_attachments;
    AssistantShellProviderOperationalState provider_state;
};

std::string to_string(AssistantShellMessageBlockType type);
std::string to_string(AssistantShellSessionMode mode);

}  // namespace life_orchestrator::app::assistant_shell
