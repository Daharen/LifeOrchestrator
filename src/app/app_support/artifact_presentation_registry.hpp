#pragma once

#include "app/app_support/artifact_presentation_schema.hpp"

#include <optional>
#include <string>
#include <vector>

namespace life_orchestrator::app {

const std::vector<ArtifactPresentationSchema>& list_artifact_presentation_schemas();
std::vector<std::string> list_artifact_panel_definition_ids();
std::optional<ArtifactPresentationSchema> find_artifact_presentation_schema(const std::string& artifact_type);

}  // namespace life_orchestrator::app
