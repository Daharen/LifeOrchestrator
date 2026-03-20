#pragma once

#include "app/app_support/artifact_presentation_schema.hpp"
#include "app/app_support/artifact_query_service.hpp"

#include <string>
#include <vector>

namespace life_orchestrator::ui {

struct ArtifactRenderField {
    std::string key;
    std::string label;
    std::string value;
    bool collapsible = false;
    std::vector<ArtifactRenderField> children;
};

struct ArtifactRenderModel {
    std::string artifact_type;
    std::string artifact_id;
    std::string title;
    std::string empty_state_text;
    std::vector<ArtifactRenderField> summary_fields;
    std::vector<ArtifactRenderField> detail_fields;
    std::vector<ArtifactRenderField> metadata_groups;
};

ArtifactRenderModel render_artifact(const app::ArtifactEnvelope& artifact,
                                    const app::ArtifactPresentationSchema& schema);
std::string render_artifact_as_text(const app::ArtifactEnvelope& artifact,
                                    const app::ArtifactPresentationSchema& schema);

}  // namespace life_orchestrator::ui
