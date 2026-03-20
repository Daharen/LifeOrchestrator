#include "app/app_support/artifact_refresh_registry.hpp"

#include "app/app_support/action_result_view.hpp"
#include "app/app_support/artifact_presentation_registry.hpp"

namespace life_orchestrator::app {

std::vector<ArtifactRefreshTarget> list_artifact_refresh_targets(const ActionFormSpec& spec) {
    std::vector<ArtifactRefreshTarget> targets;
    targets.reserve(spec.refresh_targets.size());
    for (const auto& artifact_type : spec.refresh_targets) {
        auto schema = find_artifact_presentation_schema(artifact_type);
        targets.push_back({artifact_type, schema.has_value() ? schema->display_title : artifact_type});
    }
    return targets;
}

}  // namespace life_orchestrator::app
