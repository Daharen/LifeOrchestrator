#include "control_plane/module_registry.hpp"

namespace life_orchestrator::control_plane {

RegistryResult ModuleRegistry::register_module(const std::shared_ptr<modules::IModule>& module) {
    if (!module) {
        return {false, core::ExecutionStatus::InvalidRequest, "Module pointer cannot be null."};
    }

    const auto& descriptor = module->descriptor();
    if (descriptor.module_id.empty()) {
        return {false, core::ExecutionStatus::InvalidRequest, "Module id cannot be empty."};
    }

    if (modules_by_id_.contains(descriptor.module_id)) {
        return {false, core::ExecutionStatus::Rejected, "Duplicate module id registration rejected."};
    }

    for (const auto& capability : descriptor.capabilities) {
        if (capability.empty()) {
            return {false, core::ExecutionStatus::InvalidRequest, "Capability id cannot be empty."};
        }
        if (capability_to_module_.contains(capability)) {
            return {false, core::ExecutionStatus::Rejected,
                    "Duplicate capability ownership registration rejected."};
        }
    }

    modules_by_id_.emplace(descriptor.module_id, module);
    for (const auto& capability : descriptor.capabilities) {
        capability_to_module_.emplace(capability, descriptor.module_id);
    }

    return {true, core::ExecutionStatus::Succeeded, "Module registered."};
}

std::shared_ptr<modules::IModule> ModuleRegistry::find_module_by_id(const core::ModuleId& module_id) const {
    const auto found = modules_by_id_.find(module_id);
    if (found == modules_by_id_.end()) {
        return nullptr;
    }
    return found->second;
}

std::shared_ptr<modules::IModule> ModuleRegistry::find_module_by_capability(
    const core::CapabilityId& capability_id) const {
    const auto found = capability_to_module_.find(capability_id);
    if (found == capability_to_module_.end()) {
        return nullptr;
    }
    return find_module_by_id(found->second);
}

std::vector<std::shared_ptr<modules::IModule>> ModuleRegistry::all_modules() const {
    std::vector<std::shared_ptr<modules::IModule>> result;
    result.reserve(modules_by_id_.size());
    for (const auto& [_, module] : modules_by_id_) {
        result.push_back(module);
    }
    return result;
}

bool ModuleRegistry::capability_exists(const core::CapabilityId& capability_id) const {
    return capability_to_module_.contains(capability_id);
}

}  // namespace life_orchestrator::control_plane
