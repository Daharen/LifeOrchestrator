#pragma once

#include "app/app_support/artifact_query_service.hpp"

#include <string>
#include <vector>

namespace life_orchestrator::ui {

struct ArtifactRenderField {
    std::string key;
    std::string value;
    bool collapsible = false;
    std::vector<ArtifactRenderField> children;
};

struct ArtifactRenderModel {
    std::string artifact_type;
    std::string artifact_id;
    std::vector<ArtifactRenderField> fields;
};

ArtifactRenderModel render_artifact(const app::ArtifactEnvelope& artifact);
std::string render_artifact_as_text(const app::ArtifactEnvelope& artifact);

}  // namespace life_orchestrator::ui
