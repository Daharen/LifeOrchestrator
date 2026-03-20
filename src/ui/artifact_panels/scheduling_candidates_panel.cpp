#include "ui/artifact_panels/scheduling_candidates_panel.hpp"

namespace life_orchestrator::ui {

SchedulingCandidatesPanel::SchedulingCandidatesPanel()
    : ArtifactPanel("scheduling_candidates_panel", "Scheduling Candidates", "scheduling_candidates", {{"Generate Proposals", {"scheduling-generate-proposals"}}}) {}

}  // namespace life_orchestrator::ui
