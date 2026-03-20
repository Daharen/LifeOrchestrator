#ifdef _WIN32
#include "app/assistant_shell/assistant_shell_surface_service.h"
#include "ui/assistant_shell/assistant_shell_controller.h"
#include "ui/assistant_shell/assistant_shell_window.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int show_command) {
    const char* env = std::getenv("LIFE_ORCHESTRATOR_DATA_ROOT");
    const std::string data_root = env == nullptr ? std::string{"artifacts"} : std::string{env};
    auto service = std::make_shared<life_orchestrator::app::assistant_shell::AssistantShellSurfaceService>(std::filesystem::path{data_root}, std::filesystem::current_path(), data_root);
    auto controller = std::make_shared<life_orchestrator::ui::assistant_shell::AssistantShellController>(service);
    life_orchestrator::ui::assistant_shell::AssistantShellWindow window(controller);
    return window.Run(instance, show_command);
}
#endif
