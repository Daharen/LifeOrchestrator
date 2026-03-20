#pragma once
#ifdef _WIN32
#include "app/assistant_shell/assistant_shell_surface_contracts.h"
#include <vector>
#include <windows.h>
namespace life_orchestrator::ui::assistant_shell {
class AssistantShellToolPanel {
public:
    void Attach(HWND parent, HINSTANCE instance);
    void SetSections(const std::vector<life_orchestrator::app::assistant_shell::AssistantShellToolPanelSection>& sections) const;
    void SetVisible(bool visible) const;
    HWND handle() const { return handle_; }
private:
    HWND handle_ = nullptr;
};
}  // namespace life_orchestrator::ui::assistant_shell
#endif
