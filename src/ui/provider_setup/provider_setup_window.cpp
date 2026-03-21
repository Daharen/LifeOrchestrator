#include "ui/provider_setup/provider_setup_window.h"
#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

namespace life_orchestrator::ui::provider_setup {
namespace {
constexpr int kControlProviderList = 300;
constexpr int kControlProviderName = 301;
constexpr int kControlDisplayName = 302;
constexpr int kControlModelName = 303;
constexpr int kControlSecretSource = 304;
constexpr int kControlApiKey = 305;
constexpr int kControlEnvVar = 306;
constexpr int kControlSecretRef = 307;
constexpr int kControlEnabled = 308;
constexpr int kControlRefresh = 309;
constexpr int kControlSave = 310;
constexpr int kControlTest = 311;
constexpr int kControlClose = 312;
constexpr int kControlStatus = 313;

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

std::string read_window_text(HWND handle) {
    const int length = GetWindowTextLengthW(handle);
    std::wstring buffer(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(handle, buffer.data(), length + 1);
    buffer.resize(static_cast<std::size_t>(length));
    return narrow(buffer);
}

HMENU control_id(int id) {
    return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
}

HWND add_label(HWND parent, HINSTANCE instance, const wchar_t* text) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, parent, nullptr, instance, nullptr);
}

int combo_find_string_exact(HWND combo, const std::wstring& value) {
    return static_cast<int>(SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value.c_str())));
}

}  // namespace

ProviderSetupWindow::ProviderSetupWindow(std::shared_ptr<ProviderSetupController> controller) : controller_(std::move(controller)) {}

void ProviderSetupWindow::Show(HINSTANCE instance, HWND owner) {
    instance_ = instance;
    owner_ = owner;
    const wchar_t* class_name = L"LifeOrchestratorProviderSetupWindow";
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ProviderSetupWindow::WindowProc;
        wc.hInstance = instance_;
        wc.lpszClassName = class_name;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&wc);
        registered = true;
    }

    if (!IsOpen()) {
        hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW,
                                class_name,
                                L"Provider Configuration",
                                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                CW_USEDEFAULT,
                                CW_USEDEFAULT,
                                900,
                                640,
                                owner_,
                                nullptr,
                                instance_,
                                this);
    }
    if (!IsOpen()) return;
    ShowWindow(hwnd_, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd_);
    RefreshProviders();
}

bool ProviderSetupWindow::IsOpen() const {
    return hwnd_ != nullptr && IsWindow(hwnd_) != FALSE;
}

HWND ProviderSetupWindow::handle() const {
    return hwnd_;
}

LRESULT CALLBACK ProviderSetupWindow::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    ProviderSetupWindow* self = reinterpret_cast<ProviderSetupWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
        self = reinterpret_cast<ProviderSetupWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self != nullptr) self->hwnd_ = hwnd;
        return TRUE;
    }
    if (self == nullptr) return DefWindowProcW(hwnd, message, w_param, l_param);
    return self->HandleMessage(message, w_param, l_param);
}

LRESULT ProviderSetupWindow::HandleMessage(UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_CREATE: CreateUi(); RefreshProviders(); return 0;
        case WM_SIZE: Layout(); return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(l_param);
            info->ptMinTrackSize.x = minimum_size_.cx;
            info->ptMinTrackSize.y = minimum_size_.cy;
            return 0;
        }
        case WM_COMMAND: {
            const int id = LOWORD(w_param);
            const int code = HIWORD(w_param);
            if (id == kControlRefresh && code == BN_CLICKED) { RefreshProviders(); return 0; }
            if (id == kControlSave && code == BN_CLICKED) { SaveCurrentProvider(); return 0; }
            if (id == kControlTest && code == BN_CLICKED) { TestCurrentProvider(); return 0; }
            if (id == kControlClose && code == BN_CLICKED) { DestroyWindow(hwnd_); return 0; }
            if (id == kControlProviderList && code == LBN_SELCHANGE) { PopulateFromSelection(); return 0; }
            return 0;
        }
        case WM_CLOSE: DestroyWindow(hwnd_); return 0;
        case WM_DESTROY: hwnd_ = nullptr; return 0;
        default: break;
    }
    return DefWindowProcW(hwnd_, message, w_param, l_param);
}

