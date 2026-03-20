#pragma once

#include "app/app_support/action_form_spec.hpp"

#include <string>
#include <vector>

namespace life_orchestrator::app {

struct ArtifactRefreshTarget {
    std::string artifact_type;
    std::string display_label;
};

std::vector<ArtifactRefreshTarget> list_artifact_refresh_targets(const ActionFormSpec& spec);

}  // namespace life_orchestrator::app
