#include "ui/assistant_shell/assistant_shell_confirmation_surface.h"
#ifdef _WIN32
namespace life_orchestrator::ui::assistant_shell {
void AssistantShellConfirmationSurface::Attach(HWND parent, HINSTANCE instance) {
    handle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Inline confirmations appear here.", WS_CHILD | SS_LEFT, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}
}  // namespace life_orchestrator::ui::assistant_shell
#endif
