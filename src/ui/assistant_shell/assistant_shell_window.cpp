#include "ui/assistant_shell/assistant_shell_window.h"
#ifdef _WIN32
#include "ui/assistant_shell/assistant_shell_composer_input.h"
#include "app/provider_setup/provider_setup_service.h"
#include "ui/provider_setup/provider_setup_controller.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace life_orchestrator::ui::assistant_shell {
namespace {
constexpr int kMenuFile = 100;
constexpr int kMenuSettings = 101;
constexpr int kMenuActiveInterfaces = 102;
constexpr int kMenuActiveModules = 103;
constexpr int kMenuApiKeys = 104;
constexpr int kMenuConsole = 105;
constexpr int kMenuDeveloperLayer = 106;
constexpr int kMenuHelp = 107;
constexpr int kControlTitle = 200;
constexpr int kControlComposer = 201;
constexpr int kControlSubmit = 202;
constexpr int kControlAttach = 203;
constexpr int kControlTogglePanel = 204;
constexpr int kControlConfirmAccept = 205;
constexpr int kControlConfirmDecline = 206;
constexpr int kMinimumWindowWidth = 980;
constexpr int kMinimumWindowHeight = 640;

std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string narrow(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string get_window_text_utf8(HWND handle) {
    const int length = GetWindowTextLengthW(handle);
    std::wstring buffer(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(handle, buffer.data(), length + 1);
    buffer.resize(static_cast<std::size_t>(length));
    return narrow(buffer);
}

void append_menu_item(HMENU menu, UINT id, const wchar_t* title) {
    AppendMenuW(menu, MF_STRING, id, title);
}

HMENU control_menu_id(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

std::string sanitize_transcript_value(std::string value, std::size_t max_length = 320) {
    value.erase(std::remove(value.begin(), value.end(), '\r'), value.end());
    std::replace(value.begin(), value.end(), '\n', ' ');
    if (value.empty()) value = "(empty)";
    if (value.size() > max_length) value = value.substr(0, max_length) + "...";
    return value;
}

void emit_window_diagnostic(const std::string& marker, const std::string& detail = {}) {
    std::clog << marker;
    if (!detail.empty()) std::clog << " detail=" << sanitize_transcript_value(detail, 160);
    std::clog << '\n';
}
}

LRESULT CALLBACK AssistantShellWindow::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    AssistantShellWindow* self = reinterpret_cast<AssistantShellWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = reinterpret_cast<AssistantShellWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) self->hwnd_ = hwnd;
        return TRUE;
    }
    if (self == nullptr) return DefWindowProcW(hwnd, message, w_param, l_param);
    return self->HandleMessage(message, w_param, l_param);
}

AssistantShellWindow::AssistantShellWindow(std::shared_ptr<AssistantShellController> controller) : controller_(std::move(controller)) {}

int AssistantShellWindow::Run(HINSTANCE instance, int show_command) {
    instance_ = instance;
    const wchar_t* class_name = L"LifeOrchestratorAssistantShellWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = AssistantShellWindow::WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    hwnd_ = CreateWindowExW(0,
                            class_name,
                            L"Life Orchestrator Assistant Shell",
                            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                            CW_USEDEFAULT,
                            CW_USEDEFAULT,
                            1280,
                            800,
                            nullptr,
                            nullptr,
                            instance,
                            this);
    if (hwnd_ == nullptr) return 1;
    ShowWindow(hwnd_, show_command);
    UpdateWindow(hwnd_);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

LRESULT AssistantShellWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_CREATE: CreateUi(); InitializeShell(); return 0;
        case WM_SIZE: Layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(l_param);
            info->ptMinTrackSize.x = kMinimumWindowWidth;
            info->ptMinTrackSize.y = kMinimumWindowHeight;
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(w_param);
            const UINT notification_code = HIWORD(w_param);
            if (HandleMenuCommand(id, notification_code, l_param)) return 0;
            if (notification_code == BN_CLICKED) {
                if (id == kControlSubmit) { SubmitComposer(); return 0; }
                if (id == kControlTogglePanel) { ToggleToolPanel(); return 0; }
                if (id == kControlConfirmAccept) { ResolveConfirmation(true); return 0; }
                if (id == kControlConfirmDecline) { ResolveConfirmation(false); return 0; }
            }
            return 0;
        }
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: break;
    }
    return DefWindowProcW(hwnd_, message, w_param, l_param);
}

