#pragma once

#include "app/application_bootstrap.hpp"
#include "app/assistant_shell/assistant_shell_surface_contracts.h"

#include <filesystem>
#include <optional>
#include <string>

namespace life_orchestrator::app::assistant_shell {

class AssistantShellSurfaceService {
public:
    explicit AssistantShellSurfaceService(std::filesystem::path data_root,
                                          std::filesystem::path working_root = std::filesystem::current_path(),
                                          std::string environment_data_root = {});

    AssistantShellStartupSnapshot StartOrResumeSession(const std::optional<std::string>& session_id = std::nullopt);
    AssistantShellSubmissionResult SubmitUserText(const AssistantShellSubmissionRequest& request);
    AssistantShellPendingAttachmentState AddAttachment(const AssistantShellAttachmentAddRequest& request);
    AssistantShellPendingAttachmentState RemoveAttachment(const AssistantShellAttachmentRemoveRequest& request);
    AssistantShellPendingAttachmentState LoadPendingAttachments(const std::string& session_id) const;
    AssistantShellConfirmationResult ResolveConfirmation(const std::string& session_id,
                                                         const std::string& confirmation_id,
                                                         bool accepted);
    std::vector<AssistantShellSessionSummary> ListSessions() const;
    std::optional<AssistantShellStatusSnapshot> LoadLastStatus(const std::string& session_id) const;
    life_orchestrator::app::ApplicationInvocationResult RunCommand(const std::vector<std::string>& args) const;

private:
    struct PendingConfirmationState {
        std::string confirmation_id;
        std::vector<std::string> execution_args;
        std::string lineage;
    };

    std::filesystem::path data_root_;
    std::filesystem::path working_root_;
    std::string environment_data_root_;

    std::filesystem::path sessions_root() const;
    std::filesystem::path session_file_path(const std::string& session_id) const;
    AssistantShellSessionSummary make_session_summary(const std::string& session_id) const;
    AssistantShellStatusSnapshot build_status_snapshot(const std::string& session_id,
                                                       const std::string& last_action_status,
                                                       AssistantShellSessionMode mode) const;
    std::vector<AssistantShellToolPanelSection> build_tool_panel_sections() const;
    std::vector<AssistantShellMessage> load_session_messages(const std::string& session_id) const;
    void persist_session(const AssistantShellSessionSummary& summary,
                         const std::vector<AssistantShellMessage>& messages,
                         const AssistantShellStatusSnapshot& status,
                         const std::optional<PendingConfirmationState>& pending_confirmation,
                         const AssistantShellPendingAttachmentState& attachments,
                         const AssistantShellProviderOperationalState& provider_state) const;
    std::optional<PendingConfirmationState> load_pending_confirmation(const std::string& session_id) const;
    AssistantShellProviderOperationalState load_provider_operational_state(const std::string& session_id) const;
    std::string next_session_id() const;
    std::string now_string() const;
    std::string default_session_title(const std::string& first_user_text) const;
    std::optional<AssistantShellArtifactCard> build_artifact_card(const std::string& artifact_type) const;
};

}  // namespace life_orchestrator::app::assistant_shell
