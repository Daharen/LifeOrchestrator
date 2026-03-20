#pragma once
#ifdef _WIN32
#include "app/assistant_shell/assistant_shell_surface_service.h"
#include <memory>
namespace life_orchestrator::ui::assistant_shell {
class AssistantShellController {
public:
    explicit AssistantShellController(std::shared_ptr<life_orchestrator::app::assistant_shell::AssistantShellSurfaceService> service);
    life_orchestrator::app::assistant_shell::AssistantShellStartupSnapshot Start();
private:
    std::shared_ptr<life_orchestrator::app::assistant_shell::AssistantShellSurfaceService> service_;
};
}  // namespace life_orchestrator::ui::assistant_shell
#endif
