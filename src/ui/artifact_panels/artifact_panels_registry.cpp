#include "ui/artifact_panels/artifact_panels_registry.hpp"

namespace life_orchestrator::ui {
std::vector<std::shared_ptr<ArtifactPanel>> build_artifact_panels() {
    return {std::make_shared<ActivityInventoryPanel>(),
            std::make_shared<ProceduralProposalsPanel>(),
            std::make_shared<BehavioralBacklogPanel>(),
            std::make_shared<BehavioralInterventionsPanel>(),
            std::make_shared<SchedulingCandidatesPanel>(),
            std::make_shared<ScheduleProposalsPanel>(),
            std::make_shared<BehavioralReevaluationsPanel>(),
            std::make_shared<ProviderConfigPanel>()};
}
}  // namespace life_orchestrator::ui
