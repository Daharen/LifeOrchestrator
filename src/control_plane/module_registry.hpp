#pragma once

#include "core/contracts.hpp"
#include "modules/imodule.hpp"

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace life_orchestrator::control_plane {

struct RegistryResult {
    bool ok;
    core::ExecutionStatus status;
    std::string message;
};

class ModuleRegistry {
public:
    RegistryResult register_module(const std::shared_ptr<modules::IModule>& module);
    std::shared_ptr<modules::IModule> find_module_by_id(const core::ModuleId& module_id) const;
    std::shared_ptr<modules::IModule> find_module_by_capability(const core::CapabilityId& capability_id) const;
    std::vector<std::shared_ptr<modules::IModule>> all_modules() const;
    bool capability_exists(const core::CapabilityId& capability_id) const;

private:
    std::unordered_map<core::ModuleId, std::shared_ptr<modules::IModule>> modules_by_id_;
    std::unordered_map<core::CapabilityId, core::ModuleId> capability_to_module_;
};

}  // namespace life_orchestrator::control_plane