void ProviderSetupWindow::CreateUi() {
    provider_list_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 0, 0, 0, 0, hwnd_, control_id(kControlProviderList), instance_, nullptr);

    provider_name_label_ = add_label(hwnd_, instance_, L"Provider Name");
    provider_name_edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_, control_id(kControlProviderName), instance_, nullptr);
    display_name_label_ = add_label(hwnd_, instance_, L"Display Name");
    display_name_edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_, control_id(kControlDisplayName), instance_, nullptr);
    model_name_label_ = add_label(hwnd_, instance_, L"Model Name");
    model_name_edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_, control_id(kControlModelName), instance_, nullptr);
    secret_source_label_ = add_label(hwnd_, instance_, L"Secret Source");
    secret_source_combo_ = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 0, hwnd_, control_id(kControlSecretSource), instance_, nullptr);
    SendMessageW(secret_source_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"direct"));
    SendMessageW(secret_source_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"env"));
    SendMessageW(secret_source_combo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"existing"));
    SendMessageW(secret_source_combo_, CB_SETCURSEL, 0, 0);

    api_key_label_ = add_label(hwnd_, instance_, L"API Key");
    api_key_edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD, 0, 0, 0, 0, hwnd_, control_id(kControlApiKey), instance_, nullptr);
    env_var_label_ = add_label(hwnd_, instance_, L"Env Var Name");
    env_var_edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_, control_id(kControlEnvVar), instance_, nullptr);
    secret_ref_label_ = add_label(hwnd_, instance_, L"Existing Secret Reference");
    secret_ref_edit_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd_, control_id(kControlSecretRef), instance_, nullptr);
    enabled_checkbox_ = CreateWindowExW(0, L"BUTTON", L"Enabled", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 0, 0, 0, 0, hwnd_, control_id(kControlEnabled), instance_, nullptr);

    refresh_button_ = CreateWindowExW(0, L"BUTTON", L"Refresh", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, control_id(kControlRefresh), instance_, nullptr);
    save_button_ = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd_, control_id(kControlSave), instance_, nullptr);
    test_button_ = CreateWindowExW(0, L"BUTTON", L"Test", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, control_id(kControlTest), instance_, nullptr);
    close_button_ = CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd_, control_id(kControlClose), instance_, nullptr);
    status_box_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY, 0, 0, 0, 0, hwnd_, control_id(kControlStatus), instance_, nullptr);

    CheckDlgButton(hwnd_, kControlEnabled, BST_CHECKED);
    Layout();
}

void ProviderSetupWindow::Layout() {
    if (!IsOpen()) return;
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = 12;
    const int gutter = 10;
    const int list_width = std::clamp(width / 3, 220, 300);
    const int button_row_height = 30;
    const int status_height = (std::max)(140, height / 4);
    const int form_left = margin + list_width + gutter;
    const int form_width = width - form_left - margin;
    const int top_height = height - (margin * 2) - gutter - status_height;
    const int label_height = 18;
    const int edit_height = 24;
    const int row_gap = 6;

    MoveWindow(provider_list_, margin, margin, list_width, top_height, TRUE);

    int y = margin;
    auto move_label = [&](HWND label, const int current_y) {
        if (label != nullptr) MoveWindow(label, form_left, current_y, form_width, label_height, TRUE);
    };
    move_label(provider_name_label_, y); y += label_height;
    MoveWindow(provider_name_edit_, form_left, y, form_width, edit_height, TRUE); y += edit_height + row_gap;
    move_label(display_name_label_, y); y += label_height;
    MoveWindow(display_name_edit_, form_left, y, form_width, edit_height, TRUE); y += edit_height + row_gap;
    move_label(model_name_label_, y); y += label_height;
    MoveWindow(model_name_edit_, form_left, y, form_width, edit_height, TRUE); y += edit_height + row_gap;
    move_label(secret_source_label_, y); y += label_height;
    MoveWindow(secret_source_combo_, form_left, y, form_width, 160, TRUE); y += edit_height + row_gap;
    move_label(api_key_label_, y); y += label_height;
    MoveWindow(api_key_edit_, form_left, y, form_width, edit_height, TRUE); y += edit_height + row_gap;
    move_label(env_var_label_, y); y += label_height;
    MoveWindow(env_var_edit_, form_left, y, form_width, edit_height, TRUE); y += edit_height + row_gap;
    move_label(secret_ref_label_, y); y += label_height;
    MoveWindow(secret_ref_edit_, form_left, y, form_width, edit_height, TRUE); y += edit_height + row_gap + 2;

    MoveWindow(enabled_checkbox_, form_left, y, 120, edit_height, TRUE);
    const int button_width = 88;
    int button_x = form_left + (std::max)(130, form_width - ((button_width + 6) * 4));
    MoveWindow(refresh_button_, button_x, y, button_width, button_row_height, TRUE); button_x += button_width + 6;
    MoveWindow(save_button_, button_x, y, button_width, button_row_height, TRUE); button_x += button_width + 6;
    MoveWindow(test_button_, button_x, y, button_width, button_row_height, TRUE); button_x += button_width + 6;
    MoveWindow(close_button_, button_x, y, button_width, button_row_height, TRUE);

    MoveWindow(status_box_, margin, height - margin - status_height, width - (margin * 2), status_height, TRUE);
}

