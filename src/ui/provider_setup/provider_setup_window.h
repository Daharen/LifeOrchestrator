#pragma once
#ifdef _WIN32

#include "ui/provider_setup/provider_setup_controller.h"

#include <memory>
#include <string>
#include <vector>
#include <windows.h>

namespace life_orchestrator::ui::provider_setup {

class ProviderSetupWindow {
public:
    explicit ProviderSetupWindow(std::shared_ptr<ProviderSetupController> controller);

    void Show(HINSTANCE instance, HWND owner);
    bool IsOpen() const;
    HWND handle() const;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);

private:
    LRESULT HandleMessage(UINT message, WPARAM w_param, LPARAM l_param);
    void CreateUi();
    void Layout();
    void RefreshProviders();
    void PopulateFromSelection();
    void UpdateSecretSourceUi();
    void SaveCurrentProvider();
    void TestCurrentProvider();
    void SetStatusText(const std::string& text);

    std::shared_ptr<ProviderSetupController> controller_;
    HINSTANCE instance_ = nullptr;
    HWND owner_ = nullptr;
    HWND hwnd_ = nullptr;

    HWND provider_list_ = nullptr;
    HWND provider_name_label_ = nullptr;
    HWND display_name_label_ = nullptr;
    HWND model_name_label_ = nullptr;
    HWND secret_source_label_ = nullptr;
    HWND api_key_label_ = nullptr;
    HWND env_var_label_ = nullptr;
    HWND secret_ref_label_ = nullptr;
    HWND provider_name_edit_ = nullptr;
    HWND display_name_edit_ = nullptr;
    HWND model_name_edit_ = nullptr;
    HWND secret_source_combo_ = nullptr;
    HWND api_key_edit_ = nullptr;
    HWND env_var_edit_ = nullptr;
    HWND secret_ref_edit_ = nullptr;
    HWND enabled_checkbox_ = nullptr;
    HWND refresh_button_ = nullptr;
    HWND save_button_ = nullptr;
    HWND test_button_ = nullptr;
    HWND close_button_ = nullptr;
    HWND status_box_ = nullptr;

    std::vector<life_orchestrator::app::provider_setup::ProviderSetupProviderSummary> providers_;
    SIZE minimum_size_{760, 520};
};

}  // namespace life_orchestrator::ui::provider_setup
#endif
