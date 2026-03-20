#pragma once

#include "core/memory_service.hpp"
#include "integration/integration_configuration_repository.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace life_orchestrator::app {

struct ArtifactEnvelope {
    std::string artifact_type;
    std::string artifact_id;
    core::StringMap fields;
    std::unordered_map<std::string, core::StringMap> nested_metadata;
};

struct ArtifactQueryRequest {
    std::string artifact_type;
    std::optional<std::size_t> limit;
};

struct ArtifactQueryResponse {
    std::vector<ArtifactEnvelope> artifacts;
};

struct ArtifactQueryResult {
    bool ok;
    std::string message;
    std::optional<ArtifactQueryResponse> value;
};

bool is_supported_artifact_type(const std::string& artifact_type);
std::vector<std::string> supported_artifact_types();

class ArtifactQueryService {
public:
    ArtifactQueryService(core::MemoryService& memory_service,
                         integration::IntegrationConfigurationRepository& integration_repository,
                         const std::filesystem::path& data_root);

    ArtifactQueryResult query(const ArtifactQueryRequest& request) const;

private:
    core::MemoryService& memory_service_;
    integration::IntegrationConfigurationRepository& integration_repository_;
    std::filesystem::path data_root_;
};

}  // namespace life_orchestrator::app
