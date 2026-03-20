#pragma once

#include "app/app_support/artifact_query_service.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_provider_config_summary(const integration::IntegrationConfigurationRepository& integration_repository,
                                                          const std::filesystem::path& data_root);
}