void ProviderSetupWindow::RefreshProviders() {
    const int previous_index = static_cast<int>(SendMessageW(provider_list_, LB_GETCURSEL, 0, 0));
    std::string previous_name;
    if (previous_index != LB_ERR && previous_index >= 0 && previous_index < static_cast<int>(providers_.size())) previous_name = providers_[static_cast<std::size_t>(previous_index)].provider_name;

    providers_ = controller_->ListProviders();
    SendMessageW(provider_list_, LB_RESETCONTENT, 0, 0);
    int selected_index = LB_ERR;
    for (std::size_t index = 0; index < providers_.size(); ++index) {
        const auto& provider = providers_[index];
        const std::wstring label = widen(provider.provider_name + (provider.model_name.empty() ? std::string{} : " (" + provider.model_name + ")"));
        const auto inserted = static_cast<int>(SendMessageW(provider_list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
        if (provider.provider_name == previous_name) selected_index = inserted;
    }

    if (selected_index == LB_ERR && !providers_.empty()) selected_index = 0;
    if (selected_index != LB_ERR) {
        SendMessageW(provider_list_, LB_SETCURSEL, static_cast<WPARAM>(selected_index), 0);
        PopulateFromSelection();
        return;
    }

    SetWindowTextW(provider_name_edit_, L"");
    SetWindowTextW(display_name_edit_, L"");
    SetWindowTextW(model_name_edit_, L"");
    SendMessageW(secret_source_combo_, CB_SETCURSEL, 0, 0);
    SetWindowTextW(api_key_edit_, L"");
    SetWindowTextW(env_var_edit_, L"");
    SetWindowTextW(secret_ref_edit_, L"");
    CheckDlgButton(hwnd_, kControlEnabled, BST_CHECKED);
    SetStatusText("No providers configured.");
}

void ProviderSetupWindow::PopulateFromSelection() {
    const int selected = static_cast<int>(SendMessageW(provider_list_, LB_GETCURSEL, 0, 0));
    if (selected == LB_ERR || selected < 0 || selected >= static_cast<int>(providers_.size())) {
        SetStatusText("Select a provider to view or edit its settings.");
        return;
    }

    const auto& provider = providers_[static_cast<std::size_t>(selected)];
    SetWindowTextW(provider_name_edit_, widen(provider.provider_name).c_str());
    SetWindowTextW(display_name_edit_, widen(provider.display_name).c_str());
    SetWindowTextW(model_name_edit_, widen(provider.model_name).c_str());
    const int combo_index = combo_find_string_exact(secret_source_combo_, widen(provider.secret_source.empty() ? "direct" : provider.secret_source));
    SendMessageW(secret_source_combo_, CB_SETCURSEL, combo_index == CB_ERR ? 0 : combo_index, 0);
    CheckDlgButton(hwnd_, kControlEnabled, provider.enabled ? BST_CHECKED : BST_UNCHECKED);

    std::ostringstream status;
    status << "Provider: " << provider.provider_name;
    if (!provider.model_name.empty()) status << "\r\nModel: " << provider.model_name;
    status << "\r\nSecret: " << provider.redacted_secret_status;
    status << "\r\nStatus: " << (provider.status.empty() ? "unknown" : provider.status);
    SetStatusText(status.str());
}

void ProviderSetupWindow::SaveCurrentProvider() {
    app::provider_setup::ProviderSetupUpsertRequest request;
    request.provider_name = read_window_text(provider_name_edit_);
    request.display_name = read_window_text(display_name_edit_);
    request.model_name = read_window_text(model_name_edit_);
    const int selected_source = static_cast<int>(SendMessageW(secret_source_combo_, CB_GETCURSEL, 0, 0));
    wchar_t source_text[32]{};
    if (selected_source != CB_ERR) SendMessageW(secret_source_combo_, CB_GETLBTEXT, selected_source, reinterpret_cast<LPARAM>(source_text));
    request.secret_source = narrow(source_text);
    if (request.secret_source.empty()) request.secret_source = "direct";
    request.api_key = read_window_text(api_key_edit_);
    request.env_var_name = read_window_text(env_var_edit_);
    request.existing_secret_reference = read_window_text(secret_ref_edit_);
    request.enabled = IsDlgButtonChecked(hwnd_, kControlEnabled) == BST_CHECKED;

    const auto result = controller_->SaveProvider(request);
    const std::string body = result.standard_output.empty() ? result.standard_error : result.standard_output;
    SetStatusText(body.empty() ? (result.exit_code == 0 ? "Provider saved." : "Provider save failed.") : body);
    if (result.exit_code == 0) RefreshProviders();
}

void ProviderSetupWindow::TestCurrentProvider() {
    const auto provider_name = read_window_text(provider_name_edit_);
    const auto result = controller_->TestProvider(provider_name);
    std::ostringstream status;
    status << (result.summary.empty() ? (result.ok ? "Provider test succeeded." : "Provider test failed.") : result.summary);
    if (!result.safe_details.empty()) status << "\r\n\r\n" << result.safe_details;
    SetStatusText(status.str());
}

void ProviderSetupWindow::SetStatusText(const std::string& text) {
    SetWindowTextW(status_box_, widen(text).c_str());
}

}  // namespace life_orchestrator::ui::provider_setup
#endif
