#include "ui/assistant_shell/assistant_shell_message_surface.h"
#ifdef _WIN32
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
void AssistantShellMessageSurface::Attach(HWND parent, HINSTANCE instance) {
    handle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}
void AssistantShellMessageSurface::SetText(const std::string& text) const {
    if (handle_ != nullptr) SetWindowTextW(handle_, widen(text).c_str());
}
}  // namespace life_orchestrator::ui::assistant_shell
#endif
