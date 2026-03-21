#include "app/provider_setup/provider_setup_service.h"

#include <sstream>

namespace life_orchestrator::app::provider_setup {
namespace {
std::string value_for_key(const std::string& text, const std::string& key) {
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) if (line.rfind(key + "=", 0) == 0) return line.substr(key.size() + 1);
    return {};
}
}

ProviderSetupService::ProviderSetupService(std::filesystem::path data_root,
                                           std::filesystem::path working_root,
                                           std::string environment_data_root)
    : data_root_(std::move(data_root)), working_root_(std::move(working_root)), environment_data_root_(std::move(environment_data_root)) {}

std::vector<ProviderSetupProviderSummary> ProviderSetupService::ListProviders() const {
    std::vector<ProviderSetupProviderSummary> providers;
    auto result = invoke_application_command({"integration-list-providers", "--data-root=" + data_root_.string(), "--quiet-startup"}, environment_data_root_, working_root_);
    std::istringstream input(result.standard_output);
    std::string line;
    ProviderSetupProviderSummary current;
    while (std::getline(input, line)) {
        if (line.rfind("provider_name=", 0) == 0) {
            if (!current.provider_name.empty()) providers.push_back(current);
            current = {};
            current.provider_name = line.substr(14);
        } else if (line.rfind("enabled=", 0) == 0) current.enabled = line.substr(8) == "true";
        else if (line.rfind("status=", 0) == 0) current.status = line.substr(7);
        else if (line.rfind("model_name=", 0) == 0) current.model_name = line.substr(11);
        else if (line.rfind("secret_source=", 0) == 0) current.secret_source = line.substr(14);
        else if (line.rfind("api_key_redacted=", 0) == 0) current.redacted_secret_status = line.substr(17);
    }
    if (!current.provider_name.empty()) providers.push_back(current);
    return providers;
}

ApplicationInvocationResult ProviderSetupService::SaveProvider(const ProviderSetupUpsertRequest& request) const {
    std::vector<std::string> args{"integration-set-provider", "--data-root=" + data_root_.string(), "--quiet-startup", "--provider-name", request.provider_name, "--model-name", request.model_name, "--secret-source", request.secret_source};
    if (!request.display_name.empty()) args.insert(args.end(), {"--display-name", request.display_name});
    args.insert(args.end(), {"--enabled", request.enabled ? "true" : "false"});
    if (request.secret_source == "env") args.insert(args.end(), {"--env-var", request.env_var_name});
    else if (request.secret_source == "existing") args.insert(args.end(), {"--secret-ref", request.existing_secret_reference});
    else args.insert(args.end(), {"--api-key", request.api_key});
    return invoke_application_command(args, environment_data_root_, working_root_);
}

ProviderSetupTestResult ProviderSetupService::TestProvider(const std::string& provider_name) const {
    auto result = invoke_application_command({"integration-test-provider", "--data-root=" + data_root_.string(), "--quiet-startup", "--provider-name", provider_name}, environment_data_root_, working_root_);
    return {result.exit_code == 0, value_for_key(result.standard_output + "\n" + result.standard_error, "message"), result.standard_output + result.standard_error};
}

}  // namespace life_orchestrator::app::provider_setup
