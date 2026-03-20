#pragma once
#ifdef _WIN32
#include "ui/assistant_shell/assistant_shell_controller.h"
#include "ui/assistant_shell/assistant_shell_message_surface.h"
#include "ui/assistant_shell/assistant_shell_status_bar.h"
#include "ui/assistant_shell/assistant_shell_tool_panel.h"
#include "ui/assistant_shell/assistant_shell_confirmation_surface.h"
#include <memory>
namespace life_orchestrator::ui::assistant_shell {
class AssistantShellWindow {
public:
    explicit AssistantShellWindow(std::shared_ptr<AssistantShellController> controller);
    int Run(HINSTANCE instance, int show_command);
private:
    std::shared_ptr<AssistantShellController> controller_;
};
}  // namespace life_orchestrator::ui::assistant_shell
#endif
