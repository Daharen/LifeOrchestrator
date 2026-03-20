#pragma once

#include "ui/artifact_panels/activity_inventory_panel.hpp"
#include "ui/artifact_panels/procedural_proposals_panel.hpp"
#include "ui/artifact_panels/behavioral_backlog_panel.hpp"
#include "ui/artifact_panels/behavioral_interventions_panel.hpp"
#include "ui/artifact_panels/scheduling_candidates_panel.hpp"
#include "ui/artifact_panels/schedule_proposals_panel.hpp"
#include "ui/artifact_panels/behavioral_reevaluations_panel.hpp"
#include "ui/artifact_panels/provider_config_panel.hpp"

#include <memory>
#include <vector>

namespace life_orchestrator::ui {
std::vector<std::shared_ptr<ArtifactPanel>> build_artifact_panels();
}
