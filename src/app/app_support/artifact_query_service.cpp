#include "app/app_support/artifact_query_service.hpp"

#include "app/app_support/memory/activity_repository.hpp"
#include "app/app_support/memory/behavior_repository.hpp"
#include "app/app_support/memory/integration_repository.hpp"
#include "app/app_support/memory/procedural_repository.hpp"
#include "app/app_support/memory/scheduling_repository.hpp"

#include <algorithm>

namespace life_orchestrator::app {
namespace {
const std::vector<std::string>& artifact_type_table() {
    static const std::vector<std::string> types = {
        "activity_inventory",
        "procedural_proposals",
        "behavioral_backlog",
        "behavioral_interventions",
        "scheduling_candidates",
        "schedule_proposals",
        "behavioral_reevaluations",
        "provider_config_summary",
    };
    return types;
}
}

bool is_supported_artifact_type(const std::string& artifact_type) {
    const auto& types = artifact_type_table();
    return std::find(types.begin(), types.end(), artifact_type) != types.end();
}

std::vector<std::string> supported_artifact_types() {
    return artifact_type_table();
}

ArtifactQueryService::ArtifactQueryService(core::MemoryService& memory_service,
                                           integration::IntegrationConfigurationRepository& integration_repository,
                                           const std::filesystem::path& data_root)
    : memory_service_(memory_service), integration_repository_(integration_repository), data_root_(data_root) {}

ArtifactQueryResult ArtifactQueryService::query(const ArtifactQueryRequest& request) const {
    if (!is_supported_artifact_type(request.artifact_type)) {
        return {false, "unsupported_artifact_type", std::nullopt};
    }

    ArtifactQueryResponse response;
    if (request.artifact_type == "activity_inventory") {
        response.artifacts = memory::list_activity_inventory(memory_service_);
    } else if (request.artifact_type == "procedural_proposals") {
        response.artifacts = memory::list_procedural_proposals(memory_service_);
    } else if (request.artifact_type == "behavioral_backlog") {
        response.artifacts = memory::list_behavioral_backlog(memory_service_);
    } else if (request.artifact_type == "behavioral_interventions") {
        response.artifacts = memory::list_behavioral_interventions(memory_service_);
    } else if (request.artifact_type == "scheduling_candidates") {
        response.artifacts = memory::list_scheduling_candidates(memory_service_);
    } else if (request.artifact_type == "schedule_proposals") {
        response.artifacts = memory::list_schedule_proposals(memory_service_);
    } else if (request.artifact_type == "behavioral_reevaluations") {
        response.artifacts = memory::list_behavioral_reevaluations(memory_service_);
    } else if (request.artifact_type == "provider_config_summary") {
        response.artifacts = memory::list_provider_config_summary(integration_repository_, data_root_);
    }

    if (request.limit.has_value() && response.artifacts.size() > *request.limit) {
        response.artifacts.resize(*request.limit);
    }

    return {true, {}, response};
}

}  // namespace life_orchestrator::app
