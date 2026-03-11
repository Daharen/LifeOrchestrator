#pragma once

#include "core/contracts.hpp"

namespace life_orchestrator::modules {

class IModule {
public:
    virtual ~IModule() = default;

    virtual const core::ModuleDescriptor& descriptor() const = 0;
    virtual bool supports_capability(const core::CapabilityId& capability_id) const = 0;
    virtual core::ActionResponse execute(const core::ActionRequest& request) = 0;
};

}  // namespace life_orchestrator::modules
