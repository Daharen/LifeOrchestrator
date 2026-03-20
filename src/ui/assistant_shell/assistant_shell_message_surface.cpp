#include "ui/assistant_shell/assistant_shell_message_surface.h"
#ifdef _WIN32
namespace life_orchestrator::ui::assistant_shell {
void AssistantShellMessageSurface::Attach(HWND parent, HINSTANCE instance) {
    handle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Assistant transcript", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}
}  // namespace life_orchestrator::ui::assistant_shell
#endif
