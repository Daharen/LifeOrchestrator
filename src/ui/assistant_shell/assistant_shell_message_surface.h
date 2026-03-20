#pragma once
#ifdef _WIN32
#include <windows.h>
namespace life_orchestrator::ui::assistant_shell {
class AssistantShellMessageSurface {
public:
    void Attach(HWND parent, HINSTANCE instance);
    HWND handle() const { return handle_; }
private:
    HWND handle_ = nullptr;
};
}  // namespace life_orchestrator::ui::assistant_shell
#endif
