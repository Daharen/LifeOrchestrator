#include "ui/artifact_panels/behavioral_backlog_panel.hpp"

namespace life_orchestrator::ui {

BehavioralBacklogPanel::BehavioralBacklogPanel()
    : ArtifactPanel("behavioral_backlog_panel", "Behavioral Backlog", "behavioral_backlog", {{"Reevaluate Backlog", {"behavioral-reevaluate-backlog"}}}) {}

}  // namespace life_orchestrator::ui
