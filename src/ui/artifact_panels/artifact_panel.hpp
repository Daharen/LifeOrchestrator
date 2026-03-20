#pragma once

#include "app/app_support/action_form_registry.hpp"
#include "app/app_support/artifact_presentation_registry.hpp"
#include "app/application_bootstrap.hpp"
#include "ui/artifact_renderer.hpp"

#include <optional>
#include <string>
#include <vector>

namespace life_orchestrator::ui {

struct CommandInvocation {
    std::string action_id;
    std::string button_label;
    std::vector<std::string> command_args;
};

class ArtifactPanel {
public:
    explicit ArtifactPanel(std::string artifact_type);
    virtual ~ArtifactPanel() = default;

    std::string panel_id() const;
    std::string title() const;
    const std::string& artifact_type() const;
    const std::vector<CommandInvocation>& actions() const;
    const app::ArtifactPresentationSchema& schema() const;

    std::vector<ArtifactRenderModel> load(const std::string& environment_data_root,
                                          const std::filesystem::path& working_root,
                                          std::optional<std::size_t> limit = std::nullopt) const;

protected:
    std::vector<std::string> query_command_args(std::optional<std::size_t> limit) const;

private:
    app::ArtifactPresentationSchema schema_;
    std::vector<CommandInvocation> actions_;
};

}  // namespace life_orchestrator::ui
