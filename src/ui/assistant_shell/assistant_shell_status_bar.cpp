#include "ui/assistant_shell/assistant_shell_status_bar.h"
#ifdef _WIN32
namespace life_orchestrator::ui::assistant_shell {
void AssistantShellStatusBar::Attach(HWND parent, HINSTANCE instance) {
    handle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Runtime: ready | Provider: unknown | Pending confirmations: 0 | Last action: Ready | Mode: concise", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}
}  // namespace life_orchestrator::ui::assistant_shell
#endif
