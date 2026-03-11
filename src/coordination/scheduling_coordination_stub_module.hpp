#pragma once

#include "core/memory_service.hpp"
#include "modules/imodule.hpp"

namespace life_orchestrator::coordination {

class SchedulingCoordinationStubModule final : public modules::IModule {
public:
    explicit SchedulingCoordinationStubModule(core::MemoryService* memory_service = nullptr);

    const core::ModuleDescriptor& descriptor() const override;
    bool supports_capability(const core::CapabilityId& capability_id) const override;
    core::ActionResponse execute(const core::ActionRequest& request) override;

private:
    core::ModuleDescriptor descriptor_;
    core::MemoryService* memory_service_;
};

}  // namespace life_orchestrator::coordination
