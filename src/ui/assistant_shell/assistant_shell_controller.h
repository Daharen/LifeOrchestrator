#pragma once
#ifdef _WIN32
#include "app/assistant_shell/assistant_shell_surface_service.h"
#include <memory>
namespace life_orchestrator::ui::assistant_shell {
class AssistantShellController {
public:
    explicit AssistantShellController(std::shared_ptr<life_orchestrator::app::assistant_shell::AssistantShellSurfaceService> service);
    life_orchestrator::app::assistant_shell::AssistantShellStartupSnapshot Start(const std::optional<std::string>& session_id = std::nullopt);
    life_orchestrator::app::assistant_shell::AssistantShellSubmissionResult SubmitUserText(const life_orchestrator::app::assistant_shell::AssistantShellSubmissionRequest& request);
    life_orchestrator::app::assistant_shell::AssistantShellConfirmationResult ResolveConfirmation(const std::string& session_id,
                                                                                                   const std::string& confirmation_id,
                                                                                                   bool accepted);
    std::vector<life_orchestrator::app::assistant_shell::AssistantShellSessionSummary> ListSessions() const;
    std::optional<life_orchestrator::app::assistant_shell::AssistantShellStatusSnapshot> LoadLastStatus(const std::string& session_id) const;
    life_orchestrator::app::ApplicationInvocationResult RunCommand(const std::vector<std::string>& args) const;
private:
    std::shared_ptr<life_orchestrator::app::assistant_shell::AssistantShellSurfaceService> service_;
};
}  // namespace life_orchestrator::ui::assistant_shell
#endif
