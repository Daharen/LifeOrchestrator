#pragma once

#include <string>
#include <vector>

namespace life_orchestrator::app::provider_setup {

struct ProviderSetupProviderSummary {
    std::string provider_name;
    std::string display_name;
    std::string model_name;
    std::string secret_source;
    std::string redacted_secret_status;
    bool enabled = false;
    std::string status;
};

struct ProviderSetupUpsertRequest {
    std::string provider_name;
    std::string display_name;
    std::string model_name;
    std::string secret_source;
    std::string api_key;
    std::string env_var_name;
    std::string existing_secret_reference;
    bool enabled = true;
};

struct ProviderSetupTestResult {
    bool ok = false;
    std::string summary;
    std::string safe_details;
};

}  // namespace life_orchestrator::app::provider_setup
