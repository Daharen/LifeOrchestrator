#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "app/app_support/action_form_registry.hpp"
#include "app/app_support/action_result_view.hpp"
#include "app/application_bootstrap.hpp"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <optional>
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
constexpr int kDialogFirstFieldLabel = 2000;
constexpr int kDialogFirstFieldEdit = 3000;
constexpr int kDialogSubmitButton = 4000;
constexpr int kDialogCancelButton = 4001;

HMENU control_menu_id(const int value) {
    return reinterpret_cast<HMENU>(static_cast<intptr_t>(value));
}

struct QuickCommand {
    std::wstring label;
    std::vector<std::string> args;
};

struct ActionDialogFieldControl {
    life_orchestrator::app::ActionFormFieldSpec field;
    HWND label = nullptr;
    HWND input = nullptr;
    HWND help = nullptr;
};

struct ActionDialogState {
    life_orchestrator::app::ActionFormSpec spec;
    std::vector<ActionDialogFieldControl> fields;
    std::vector<std::string> submitted_args;
    bool submitted = false;
};

std::wstring widen(const std::string& value);
std::string get_control_text(HWND handle);
void layout_action_dialog(HWND hwnd);

const std::vector<QuickCommand>& quick_commands() {
    static const std::vector<QuickCommand> commands = [] {
        std::vector<QuickCommand> built = {{L"Status", {"status"}},
                                           {L"Activity Inventory", {"procedural-list-activities"}},
                                           {L"Procedural Proposals", {"procedural-list-proposals"}},
                                           {L"Behavioral Backlog", {"behavioral-list-backlog"}},
                                           {L"Behavioral Interventions", {"behavioral-list-interventions"}},
                                           {L"Behavioral Reevaluations", {"behavioral-list-reevaluations"}},
                                           {L"Scheduling Candidates", {"scheduling-list-candidates"}},
                                           {L"Schedule Proposals", {"scheduling-list-proposals"}},
                                           {L"Provider Summary", {"integration-list-providers"}},
                                           {L"Commands", {"commands"}},
                                           {L"Aliases", {"aliases"}}};
        for (const auto& spec : life_orchestrator::app::list_action_form_specs()) {
            built.push_back({widen(spec.display_label), {spec.canonical_command_target, "--help"}});
        }
        return built;
    }();
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

void send_listbox_add_string(HWND handle, const std::wstring& value) {
    SendMessageW(handle, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
}

void send_combobox_add_string(HWND handle, const std::wstring& value) {
    SendMessageW(handle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value.c_str()));
}



std::wstring action_dialog_label_text(const life_orchestrator::app::ActionFormFieldSpec& field) {
    auto label = field.label;
    if (field.required) label += " *";
    return widen(label);
}

std::wstring action_dialog_help_text(const life_orchestrator::app::ActionFormFieldSpec& field) {
    std::string help = field.help_text;
    if (!field.example_value.empty()) {
        if (!help.empty()) help += " ";
        help += "Example: " + field.example_value;
    }
    return widen(help);
}

void set_edit_placeholder(HWND handle, const std::wstring& value) {
    SendMessageW(handle, EM_SETCUEBANNER, 0, reinterpret_cast<LPARAM>(value.c_str()));
}

bool field_is_visible(const life_orchestrator::app::ActionFormFieldSpec& field, const std::vector<ActionDialogFieldControl>& fields) {
    for (const auto& rule : field.visibility_rules) {
        const auto it = std::find_if(fields.begin(), fields.end(), [&](const auto& candidate) { return candidate.field.field_id == rule.controlling_field_id; });
        if (it == fields.end()) return false;
        if (get_control_text(it->input) != rule.expected_value) return false;
    }
    return true;
}

void set_field_visibility(const ActionDialogFieldControl& field, const bool visible) {
    ShowWindow(field.label, visible ? SW_SHOW : SW_HIDE);
    ShowWindow(field.input, visible ? SW_SHOW : SW_HIDE);
    ShowWindow(field.help, visible ? SW_SHOW : SW_HIDE);
}

void update_action_dialog_visibility(HWND hwnd) {
    auto* state = reinterpret_cast<ActionDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) return;
    for (const auto& field : state->fields) set_field_visibility(field, field_is_visible(field.field, state->fields));
    layout_action_dialog(hwnd);
}

