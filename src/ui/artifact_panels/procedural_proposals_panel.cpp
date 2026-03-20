#include "ui/artifact_panels/procedural_proposals_panel.hpp"

namespace life_orchestrator::ui {

ProceduralProposalsPanel::ProceduralProposalsPanel()
    : ArtifactPanel("procedural_proposals_panel", "Procedural Proposals", "procedural_proposals", {{"Run Procedural Audit", {"procedural-run-audit"}}}) {}

}  // namespace life_orchestrator::ui
