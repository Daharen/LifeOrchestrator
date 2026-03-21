#pragma once

#include "app/application_bootstrap.hpp"
#include "app/provider_setup/provider_setup_contracts.h"

#include <filesystem>
#include <string>
#include <vector>

namespace life_orchestrator::app::provider_setup {

class ProviderSetupService {
public:
    ProviderSetupService(std::filesystem::path data_root,
                         std::filesystem::path working_root = std::filesystem::current_path(),
                         std::string environment_data_root = {});

    std::vector<ProviderSetupProviderSummary> ListProviders() const;
    ApplicationInvocationResult SaveProvider(const ProviderSetupUpsertRequest& request) const;
    ProviderSetupTestResult TestProvider(const std::string& provider_name) const;
private:
    std::filesystem::path data_root_;
    std::filesystem::path working_root_;
    std::string environment_data_root_;
};

}  // namespace life_orchestrator::app::provider_setup
