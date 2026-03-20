#pragma once
#ifdef _WIN32
#include "app/assistant_shell/assistant_shell_surface_contracts.h"
#include <windows.h>
namespace life_orchestrator::ui::assistant_shell {
class AssistantShellStatusBar {
public:
    void Attach(HWND parent, HINSTANCE instance);
    void SetSnapshot(const life_orchestrator::app::assistant_shell::AssistantShellStatusSnapshot& snapshot) const;
    HWND handle() const { return handle_; }
private:
    HWND handle_ = nullptr;
};
}  // namespace life_orchestrator::ui::assistant_shell
#endif
