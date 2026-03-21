#pragma once
#include "app/provider_setup/provider_setup_service.h"
#include <memory>
namespace life_orchestrator::ui::provider_setup { class ProviderSetupController { public: explicit ProviderSetupController(std::shared_ptr<life_orchestrator::app::provider_setup::ProviderSetupService> service): service_(std::move(service)) {} private: std::shared_ptr<life_orchestrator::app::provider_setup::ProviderSetupService> service_; }; }