void layout_action_dialog(HWND hwnd) {
    auto* state = reinterpret_cast<ActionDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (state == nullptr) return;

    RECT client{};
    GetClientRect(hwnd, &client);
    const int margin = 12;
    const int label_height = 20;
    const int edit_height = 24;
    const int help_height = 32;
    const int field_gap = 8;
    const int content_width = (client.right - client.left) - (margin * 2);
    int y = margin;

    for (auto& field : state->fields) {
        if (!IsWindowVisible(field.label)) continue;
        MoveWindow(field.label, margin, y, content_width, label_height, TRUE);
        y += label_height + 2;
        MoveWindow(field.input, margin, y, content_width, edit_height + (field.field.input_kind == "dropdown" ? 120 : 0), TRUE);
        y += edit_height + (field.field.input_kind == "dropdown" ? 120 : 0) + 2;
        MoveWindow(field.help, margin, y, content_width, help_height, TRUE);
        y += help_height + field_gap;
    }

    const int button_width = 120;
    MoveWindow(GetDlgItem(hwnd, kDialogSubmitButton), client.right - margin - (button_width * 2) - 8, y, button_width, 28, TRUE);
    MoveWindow(GetDlgItem(hwnd, kDialogCancelButton), client.right - margin - button_width, y, button_width, 28, TRUE);
}

LRESULT CALLBACK action_dialog_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_NCCREATE: {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(l_param);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return TRUE;
        }
        case WM_CREATE: {
            auto* state = reinterpret_cast<ActionDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (state == nullptr) return -1;
            state->fields.reserve(state->spec.input_fields.size());
            for (std::size_t index = 0; index < state->spec.input_fields.size(); ++index) {
                const auto& field = state->spec.input_fields[index];
                auto label = CreateWindowExW(0, L"STATIC", action_dialog_label_text(field).c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, control_menu_id(kDialogFirstFieldLabel + static_cast<int>(index)), nullptr, nullptr);
                const auto is_dropdown = field.input_kind == "dropdown";
                const auto class_name = is_dropdown ? L"COMBOBOX" : L"EDIT";
                const DWORD style = is_dropdown ? (WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST) : (WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | (field.input_kind == "password" ? ES_PASSWORD : 0));
                const DWORD ex_style = is_dropdown ? 0 : WS_EX_CLIENTEDGE;
                auto input = CreateWindowExW(ex_style, class_name, L"", style, 0, 0, 0, 0, hwnd, control_menu_id(kDialogFirstFieldEdit + static_cast<int>(index)), nullptr, nullptr);
                auto help = CreateWindowExW(0, L"STATIC", action_dialog_help_text(field).c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, nullptr, nullptr, nullptr);
                if (is_dropdown) {
                    for (const auto& option : field.options) {
                        const auto widened = widen(option.label);
                        const auto idx = SendMessageW(input, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widened.c_str()));
                        SendMessageW(input, CB_SETITEMDATA, idx, reinterpret_cast<LPARAM>(index));
                    }
                    int select_index = 0;
                    for (std::size_t option_index = 0; option_index < field.options.size(); ++option_index) {
                        if (field.options[option_index].value == field.example_value) select_index = static_cast<int>(option_index);
                    }
                    SendMessageW(input, CB_SETCURSEL, select_index, 0);
                } else if (!field.example_value.empty()) set_edit_placeholder(input, widen(field.example_value));
                state->fields.push_back({field, label, input, help});
            }
            CreateWindowExW(0, L"BUTTON", L"Submit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 0, 0, hwnd, control_menu_id(kDialogSubmitButton), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, control_menu_id(kDialogCancelButton), nullptr, nullptr);
            layout_action_dialog(hwnd);
            update_action_dialog_visibility(hwnd);
            if (!state->fields.empty()) SetFocus(state->fields.front().input);
            return 0;
        }
        case WM_SIZE:
            layout_action_dialog(hwnd);
            return 0;
        case WM_COMMAND: {
            switch (LOWORD(w_param)) {
                case kDialogSubmitButton: {
                    auto* state = reinterpret_cast<ActionDialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                    if (state == nullptr) return 0;
                    std::vector<life_orchestrator::app::ActionFormSubmissionField> values;
                    values.reserve(state->fields.size());
                    for (const auto& field : state->fields) values.push_back({field.field.field_id, get_control_text(field.input)});
                    const auto submission = life_orchestrator::app::build_action_form_submission_args(state->spec, values);
                    state->submitted_args = submission.args;
                    state->submitted = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                case kDialogCancelButton:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    if (HIWORD(w_param) == CBN_SELCHANGE) update_action_dialog_visibility(hwnd);
                    break;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

std::optional<std::vector<std::string>> show_action_form_dialog(HWND owner, const life_orchestrator::app::ActionFormSpec& spec) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW dialog_class{};
        dialog_class.cbSize = sizeof(dialog_class);
        dialog_class.lpfnWndProc = action_dialog_proc;
        dialog_class.hInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE));
        dialog_class.lpszClassName = L"LifeOrchestratorActionDialogWindow";
        dialog_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        dialog_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassExW(&dialog_class);
        registered = true;
    }

    ActionDialogState state{};
    state.spec = spec;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME,
                                  L"LifeOrchestratorActionDialogWindow",
                                  widen(spec.display_label).c_str(),
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  520,
                                  static_cast<int>(160 + (spec.input_fields.size() * 88)),
                                  owner,
                                  nullptr,
                                  reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner, GWLP_HINSTANCE)),
                                  &state);
    if (dialog == nullptr) return std::nullopt;

    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);

    if (!state.submitted) return std::nullopt;
    return state.submitted_args;
}

