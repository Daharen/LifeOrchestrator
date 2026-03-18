#pragma once

#include "coordination/behavioral_triage_engine.hpp"
#include "core/memory_service.hpp"
#include "modules/imodule.hpp"

namespace life_orchestrator::coordination {

class BehavioralTriageModule final : public modules::IModule {
public:
    explicit BehavioralTriageModule(core::MemoryService* memory_service);

    const core::ModuleDescriptor& descriptor() const override;
    bool supports_capability(const core::CapabilityId& capability_id) const override;
    core::ActionResponse execute(const core::ActionRequest& request) override;

private:
    core::ActionResponse record_state(const core::ActionRequest& request);
    core::ActionResponse triage_proposals(const core::ActionRequest& request, bool backlog_only = false);
    core::ActionResponse list_backlog(const core::ActionRequest& request);
    core::ActionResponse reevaluate_backlog(const core::ActionRequest& request);
    core::ActionResponse list_next_interventions(const core::ActionRequest& request);

    core::ModuleDescriptor descriptor_;
    core::MemoryService* memory_service_;
    BehavioralTriageEngine engine_;
};

}  // namespace life_orchestrator::coordination
