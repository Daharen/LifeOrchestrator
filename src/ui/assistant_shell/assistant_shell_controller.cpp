#include "ui/assistant_shell/assistant_shell_controller.h"
#ifdef _WIN32
namespace life_orchestrator::ui::assistant_shell {
AssistantShellController::AssistantShellController(std::shared_ptr<life_orchestrator::app::assistant_shell::AssistantShellSurfaceService> service) : service_(std::move(service)) {}
life_orchestrator::app::assistant_shell::AssistantShellStartupSnapshot AssistantShellController::Start(const std::optional<std::string>& session_id) { return service_->StartOrResumeSession(session_id); }
life_orchestrator::app::assistant_shell::AssistantShellSubmissionResult AssistantShellController::SubmitUserText(const life_orchestrator::app::assistant_shell::AssistantShellSubmissionRequest& request) { return service_->SubmitUserText(request); }
life_orchestrator::app::assistant_shell::AssistantShellConfirmationResult AssistantShellController::ResolveConfirmation(const std::string& session_id,
                                                                                                                         const std::string& confirmation_id,
                                                                                                                         bool accepted) { return service_->ResolveConfirmation(session_id, confirmation_id, accepted); }
std::vector<life_orchestrator::app::assistant_shell::AssistantShellSessionSummary> AssistantShellController::ListSessions() const { return service_->ListSessions(); }
std::optional<life_orchestrator::app::assistant_shell::AssistantShellStatusSnapshot> AssistantShellController::LoadLastStatus(const std::string& session_id) const { return service_->LoadLastStatus(session_id); }
life_orchestrator::app::ApplicationInvocationResult AssistantShellController::RunCommand(const std::vector<std::string>& args) const { return service_->RunCommand(args); }
}  // namespace life_orchestrator::ui::assistant_shell
#endif