std::string get_control_text(HWND handle) {
    wchar_t class_name[32] = {};
    GetClassNameW(handle, class_name, 31);
    if (std::wstring(class_name) == L"COMBOBOX") {
        const auto selection = SendMessageW(handle, CB_GETCURSEL, 0, 0);
        if (selection == CB_ERR) return {};
        wchar_t buffer[512] = {};
        SendMessageW(handle, CB_GETLBTEXT, selection, reinterpret_cast<LPARAM>(buffer));
        const auto chosen_label = narrow(buffer);
        auto* state = reinterpret_cast<ActionDialogState*>(GetWindowLongPtrW(GetParent(handle), GWLP_USERDATA));
        if (state != nullptr) {
            const int control_id = static_cast<int>(GetDlgCtrlID(handle)) - kDialogFirstFieldEdit;
            if (control_id >= 0 && static_cast<std::size_t>(control_id) < state->fields.size()) {
                const auto& options = state->fields[static_cast<std::size_t>(control_id)].field.options;
                if (static_cast<std::size_t>(selection) < options.size()) return options[static_cast<std::size_t>(selection)].value;
            }
        }
        return chosen_label;
    }
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
        send_listbox_add_string(state->suggestion_list, widened);
    }
    if (!suggestions.empty()) {
        SendMessageW(state->suggestion_list, LB_SETCURSEL, 0, 0);
    }
}


std::string render_action_feedback(const life_orchestrator::app::ActionExecutionFeedback& feedback) {
    std::string rendered;
    rendered += "action_label=" + feedback.result_view.action_label + "\n";
    rendered += "canonical_command=" + feedback.result_view.canonical_command_id + "\n";
    rendered += "status=" + std::string(feedback.result_view.succeeded ? "success" : "failure") + "\n";
    rendered += "exit_code=" + std::to_string(feedback.result_view.exit_code) + "\n";
    if (!feedback.result_view.output_rows.empty()) {
        rendered += "\n[result_rows]\n";
        for (const auto& row : feedback.result_view.output_rows) rendered += row.key + "=" + row.value + "\n";
    } else if (!feedback.result_view.raw_output.empty()) {
        rendered += "\n[raw_output]\n";
        rendered += feedback.result_view.raw_output;
        if (feedback.result_view.raw_output.back() != '\n') rendered += "\n";
    }
    rendered += "\n[next_state_hint]\n" + feedback.result_view.next_state_hint + "\n";
    if (!feedback.refreshed_artifacts.empty()) {
        rendered += "\n[refreshed_artifacts]\n";
        for (const auto& refreshed : feedback.refreshed_artifacts) {
            rendered += "artifact=" + refreshed.target.display_label + "\n";
            rendered += "query_command=";
            for (std::size_t i = 0; i < refreshed.query_args.size(); ++i) {
                if (i != 0) rendered += ' ';
                rendered += refreshed.query_args[i];
            }
            rendered += "\n";
            if (!refreshed.query_result.standard_output.empty()) rendered += refreshed.query_result.standard_output;
            if (!rendered.empty() && rendered.back() != '\n') rendered += "\n";
        }
    }
    return rendered;
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

            CreateWindowExW(0, L"STATIC", L"Assisted operator request", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hwnd, control_menu_id(kControlCommandLabel), nullptr, nullptr);
            state->command_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"status", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 0, 0, 0, 0, hwnd, control_menu_id(kControlCommandEdit), nullptr, nullptr);
            CreateWindowExW(0, L"BUTTON", L"Route Request", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 0, 0, hwnd, control_menu_id(kControlRunCommandButton), nullptr, nullptr);
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
                send_combobox_add_string(state->quick_combo, command.label);
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
                    if (index < 0 || index >= static_cast<int>(quick_commands().size())) return 0;

                    const auto& quick = quick_commands()[static_cast<std::size_t>(index)];
                    if (!quick.args.empty()) {
                        const auto spec = life_orchestrator::app::find_action_form_spec_by_command_target(quick.args.front());
                        if (spec.has_value()) {
                            const auto submitted = show_action_form_dialog(hwnd, *spec);
                            if (!submitted.has_value()) {
                                set_status(hwnd, "Action dialog cancelled.");
                                return 0;
                            }
                            const auto feedback = life_orchestrator::app::execute_action_form_command(*spec, *submitted, state->environment_data_root, state->working_root);
                            set_output(hwnd, render_action_feedback(feedback));
                            set_status(hwnd, feedback.result_view.succeeded ? "Action succeeded and refresh targets updated." : "Action failed; authoritative result preserved.");
                            return 0;
                        }
                    }

                    execute_args(hwnd, quick.args);
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
                    if (HIWORD(w_param) == CBN_SELCHANGE) update_action_dialog_visibility(hwnd);
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
