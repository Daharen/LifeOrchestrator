#pragma once
#ifdef _WIN32
#include "ui/assistant_shell/assistant_shell_controller.h"
#include "ui/assistant_shell/assistant_shell_message_surface.h"
#include "ui/assistant_shell/assistant_shell_status_bar.h"
#include "ui/assistant_shell/assistant_shell_tool_panel.h"
#include "ui/assistant_shell/assistant_shell_confirmation_surface.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace life_orchestrator::ui::assistant_shell {
class AssistantShellWindow {
public:
    explicit AssistantShellWindow(std::shared_ptr<AssistantShellController> controller);
    int Run(HINSTANCE instance, int show_command);
private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);
    void CreateUi();
    void InitializeShell();
    void Layout();
    void AppendMessage(const life_orchestrator::app::assistant_shell::AssistantShellMessage& message);
    void UpdateStatus(const life_orchestrator::app::assistant_shell::AssistantShellStatusSnapshot& status_snapshot);
    void UpdateToolPanel(const std::vector<life_orchestrator::app::assistant_shell::AssistantShellToolPanelSection>& sections);
    void UpdateConfirmationSurface();
    void SubmitComposer();
    void ResolveConfirmation(bool accepted);
    void ToggleToolPanel();
    void ShowInfoDialog(const std::wstring& title, const std::string& body) const;
    void OpenDeveloperLayer();
    void OpenConsole();
    void OpenProviderConfiguration();
    void ShowActiveModules();
    void ShowActiveInterfaces();
    void ShowHelp();
    void ShowSettings();
    std::wstring sibling_executable_path(const wchar_t* executable_name) const;
    void OpenSiblingExecutable(const wchar_t* executable_name, const wchar_t* title, const std::string& success_message);

    std::shared_ptr<AssistantShellController> controller_;
    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    HWND title_label_ = nullptr;
    HWND composer_edit_ = nullptr;
    HWND submit_button_ = nullptr;
    HWND attach_button_ = nullptr;
    HWND toggle_panel_button_ = nullptr;
    HWND confirm_accept_button_ = nullptr;
    HWND confirm_decline_button_ = nullptr;
    AssistantShellMessageSurface transcript_;
    AssistantShellConfirmationSurface confirmation_;
    AssistantShellStatusBar status_;
    AssistantShellToolPanel tool_panel_;
    std::string session_id_;
    std::string composer_placeholder_;
    bool tool_panel_visible_ = true;
    std::vector<std::string> transcript_lines_;
    std::optional<life_orchestrator::app::assistant_shell::AssistantShellConfirmationRequest> pending_confirmation_;
};
}  // namespace life_orchestrator::ui::assistant_shell
#endif
