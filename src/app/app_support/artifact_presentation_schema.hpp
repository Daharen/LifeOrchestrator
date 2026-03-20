#pragma once

#include <string>
#include <vector>

namespace life_orchestrator::app {

struct ArtifactFieldSpec {
    std::string field_key;
    std::string display_label;
};

struct ArtifactFieldGroupSpec {
    std::string group_key;
    std::string display_label;
    bool collapsible = false;
    std::vector<ArtifactFieldSpec> fields;
};

struct ArtifactActionSpec {
    std::string action_id;
    std::string display_label;
    std::string command_target;
};

struct ArtifactPresentationSchema {
    std::string artifact_type_key;
    std::string display_title;
    std::vector<ArtifactFieldSpec> summary_fields;
    std::vector<ArtifactFieldSpec> detail_fields;
    std::vector<ArtifactFieldGroupSpec> metadata_groups;
    std::string empty_state_text;
    std::vector<ArtifactActionSpec> allowed_panel_actions;
};

}  // namespace life_orchestrator::app