bool AssistantShellWindow::HandleMenuCommand(int id, UINT notification_code, LPARAM source_handle) {
    const bool is_menu_command = notification_code == 0 && source_handle == 0;
    if (!is_menu_command) {
        if (id == kMenuDeveloperLayer) emit_window_diagnostic("assistant_shell_ignored_non_menu_developer_layer_launch");
        return false;
    }

    if (id == kMenuDeveloperLayer) { OpenDeveloperLayer(SurfaceLaunchOrigin::MenuCommand); return true; }
    if (id == kMenuConsole) { OpenConsole(); return true; }
    if (id == kMenuApiKeys) { OpenProviderConfiguration(); return true; }
    if (id == kMenuActiveModules) { ShowActiveModules(); return true; }
    if (id == kMenuActiveInterfaces) { ShowActiveInterfaces(); return true; }
    if (id == kMenuHelp) { ShowHelp(); return true; }
    if (id == kMenuSettings) { ShowSettings(); return true; }
    return false;
}

void AssistantShellWindow::CreateUi() {
    auto menu = CreateMenu();
    append_menu_item(menu, kMenuFile, L"File");
    append_menu_item(menu, kMenuSettings, L"Settings");
    append_menu_item(menu, kMenuActiveInterfaces, L"Active Interfaces");
    append_menu_item(menu, kMenuActiveModules, L"Active Modules");
    append_menu_item(menu, kMenuApiKeys, L"API Keys");
    append_menu_item(menu, kMenuConsole, L"Console");
    append_menu_item(menu, kMenuDeveloperLayer, L"Developer Layer");
    append_menu_item(menu, kMenuHelp, L"Help");
    SetMenu(hwnd_, menu);

    transcript_.Attach(hwnd_, instance_);
    confirmation_.Attach(hwnd_, instance_);
    status_.Attach(hwnd_, instance_);
    tool_panel_.Attach(hwnd_, instance_);

    title_label_ = CreateWindowExW(0, L"STATIC", L"Life Orchestrator Assistant Shell", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, control_menu_id(kControlTitle), instance_, nullptr);
    composer_edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 0, 0, hwnd_, control_menu_id(kControlComposer), instance_, nullptr);
    submit_button_ = CreateWindowExW(0, L"BUTTON", L"Submit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd_, control_menu_id(kControlSubmit), instance_, nullptr);
    attach_button_ = CreateWindowExW(0, L"BUTTON", L"Attach", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, control_menu_id(kControlAttach), instance_, nullptr);
    toggle_panel_button_ = CreateWindowExW(0, L"BUTTON", L"Hide Panel", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, control_menu_id(kControlTogglePanel), instance_, nullptr);
    confirm_accept_button_ = CreateWindowExW(0, L"BUTTON", L"Confirm", WS_CHILD, 0, 0, 0, 0, hwnd_, control_menu_id(kControlConfirmAccept), instance_, nullptr);
    confirm_decline_button_ = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD, 0, 0, 0, 0, hwnd_, control_menu_id(kControlConfirmDecline), instance_, nullptr);

    SendMessageW(composer_edit_, EM_SETLIMITTEXT, 8000, 0);
    AttachComposerSubclass();
    Layout();
}

void AssistantShellWindow::InitializeShell() {
    const auto startup = controller_->Start();
    session_id_ = startup.session.session_id;
    composer_placeholder_ = startup.composer.placeholder_text;
    transcript_lines_.clear();
    for (const auto& message : startup.initial_messages) AppendMessage(message);
    UpdateStatus(startup.status);
    UpdateToolPanel(startup.tool_panel_sections);
    SetWindowTextW(composer_edit_, L"");
    UpdateConfirmationSurface();
}

