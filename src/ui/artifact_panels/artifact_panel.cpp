#include "ui/artifact_panels/artifact_panel.hpp"

#include <sstream>
#include <stdexcept>

namespace life_orchestrator::ui {

ArtifactPanel::ArtifactPanel(std::string artifact_type) {
    auto schema = app::find_artifact_presentation_schema(artifact_type);
    if (!schema.has_value()) throw std::runtime_error("missing artifact presentation schema: " + artifact_type);
    schema_ = *schema;
    for (const auto& action : schema_.allowed_panel_actions) {
        actions_.push_back({action.action_id, action.display_label, {action.command_target}});
    }
}

std::string ArtifactPanel::panel_id() const { return schema_.artifact_type_key + "_panel"; }
std::string ArtifactPanel::title() const { return schema_.display_title; }
const std::string& ArtifactPanel::artifact_type() const { return schema_.artifact_type_key; }
const std::vector<CommandInvocation>& ArtifactPanel::actions() const { return actions_; }
const app::ArtifactPresentationSchema& ArtifactPanel::schema() const { return schema_; }

std::vector<std::string> ArtifactPanel::query_command_args(std::optional<std::size_t> limit) const {
    std::vector<std::string> args = {"artifact.query", "--artifact-type", artifact_type()};
    if (limit.has_value()) args.insert(args.end(), {"--limit", std::to_string(*limit)});
    return args;
}

std::vector<ArtifactRenderModel> ArtifactPanel::load(const std::string& environment_data_root,
                                                     const std::filesystem::path& working_root,
                                                     std::optional<std::size_t> limit) const {
    std::vector<ArtifactRenderModel> models;
    const auto result = app::invoke_application_command(query_command_args(limit), environment_data_root, working_root);
    if (result.exit_code != 0) return models;

    app::ArtifactEnvelope current{artifact_type(), {}, {}, {}};
    bool open = false;
    std::istringstream input(result.standard_output);
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("artifact_type=", 0) == 0) {
            if (open) models.push_back(render_artifact(current, schema_));
            current = {line.substr(14), {}, {}, {}};
            open = true;
        } else if (line.rfind("artifact_id=", 0) == 0) {
            current.artifact_id = line.substr(12);
        } else if (open) {
            const auto pos = line.find('=');
            if (pos != std::string::npos && line.find(' ') != 0) current.fields[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }
    if (open) models.push_back(render_artifact(current, schema_));
    return models;
}

}  // namespace life_orchestrator::ui
