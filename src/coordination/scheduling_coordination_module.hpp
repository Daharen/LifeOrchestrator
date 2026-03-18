#pragma once

#include "coordination/scheduling_engine.hpp"
#include "core/memory_service.hpp"
#include "modules/imodule.hpp"

namespace life_orchestrator::coordination {

class SchedulingCoordinationModule final : public modules::IModule {
public:
    explicit SchedulingCoordinationModule(core::MemoryService* memory_service);

    const core::ModuleDescriptor& descriptor() const override;
    bool supports_capability(const core::CapabilityId& capability_id) const override;
    core::ActionResponse execute(const core::ActionRequest& request) override;

private:
    core::ActionResponse add_commitment(const core::ActionRequest& request);
    core::ActionResponse detect_conflicts(const core::ActionRequest& request);
    core::ActionResponse propose_time_blocks(const core::ActionRequest& request);
    core::ActionResponse commit_proposal(const core::ActionRequest& request);
    core::ActionResponse list_schedule_window(const core::ActionRequest& request);

    core::ModuleDescriptor descriptor_;
    core::MemoryService* memory_service_;
    SchedulingEngine engine_;
};

}  // namespace life_orchestrator::coordination