void AssistantShellWindow::Layout() {
    if (hwnd_ == nullptr) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = 12;
    const int gutter = 10;
    const int title_height = 24;
    const int status_height = 24;
    const int composer_height = 78;
    const int confirmation_height = 32;
    const int button_width = 92;
    const int side_width = tool_panel_visible_ ? std::clamp(width / 4, 260, 360) : 0;
    const int main_width = width - (margin * 2) - (tool_panel_visible_ ? side_width + gutter : 0);
    const int transcript_top = margin + title_height + 8;
    const int transcript_bottom = height - margin - status_height - gutter - composer_height - gutter - confirmation_height - gutter;
    const int transcript_height = (std::max)(180, transcript_bottom - transcript_top);
    const int main_left = margin;
    const int side_left = main_left + main_width + gutter;

    MoveWindow(title_label_, main_left, margin, (std::max)(240, main_width - 120), title_height, TRUE);
    MoveWindow(toggle_panel_button_, main_left + (std::max)(240, main_width - 120), margin - 2, 108, 28, TRUE);
    MoveWindow(transcript_.handle(), main_left, transcript_top, main_width, transcript_height, TRUE);
    MoveWindow(confirmation_.handle(), main_left, transcript_top + transcript_height + gutter, (std::max)(300, main_width - (button_width * 2) - 12), confirmation_height, TRUE);
    MoveWindow(confirm_accept_button_, main_left + (std::max)(300, main_width - (button_width * 2) - 12) + 6, transcript_top + transcript_height + gutter, button_width, confirmation_height, TRUE);
    MoveWindow(confirm_decline_button_, main_left + (std::max)(300, main_width - (button_width * 2) - 12) + 6 + button_width + 6, transcript_top + transcript_height + gutter, button_width, confirmation_height, TRUE);
    MoveWindow(composer_edit_, main_left, height - margin - status_height - gutter - composer_height, (std::max)(320, main_width - (button_width * 2) - 12), composer_height, TRUE);
    MoveWindow(submit_button_, main_left + (std::max)(320, main_width - (button_width * 2) - 12) + 6, height - margin - status_height - gutter - composer_height, button_width, 32, TRUE);
    MoveWindow(attach_button_, main_left + (std::max)(320, main_width - (button_width * 2) - 12) + 6, height - margin - status_height - gutter - composer_height + 38, button_width, 32, TRUE);
    MoveWindow(status_.handle(), main_left, height - margin - status_height, width - (margin * 2), status_height, TRUE);

    tool_panel_.SetVisible(tool_panel_visible_);
    if (tool_panel_visible_) MoveWindow(tool_panel_.handle(), side_left, transcript_top, side_width, transcript_height + confirmation_height + gutter + composer_height, TRUE);
    ShowWindow(confirm_accept_button_, pending_confirmation_.has_value() ? SW_SHOW : SW_HIDE);
    ShowWindow(confirm_decline_button_, pending_confirmation_.has_value() ? SW_SHOW : SW_HIDE);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

std::string AssistantShellWindow::RenderMessageBlockLine(const app::assistant_shell::AssistantShellMessage& message,
                                                         const app::assistant_shell::AssistantShellMessageBlock& block) {
    std::ostringstream line;
    if (block.type == app::assistant_shell::AssistantShellMessageBlockType::ExecutionSummary) line << "Details: ";
    else if (block.type == app::assistant_shell::AssistantShellMessageBlockType::Confirmation) line << "Confirmation: ";
    else if (block.type == app::assistant_shell::AssistantShellMessageBlockType::ArtifactCard) line << "Artifact: ";
    else if (block.type == app::assistant_shell::AssistantShellMessageBlockType::UserText) line << "You: ";
    else if (message.role == "assistant") line << "Assistant: ";
    else line << "System: ";
    line << sanitize_transcript_value(block.text);
    if (block.execution_summary.has_value()) {
        const auto& summary = *block.execution_summary;
        line << "\r\n  Route: " << sanitize_transcript_value(summary.selected_route, 120)
             << " | Path: " << sanitize_transcript_value(summary.resolution_path, 120)
             << " | Confidence: " << summary.confidence
             << " | Provider: " << (summary.provider_used ? "yes" : "no");
        if (!summary.explanation.empty()) {
            line << "\r\n  Why: " << sanitize_transcript_value(summary.explanation);
        }
    }
    if (block.artifact_card.has_value()) {
        line << "\r\n  " << sanitize_transcript_value(block.artifact_card->title, 120);
        for (const auto& [label, value] : block.artifact_card->summary_fields) {
            line << "\r\n    " << sanitize_transcript_value(label, 80)
                 << ": " << sanitize_transcript_value(value, 160);
        }
    }
    if (block.confirmation_request.has_value()) {
        line << "\r\n  Confirmation: "
             << sanitize_transcript_value(block.confirmation_request->prompt, 160);
        pending_confirmation_ = block.confirmation_request;
    }
    return line.str();
}

void AssistantShellWindow::AppendMessage(const app::assistant_shell::AssistantShellMessage& message) {
    try {
        for (const auto& block : message.blocks) transcript_lines_.push_back(RenderMessageBlockLine(message, block));
    } catch (const std::exception& ex) {
        emit_window_diagnostic("assistant_shell_submit_failed", ex.what());
        transcript_lines_.push_back("System: Assistant shell rendering fallback activated.");
    } catch (...) {
        emit_window_diagnostic("assistant_shell_submit_failed", "unknown_render_exception");
        transcript_lines_.push_back("System: Assistant shell rendering fallback activated.");
    }
    SafeRefreshTranscriptText();
}

void AssistantShellWindow::AttachComposerSubclass() {
    SetWindowLongPtrW(composer_edit_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    composer_original_wndproc_ = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(
        composer_edit_, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&AssistantShellWindow::ComposerEditProc)));
}

