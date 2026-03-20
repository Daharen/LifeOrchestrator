#include "ui/artifact_panels/provider_config_panel.hpp"

namespace life_orchestrator::ui {

ProviderConfigPanel::ProviderConfigPanel()
    : ArtifactPanel("provider_config_panel", "Provider Config", "provider_config_summary", {{"Update Provider", {"integration-set-provider"}}}) {}

}  // namespace life_orchestrator::ui
