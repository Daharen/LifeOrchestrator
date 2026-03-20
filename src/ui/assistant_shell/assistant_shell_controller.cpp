#include "ui/assistant_shell/assistant_shell_controller.h"
#ifdef _WIN32
namespace life_orchestrator::ui::assistant_shell {
AssistantShellController::AssistantShellController(std::shared_ptr<life_orchestrator::app::assistant_shell::AssistantShellSurfaceService> service) : service_(std::move(service)) {}
life_orchestrator::app::assistant_shell::AssistantShellStartupSnapshot AssistantShellController::Start() { return service_->StartOrResumeSession(); }
}  // namespace life_orchestrator::ui::assistant_shell
#endif
