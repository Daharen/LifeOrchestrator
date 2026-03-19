#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "app/application_bootstrap.hpp"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "comctl32.lib")

namespace {

constexpr int kControlQuickLabel = 100;
constexpr int kControlQuickCombo = 101;
constexpr int kControlRunQuickButton = 102;
constexpr int kControlPaletteLabel = 103;
constexpr int kControlPaletteEdit = 104;
constexpr int kControlSuggestionList = 105;
constexpr int kControlUseSuggestionButton = 106;
constexpr int kControlCommandLabel = 107;
constexpr int kControlCommandEdit = 108;
constexpr int kControlRunCommandButton = 109;
constexpr int kControlRefreshStatusButton = 110;
constexpr int kControlOutputEdit = 111;
constexpr int kControlStatusText = 112;

HMENU control_menu_id(const int value) {
    return reinterpret_cast<HMENU>(static_cast<intptr_t>(value));
}

struct QuickCommand {
    const wchar_t* label;
    std::vector<std::string> args;
};

const std::vector<QuickCommand>& quick_commands() {
    static const std::vector<QuickCommand> commands = {
        {L"Status", {"status"}},
        {L"Activity Inventory", {"procedural-list-activities"}},
        {L"Procedural Proposals", {"procedural-list-proposals"}},
        {L"Behavioral Backlog", {"behavioral-list-backlog"}},
        {L"Behavioral Interventions", {"behavioral-list-interventions"}},
        {L"Behavioral Reevaluations", {"behavioral-list-reevaluations"}},
        {L"Scheduling Candidates", {"scheduling-list-candidates"}},
        {L"Schedule Proposals", {"scheduling-list-proposals"}},
        {L"Provider Summary", {"integration-list-providers"}},
        {L"Commands", {"commands"}},
        {L"Aliases", {"aliases"}}
    };
    return commands;
}

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

void set_control_text(HWND handle, const std::string& value) {
    SetWindowTextW(handle, widen(value).c_str());
}

std::string get_control_text(HWND handle) {
    const int length = GetWindowTextLengthW(handle);
    std::wstring buffer(static_cast<std::size_t>(length + 1), L'\0');
    const int written = GetWindowTextW(handle, buffer.data(), length + 1);
    buffer.resize(static_cast<std::size_t>(written));
    return narrow(buffer);
}

struct GuiState {
    std::filesystem::path working_root;
    std::string environment_data_root;
    HWND quick_combo = nullptr;
    HWND palette_edit = nullptr;
    HWND suggestion_list = nullptr;
    HWND command_edit = nullptr;
    HWND output_edit = nullptr;
    HWND status_text = nullptr;
};

GuiState* state_from(HWND hwnd) {
    return reinterpret_cast<GuiState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void set_status(HWND hwnd, const std::string& text) {
    if (auto* state = state_from(hwnd); state != nullptr && state->status_text != nullptr) {
        set_control_text(state->status_text, text);
    }
}

void set_output(HWND hwnd, const std::string& text) {
    if (auto* state = state_from(hwnd); state != nullptr && state->output_edit != nullptr) {
        set_control_text(state->output_edit, text);
    }
}

void refresh_suggestions(HWND hwnd) {
    auto* state = state_from(hwnd);
    if (state == nullptr || state->palette_edit == nullptr || state->suggestion_list == nullptr) return;

    SendMessageW(state->suggestion_list, LB_RESETCONTENT, 0, 0);
    const auto suggestions = life_orchestrator::app::suggest_application_commands(get_control_text(state->palette_edit));
    for (const auto& suggestion : suggestions) {
        const auto widened = widen(suggestion);
        SendMessageW(state->suggestion_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widened.c_str()));
    }
    if (!suggestions.empty()) {
        SendMessageW(state->suggestion_list, LB_SETCURSEL, 0, 0);
    }
}

void execute_args(HWND hwnd, const std::vector<std::string>& args) {
    auto* state = state_from(hwnd);
    if (state == nullptr) return;

    auto forwarded = args;
    forwarded.push_back("--quiet-startup");
    const auto result = life_orchestrator::app::invoke_application_command(forwarded, state->environment_data_root, state->working_root);

    std::string rendered;
    rendered += "exit_code=" + std::to_string(result.exit_code) + "\n";
    if (!result.standard_output.empty()) {
        rendered += "\n[stdout]\n";
        rendered += result.standard_output;
    }
    if (!result.standard_error.empty()) {
        rendered += "\n[stderr]\n";
        rendered += result.standard_error;
    }

    set_output(hwnd, rendered);
    set_status(hwnd, result.exit_code == 0 ? "Last command succeeded." : "Last command failed.");
}

void execute_operator_query(HWND hwnd) {
    auto* state = state_from(hwnd);
    if (state == nullptr || state->command_edit == nullptr) return;

    const auto command = get_control_text(state->command_edit);
    if (command.empty()) {
        set_status(hwnd, "Enter a command or natural-language query.");
        return;
    }

    execute_args(hwnd, {"operator-query", "--input", command});
}

void apply_selected_suggestion(HWND hwnd) {
    auto* state = state_from(hwnd);
    if (state == nullptr || state->suggestion_list == nullptr || state->command_edit == nullptr) return;

    const auto index = static_cast<int>(SendMessageW(state->suggestion_list, LB_GETCURSEL, 0, 0));
    if (index == LB_ERR) {
        set_status(hwnd, "Select a suggestion first.");
        return;
    }

    const auto length = static_cast<int>(SendMessageW(state->suggestion_list, LB_GETTEXTLEN, static_cast<WPARAM>(index), 0));
    std::wstring buffer(static_cast<std::size_t>(length + 1), L'\0');
    const auto written = static_cast<int>(SendMessageW(state->suggestion_list,
                                                       LB_GETTEXT,
                                                       static_cast<WPARAM>(index),
                                                       reinterpret_cast<LPARAM>(buffer.data())));
    buffer.resize(static_cast<std::size_t>(written));

    auto selected = narrow(buffer);
    if (const auto alias_separator = selected.find("=>"); alias_separator != std::string::npos) {
        selected = selected.substr(0, alias_separator);
    }

    set_control_text(state->command_edit, selected);
    set_status(hwnd, "Suggestion copied to command input.");
}

void layout_controls(HWND hwnd) {
    RECT client{};
    GetClientRect(hwnd, &client);

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = 12;
    const int row_height = 24;
    const int button_width = 160;
    const int right_column_width = 260;
    const int left_column_width = width - (margin * 3) - right_column_width;
    const int output_height = height - (margin * 6) - (row_height * 3) - 160;

    MoveWindow(GetDlgItem(hwnd, kControlQuickLabel), margin, margin, left_column_width, row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlQuickCombo), margin, margin + row_height + 4, left_column_width - button_width - 8, row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlRunQuickButton), margin + left_column_width - button_width, margin + row_height + 4, button_width, row_height, TRUE);

    MoveWindow(GetDlgItem(hwnd, kControlPaletteLabel), width - right_column_width - margin, margin, right_column_width, row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlPaletteEdit), width - right_column_width - margin, margin + row_height + 4, right_column_width, row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlSuggestionList), width - right_column_width - margin, margin + (row_height * 2) + 8, right_column_width, 120, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlUseSuggestionButton), width - right_column_width - margin, margin + (row_height * 2) + 132, right_column_width, row_height, TRUE);

    const int command_top = margin + (row_height * 2) + 36;
    MoveWindow(GetDlgItem(hwnd, kControlCommandLabel), margin, command_top, left_column_width, row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlCommandEdit), margin, command_top + row_height + 4, left_column_width - (button_width * 2) - 8, row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlRunCommandButton), margin + left_column_width - (button_width * 2) - 4, command_top + row_height + 4, button_width, row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlRefreshStatusButton), margin + left_column_width - button_width, command_top + row_height + 4, button_width, row_height, TRUE);

    const int output_top = command_top + (row_height * 2) + 12;
    MoveWindow(GetDlgItem(hwnd, kControlStatusText), margin, output_top, width - (margin * 2), row_height, TRUE);
    MoveWindow(GetDlgItem(hwnd, kControlOutputEdit), margin, output_top + row_height + 4, width - (margin * 2), (std::max)(120, output_height), TRUE);
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_CREATE: {
            auto* state = state_from(hwnd);
            if (state == nullptr) return -1;

            CreateWindowExW(0, L"STATIC", L"Quick visibility views", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, control_menu_id(kControlQuickLabel), nullptr, nullptr);
            state->quick_combo = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 0, 0, 0, 0, hwnd, control_menu_id(kControlQuickCombo), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Run Selected View", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, control_menu_id(kControlRunQuickButton), nullptr, nullptr);

            CreateWindowExW(0, L"STATIC", L"Command palette", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, control_menu_id(kControlPaletteLabel), nullptr, nullptr);
            state->palette_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, control_menu_id(kControlPaletteEdit), nullptr, nullptr);
            state->suggestion_list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL, 0, 0, 0, 0, hwnd, control_menu_id(kControlSuggestionList), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Copy Selected Suggestion", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, control_menu_id(kControlUseSuggestionButton), nullptr, nullptr);

            CreateWindowExW(0, L"STATIC", L"Operator command or natural-language input", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, control_menu_id(kControlCommandLabel), nullptr, nullptr);
            state->command_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"status", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, control_menu_id(kControlCommandEdit), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Run Operator Query", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, control_menu_id(kControlRunCommandButton), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Refresh Status", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, control_menu_id(kControlRefreshStatusButton), nullptr, nullptr);

            state->status_text = CreateWindowExW(0, L"STATIC", L"Ready.", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, control_menu_id(kControlStatusText), nullptr, nullptr);
            state->output_edit = CreateWindowExW(WS_EX_CLIENTEDGE,
                                                 L"EDIT",
                                                 L"",
                                                 WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL | WS_HSCROLL,
                                                 0,
                                                 0,
                                                 0,
                                                 0,
                                                 hwnd,
                                                 control_menu_id(kControlOutputEdit),
                                                 nullptr,
                                                 nullptr);

            for (const auto& command : quick_commands()) {
                SendMessageW(state->quick_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(command.label));
            }
            SendMessageW(state->quick_combo, CB_SETCURSEL, 0, 0);

            refresh_suggestions(hwnd);
            layout_controls(hwnd);
            execute_args(hwnd, {"status"});
            return 0;
        }
        case WM_SIZE:
            layout_controls(hwnd);
            return 0;
        case WM_COMMAND: {
            const auto control_id = LOWORD(w_param);
            const auto notify_code = HIWORD(w_param);

            switch (control_id) {
                case kControlRunQuickButton: {
                    auto* state = state_from(hwnd);
                    if (state == nullptr || state->quick_combo == nullptr) return 0;

                    const auto index = static_cast<int>(SendMessageW(state->quick_combo, CB_GETCURSEL, 0, 0));
                    if (index >= 0 && index < static_cast<int>(quick_commands().size())) {
                        execute_args(hwnd, quick_commands()[static_cast<std::size_t>(index)].args);
                    }
                    return 0;
                }
                case kControlRunCommandButton:
                    execute_operator_query(hwnd);
                    return 0;
                case kControlRefreshStatusButton:
                    execute_args(hwnd, {"status"});
                    return 0;
                case kControlUseSuggestionButton:
                    apply_selected_suggestion(hwnd);
                    return 0;
                case kControlPaletteEdit:
                    if (notify_code == EN_CHANGE) refresh_suggestions(hwnd);
                    return 0;
                case kControlSuggestionList:
                    if (notify_code == LBN_DBLCLK) apply_selected_suggestion(hwnd);
                    return 0;
                default:
                    break;
            }
            break;
        }
        case WM_DESTROY: {
            delete state_from(hwnd);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
    }

    return DefWindowProcW(hwnd, message, w_param, l_param);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command) {
    INITCOMMONCONTROLSEX common_controls{};
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&common_controls);

    auto* state = new GuiState{};
    state->working_root = std::filesystem::current_path();
    if (const char* env = std::getenv("LIFE_ORCHESTRATOR_DATA_ROOT"); env != nullptr) {
        state->environment_data_root = env;
    }

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"LifeOrchestratorAdminGuiWindow";
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&window_class);

    HWND window = CreateWindowExW(0,
                                  window_class.lpszClassName,
                                  L"LifeOrchestrator Admin GUI",
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  1080,
                                  720,
                                  nullptr,
                                  nullptr,
                                  instance,
                                  state);
    if (window == nullptr) return 1;

    ShowWindow(window, show_command);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

#endif