void AssistantShellWindow::RefreshTranscriptText() {
    std::ostringstream combined;
    for (std::size_t index = 0; index < transcript_lines_.size(); ++index) {
        if (index > 0) combined << "\r\n\r\n";
        combined << sanitize_transcript_value(transcript_lines_[index], 2000);
    }
    transcript_.SetText(combined.str());
}

void AssistantShellWindow::SafeRefreshTranscriptText() {
    emit_window_diagnostic("assistant_shell_render_begin");
    try {
        RefreshTranscriptText();
        emit_window_diagnostic("assistant_shell_render_complete");
    } catch (const std::exception& ex) {
        emit_window_diagnostic("assistant_shell_submit_failed", ex.what());
        transcript_.SetText("System: Assistant shell transcript refresh failed; fallback rendering applied.");
    } catch (...) {
        emit_window_diagnostic("assistant_shell_submit_failed", "unknown_refresh_exception");
        transcript_.SetText("System: Assistant shell transcript refresh failed; fallback rendering applied.");
    }
}

LRESULT CALLBACK AssistantShellWindow::ComposerEditProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    auto* self = reinterpret_cast<AssistantShellWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self == nullptr || self->composer_original_wndproc_ == nullptr) return DefWindowProcW(hwnd, message, w_param, l_param);

    const bool is_return = (message == WM_KEYDOWN && w_param == VK_RETURN) || (message == WM_CHAR && w_param == '\r');
    if (is_return) {
        const bool shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (AssistantShellComposerInput::ResolveEnterKey(shift_pressed) == ComposerSubmitAction::Submit) {
            if (message == WM_KEYDOWN) self->SubmitComposer();
            return 0;
        }
    }

    return CallWindowProcW(self->composer_original_wndproc_, hwnd, message, w_param, l_param);
}


void AssistantShellWindow::UpdateStatus(const app::assistant_shell::AssistantShellStatusSnapshot& status_snapshot) {
    status_.SetSnapshot(status_snapshot);
}

void AssistantShellWindow::UpdateToolPanel(const std::vector<app::assistant_shell::AssistantShellToolPanelSection>& sections) {
    tool_panel_.SetSections(sections);
}

void AssistantShellWindow::UpdateConfirmationSurface() {
    if (!pending_confirmation_.has_value()) {
        confirmation_.SetText("No confirmation pending.");
    } else {
        confirmation_.SetText("Pending confirmation: " + pending_confirmation_->prompt);
    }
    Layout();
}

void AssistantShellWindow::SubmitComposer() {
    const auto text = get_window_text_utf8(composer_edit_);
    if (!AssistantShellComposerInput::CanSubmit(text) || text == composer_placeholder_) return;
    try {
        const auto result = controller_->SubmitUserText({session_id_, text});
        transcript_lines_.push_back("You: " + sanitize_transcript_value(text, 320));
        for (const auto& message : result.appended_messages) AppendMessage(message);
        pending_confirmation_ = result.pending_confirmation;
        UpdateStatus(result.status);
        UpdateToolPanel(result.tool_panel_sections);
        UpdateConfirmationSurface();
    } catch (const std::exception& ex) {
        emit_window_diagnostic("assistant_shell_submit_failed", ex.what());
        transcript_lines_.push_back("System: Assistant shell submission failed.");
        SafeRefreshTranscriptText();
    } catch (...) {
        emit_window_diagnostic("assistant_shell_submit_failed", "unknown_submit_exception");
        transcript_lines_.push_back("System: Assistant shell submission failed.");
        SafeRefreshTranscriptText();
    }
    SetWindowTextW(composer_edit_, L"");
    SetFocus(composer_edit_);
}

void AssistantShellWindow::ResolveConfirmation(bool accepted) {
    if (!pending_confirmation_.has_value()) return;
    const auto confirmation = controller_->ResolveConfirmation(session_id_, pending_confirmation_->confirmation_id, accepted);
    transcript_lines_.push_back(std::string{"Assistant: "} + sanitize_transcript_value(confirmation.assistant_message, 320));
    SafeRefreshTranscriptText();
    pending_confirmation_.reset();
    UpdateConfirmationSurface();
    if (const auto status = controller_->LoadLastStatus(session_id_); status.has_value()) UpdateStatus(*status);
    UpdateToolPanel(controller_->ListSessions().empty() ? std::vector<app::assistant_shell::AssistantShellToolPanelSection>{} : controller_->Start(session_id_).tool_panel_sections);
}

