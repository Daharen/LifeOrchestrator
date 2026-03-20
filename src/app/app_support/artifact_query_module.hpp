#pragma once

#include "app/app_support/artifact_query_service.hpp"
#include "modules/imodule.hpp"

namespace life_orchestrator::app {

class ArtifactQueryModule : public modules::IModule {
public:
    explicit ArtifactQueryModule(const ArtifactQueryService* artifact_query_service);

    const core::ModuleDescriptor& descriptor() const override;
    bool supports_capability(const core::CapabilityId& capability_id) const override;
    core::ActionResponse execute(const core::ActionRequest& request) override;

private:
    const ArtifactQueryService* artifact_query_service_;
    core::ModuleDescriptor descriptor_;
};

}  // namespace life_orchestrator::app
