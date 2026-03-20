#include "ui/assistant_shell/assistant_shell_window.h"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace life_orchestrator::ui::assistant_shell {
namespace {
LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    switch (message) {
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProcW(hwnd, message, w_param, l_param);
    }
}
}
AssistantShellWindow::AssistantShellWindow(std::shared_ptr<AssistantShellController> controller) : controller_(std::move(controller)) {}
int AssistantShellWindow::Run(HINSTANCE instance, int show_command) {
    const wchar_t* class_name = L"LifeOrchestratorAssistantShellWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, class_name, L"Life Orchestrator Assistant Shell", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800, nullptr, nullptr, instance, nullptr);
    CreateMenu();
    auto transcript = AssistantShellMessageSurface{};
    transcript.Attach(hwnd, instance);
    auto confirmation = AssistantShellConfirmationSurface{};
    confirmation.Attach(hwnd, instance);
    auto status = AssistantShellStatusBar{};
    status.Attach(hwnd, instance);
    auto tool_panel = AssistantShellToolPanel{};
    tool_panel.Attach(hwnd, instance);
    tool_panel.SetVisible(false);
    CreateWindowExW(0, L"STATIC", L"Life Orchestrator Assistant Shell", WS_CHILD | WS_VISIBLE, 12, 8, 400, 24, hwnd, nullptr, instance, nullptr);
    CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_BORDER, 12, 700, 940, 26, hwnd, nullptr, instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Submit", WS_CHILD | WS_VISIBLE, 960, 700, 90, 26, hwnd, nullptr, instance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Attach", WS_CHILD | WS_VISIBLE, 1060, 700, 90, 26, hwnd, nullptr, instance, nullptr);
    MoveWindow(transcript.handle(), 12, 60, 940, 600, TRUE);
    MoveWindow(confirmation.handle(), 12, 666, 940, 26, TRUE);
    MoveWindow(status.handle(), 12, 734, 1240, 24, TRUE);
    MoveWindow(tool_panel.handle(), 970, 60, 280, 632, TRUE);
    auto menu = CreateMenu();
    for (const wchar_t* title : {L"File", L"Settings", L"Active Interfaces", L"Active Modules", L"API Keys", L"Console", L"Developer Layer", L"Help"}) {
        AppendMenuW(menu, MF_STRING, 0, title);
    }
    SetMenu(hwnd, menu);
    ShowWindow(hwnd, show_command);
    controller_->Start();
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
}  // namespace life_orchestrator::ui::assistant_shell
#endif
