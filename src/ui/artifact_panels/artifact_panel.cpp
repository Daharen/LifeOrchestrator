#include "ui/artifact_panels/artifact_panel.hpp"

namespace life_orchestrator::ui {

ArtifactPanel::ArtifactPanel(std::string panel_id,
                             std::string title,
                             std::string artifact_type,
                             std::vector<CommandInvocation> actions)
    : panel_id_(std::move(panel_id)),
      title_(std::move(title)),
      artifact_type_(std::move(artifact_type)),
      actions_(std::move(actions)) {}

const std::string& ArtifactPanel::panel_id() const { return panel_id_; }
const std::string& ArtifactPanel::title() const { return title_; }
const std::string& ArtifactPanel::artifact_type() const { return artifact_type_; }
const std::vector<CommandInvocation>& ArtifactPanel::actions() const { return actions_; }

std::vector<std::string> ArtifactPanel::query_command_args(std::optional<std::size_t> limit) const {
    std::vector<std::string> args = {"artifact.query", "--artifact-type", artifact_type_};
    if (limit.has_value()) args.insert(args.end(), {"--limit", std::to_string(*limit)});
    return args;
}

std::vector<ArtifactRenderModel> ArtifactPanel::load(const std::string& environment_data_root,
                                                     const std::filesystem::path& working_root,
                                                     std::optional<std::size_t> limit) const {
    std::vector<ArtifactRenderModel> models;
    const auto result = app::invoke_application_command(query_command_args(limit), environment_data_root, working_root);
    if (result.exit_code != 0) return models;

    app::ArtifactEnvelope current{artifact_type_, {}, {}, {}};
    bool open = false;
    std::istringstream input(result.standard_output);
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("artifact_type=", 0) == 0) {
            if (open) models.push_back(render_artifact(current));
            current = {line.substr(14), {}, {}, {}};
            open = true;
        } else if (line.rfind("artifact_id=", 0) == 0) {
            current.artifact_id = line.substr(12);
        } else if (open) {
            const auto pos = line.find('=');
            if (pos != std::string::npos && line.find(' ') != 0) current.fields[line.substr(0, pos)] = line.substr(pos + 1);
        }
    }
    if (open) models.push_back(render_artifact(current));
    return models;
}

}  // namespace life_orchestrator::ui
