#pragma once

#include "modules/imodule.hpp"

namespace life_orchestrator::coordination {

class SchedulingCoordinationStubModule final : public modules::IModule {
public:
    SchedulingCoordinationStubModule();

    const core::ModuleDescriptor& descriptor() const override;
    bool supports_capability(const core::CapabilityId& capability_id) const override;
    core::ActionResponse execute(const core::ActionRequest& request) override;

private:
    core::ModuleDescriptor descriptor_;
};

}  // namespace life_orchestrator::coordination
