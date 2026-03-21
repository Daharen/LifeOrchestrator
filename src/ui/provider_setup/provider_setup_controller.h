#pragma once

#include "app/provider_setup/provider_setup_service.h"

#include <memory>
#include <string>
#include <vector>

namespace life_orchestrator::ui::provider_setup {

class ProviderSetupController {
public:
    explicit ProviderSetupController(std::shared_ptr<life_orchestrator::app::provider_setup::ProviderSetupService> service);

    std::vector<life_orchestrator::app::provider_setup::ProviderSetupProviderSummary> ListProviders() const;
    life_orchestrator::app::ApplicationInvocationResult SaveProvider(const life_orchestrator::app::provider_setup::ProviderSetupUpsertRequest& request) const;
    life_orchestrator::app::provider_setup::ProviderSetupTestResult TestProvider(const std::string& provider_name) const;

private:
    std::shared_ptr<life_orchestrator::app::provider_setup::ProviderSetupService> service_;
};

}  // namespace life_orchestrator::ui::provider_setup
