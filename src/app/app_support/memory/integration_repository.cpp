#include "app/app_support/memory/integration_repository.hpp"

#include <cstdlib>
#include <fstream>

namespace life_orchestrator::app::memory {
namespace {
std::string read_secret(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::string value;
    std::getline(in, value);
    return value;
}
std::string read_env_var(const std::string& name) {
    if (name.empty()) return {};
    const char* value = std::getenv(name.c_str());
    return value == nullptr ? std::string{} : std::string{value};
}
std::string redact_secret(const std::string& value) {
    if (value.empty()) return "unset";
    if (value.size() <= 4) return "****";
    return value.substr(0, 2) + "***" + value.substr(value.size() - 2);
}
}

std::vector<ArtifactEnvelope> list_provider_config_summary(const integration::IntegrationConfigurationRepository& integration_repository,
                                                          const std::filesystem::path& data_root) {
    std::vector<ArtifactEnvelope> artifacts;
    for (const auto& record : integration_repository.list_all()) {
        const auto secret = record.credential_storage_mode == core::CredentialStorageMode::InlinePlaceholderOnly
                                ? read_env_var(record.non_secret_settings.contains("env_var_name") ? record.non_secret_settings.at("env_var_name") : record.credential_reference)
                                : read_secret(data_root / record.credential_reference);
        artifacts.push_back({"provider_config_summary",
                             record.integration_config_id,
                             {{"integration_config_id", record.integration_config_id},
                              {"provider_name", record.integration_id},
                              {"display_name", record.display_name},
                              {"model_name", record.non_secret_settings.contains("model_name") ? record.non_secret_settings.at("model_name") : std::string{"unset"}},
                              {"secret_source", record.non_secret_settings.contains("secret_source") ? record.non_secret_settings.at("secret_source") : std::string{record.credential_storage_mode == core::CredentialStorageMode::InlinePlaceholderOnly ? "env" : "direct"}},
                              {"env_var_name", record.non_secret_settings.contains("env_var_name") ? record.non_secret_settings.at("env_var_name") : std::string{"unset"}},
                              {"enabled", record.enabled ? "true" : "false"},
                              {"status", core::to_string(record.status)},
                              {"credential_storage_mode", core::to_string(record.credential_storage_mode)},
                              {"credential_reference", record.credential_reference},
                              {"api_key_redacted", redact_secret(secret)},
                              {"created_at", record.created_at},
                              {"updated_at", record.updated_at},
                              {"version", std::to_string(record.version)}},
                             {{"non_secret_settings", record.non_secret_settings}}});
    }
    return artifacts;
}
}  // namespace life_orchestrator::app::memory
