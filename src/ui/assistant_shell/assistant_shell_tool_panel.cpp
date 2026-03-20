#include "ui/assistant_shell/assistant_shell_tool_panel.h"
#ifdef _WIN32
namespace life_orchestrator::ui::assistant_shell {
void AssistantShellToolPanel::Attach(HWND parent, HINSTANCE instance) {
    handle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"Historical Chats\r\nLinks\r\nCurrent AI Priority Lists\r\nScheduled AI Activities", WS_CHILD | SS_LEFT, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}
void AssistantShellToolPanel::SetVisible(bool visible) const { if (handle_ != nullptr) ShowWindow(handle_, visible ? SW_SHOW : SW_HIDE); }
}  // namespace life_orchestrator::ui::assistant_shell
#endif
