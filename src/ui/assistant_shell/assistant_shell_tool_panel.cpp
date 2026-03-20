#include "ui/assistant_shell/assistant_shell_tool_panel.h"
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
void AssistantShellToolPanel::Attach(HWND parent, HINSTANCE instance) {
    handle_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}
void AssistantShellToolPanel::SetSections(const std::vector<life_orchestrator::app::assistant_shell::AssistantShellToolPanelSection>& sections) const {
    std::ostringstream text;
    if (sections.empty()) {
        text << "Right panel is online but no sections are available yet.";
    }
    for (std::size_t i = 0; i < sections.size(); ++i) {
        const auto& section = sections[i];
        if (i > 0) text << "\r\n\r\n";
        text << section.title << "\r\n";
        if (section.items.empty()) {
            text << "  " << section.empty_state;
            continue;
        }
        for (const auto& item : section.items) {
            text << "  • " << item.title;
            if (!item.subtitle.empty()) text << " — " << item.subtitle;
            if (!item.comment_prompt.empty()) text << "\r\n    " << item.comment_prompt;
            text << "\r\n";
        }
    }
    if (handle_ != nullptr) SetWindowTextW(handle_, widen(text.str()).c_str());
}
void AssistantShellToolPanel::SetVisible(bool visible) const { if (handle_ != nullptr) ShowWindow(handle_, visible ? SW_SHOW : SW_HIDE); }
}  // namespace life_orchestrator::ui::assistant_shell
#endif
