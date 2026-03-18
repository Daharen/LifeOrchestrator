#pragma once

#include "control_plane/control_plane.hpp"
#include "core/memory_service.hpp"
#include "meta/procedural_auditor_engine.hpp"
#include "modules/imodule.hpp"

namespace life_orchestrator::meta {

class ProceduralAuditorModule final : public modules::IModule {
public:
    ProceduralAuditorModule(core::MemoryService* memory_service,
                            control_plane::ControlPlane* control_plane);

    const core::ModuleDescriptor& descriptor() const override;
    bool supports_capability(const core::CapabilityId& capability_id) const override;
    core::ActionResponse execute(const core::ActionRequest& request) override;

private:
    core::ActionResponse upsert_activity(const core::ActionRequest& request);
    core::ActionResponse audit_inventory(const core::ActionRequest& request);
    core::ActionResponse list_optimization_proposals(const core::ActionRequest& request);
    core::ActionResponse health_check(const core::ActionRequest& request);

    core::ModuleDescriptor descriptor_;
    core::MemoryService* memory_service_;
    control_plane::ControlPlane* control_plane_;
    ProceduralAuditorEngine engine_;
};

}  // namespace life_orchestrator::meta
