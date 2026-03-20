#pragma once

#include "app/application_bootstrap.hpp"
#include "ui/artifact_renderer.hpp"

#include <optional>
#include <string>
#include <vector>

namespace life_orchestrator::ui {

struct CommandInvocation {
    std::string button_label;
    std::vector<std::string> command_args;
};

class ArtifactPanel {
public:
    ArtifactPanel(std::string panel_id,
                  std::string title,
                  std::string artifact_type,
                  std::vector<CommandInvocation> actions = {});
    virtual ~ArtifactPanel() = default;

    const std::string& panel_id() const;
    const std::string& title() const;
    const std::string& artifact_type() const;
    const std::vector<CommandInvocation>& actions() const;

    std::vector<ArtifactRenderModel> load(const std::string& environment_data_root,
                                          const std::filesystem::path& working_root,
                                          std::optional<std::size_t> limit = std::nullopt) const;

protected:
    std::vector<std::string> query_command_args(std::optional<std::size_t> limit) const;

private:
    std::string panel_id_;
    std::string title_;
    std::string artifact_type_;
    std::vector<CommandInvocation> actions_;
};

}  // namespace life_orchestrator::ui