void AssistantShellWindow::ToggleToolPanel() {
    tool_panel_visible_ = !tool_panel_visible_;
    SetWindowTextW(toggle_panel_button_, tool_panel_visible_ ? L"Hide Panel" : L"Show Panel");
    Layout();
}

void AssistantShellWindow::ShowInfoDialog(const std::wstring& title, const std::string& body) const {
    MessageBoxW(hwnd_, widen(body).c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
}

void AssistantShellWindow::OpenDeveloperLayer(SurfaceLaunchOrigin origin) {
    if (origin != SurfaceLaunchOrigin::MenuCommand) {
        emit_window_diagnostic("assistant_shell_ignored_non_explicit_developer_layer_launch");
        return;
    }
    OpenSiblingExecutable(L"life_orchestrator_admin_gui.exe", L"Developer Layer", "Opened the Developer Layer surface in a separate window.");
}

void AssistantShellWindow::OpenConsole() {
    const auto app_path = sibling_executable_path(L"life_orchestrator_app.exe");
    if (app_path.empty()) {
        ShowInfoDialog(L"Console", "The operator console executable was not found next to the assistant shell binary.");
        return;
    }
    std::wstring shell = L"powershell.exe";
    if (GetFileAttributesW(L"C:\\Program Files\\PowerShell\\7\\pwsh.exe") != INVALID_FILE_ATTRIBUTES) shell = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe";
    std::wstring command = L"\"" + shell + L"\" -NoExit -Command \"& '" + app_path + L"' operator-console\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring mutable_command = command;
    const BOOL ok = CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, nullptr, &startup, &process);
    if (ok) {
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        transcript_lines_.push_back("System: Opened the operator console in a separate terminal window.");
        RefreshTranscriptText();
        return;
    }
    ShowInfoDialog(L"Console", "Failed to launch the operator console path.");
}

void AssistantShellWindow::OpenProviderConfiguration() {
    const char* env = std::getenv("LIFE_ORCHESTRATOR_DATA_ROOT");
    const std::string environment_data_root = env == nullptr ? std::string{"artifacts"} : std::string{env};
    const std::filesystem::path data_root{environment_data_root};
    if (!provider_setup_window_) {
        auto service = std::make_shared<app::provider_setup::ProviderSetupService>(data_root, std::filesystem::current_path(), environment_data_root);
        auto controller = std::make_shared<ui::provider_setup::ProviderSetupController>(service);
        provider_setup_window_ = std::make_unique<ui::provider_setup::ProviderSetupWindow>(controller);
    }
    provider_setup_window_->Show(instance_, hwnd_);
    transcript_lines_.push_back("System: Opened the provider configuration surface.");
    RefreshTranscriptText();
}

void AssistantShellWindow::ShowActiveModules() {
    const auto result = controller_->RunCommand({"list-modules"});
    ShowInfoDialog(L"Active Modules", result.standard_output.empty() ? result.standard_error : result.standard_output);
}

void AssistantShellWindow::ShowActiveInterfaces() {
    std::ostringstream body;
    const auto providers = controller_->RunCommand({"integration-list-providers"});
    const auto status = controller_->RunCommand({"status"});
    body << "Configured provider/interfaces\r\n-----------------------------\r\n" << (providers.standard_output.empty() ? providers.standard_error : providers.standard_output)
         << "\r\nRuntime snapshot\r\n-----------------------------\r\n" << (status.standard_output.empty() ? status.standard_error : status.standard_output);
    ShowInfoDialog(L"Active Interfaces", body.str());
}

void AssistantShellWindow::ShowHelp() {
    const auto result = controller_->RunCommand({"help"});
    ShowInfoDialog(L"Help", result.standard_output.empty() ? "Use exact commands, aliases, or the composer for guided routing." : result.standard_output);
}

void AssistantShellWindow::ShowSettings() {
    ShowInfoDialog(L"Settings", "Settings is intentionally minimal in this checkpoint. Use the status snapshot and Developer Layer while the settings surface is expanded.");
}

std::wstring AssistantShellWindow::sibling_executable_path(const wchar_t* executable_name) const {
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::filesystem::path path{buffer};
    return (path.parent_path() / executable_name).wstring();
}

void AssistantShellWindow::OpenSiblingExecutable(const wchar_t* executable_name, const wchar_t* title, const std::string& success_message) {
    const auto path = sibling_executable_path(executable_name);
    const HINSTANCE launched = ShellExecuteW(hwnd_, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(launched) <= 32) {
        ShowInfoDialog(title, "The requested surface could not be launched from the current build output directory.");
        return;
    }
    transcript_lines_.push_back("System: " + success_message);
    RefreshTranscriptText();
}

}  // namespace life_orchestrator::ui::assistant_shell
#endif
