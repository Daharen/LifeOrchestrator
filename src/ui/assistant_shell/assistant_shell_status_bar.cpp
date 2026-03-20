#include "ui/assistant_shell/assistant_shell_status_bar.h"
#ifdef _WIN32
#include <sstream>
namespace life_orchestrator::ui::assistant_shell {
namespace {
std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size);
    return result;
}
}
void AssistantShellStatusBar::Attach(HWND parent, HINSTANCE instance) {
    handle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}
void AssistantShellStatusBar::SetSnapshot(const life_orchestrator::app::assistant_shell::AssistantShellStatusSnapshot& snapshot) const {
    std::ostringstream text;
    text << "Runtime: " << (snapshot.runtime_available ? "ready" : "offline")
         << " | Provider: " << (snapshot.provider_ready ? "configured" : "not configured")
         << " | Pending confirmations: " << snapshot.pending_confirmation_count
         << " | Last action: " << snapshot.last_action_status
         << " | Mode: " << life_orchestrator::app::assistant_shell::to_string(snapshot.session_mode);
    if (handle_ != nullptr) SetWindowTextW(handle_, widen(text.str()).c_str());
}
}  // namespace life_orchestrator::ui::assistant_shell
#endif
