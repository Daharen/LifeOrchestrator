#include "ui/provider_setup/provider_setup_controller.h"

namespace life_orchestrator::ui::provider_setup {

ProviderSetupController::ProviderSetupController(std::shared_ptr<life_orchestrator::app::provider_setup::ProviderSetupService> service)
    : service_(std::move(service)) {}

std::vector<life_orchestrator::app::provider_setup::ProviderSetupProviderSummary> ProviderSetupController::ListProviders() const {
    return service_->ListProviders();
}

life_orchestrator::app::ApplicationInvocationResult ProviderSetupController::SaveProvider(
    const life_orchestrator::app::provider_setup::ProviderSetupUpsertRequest& request) const {
    return service_->SaveProvider(request);
}

life_orchestrator::app::provider_setup::ProviderSetupTestResult ProviderSetupController::TestProvider(const std::string& provider_name) const {
    return service_->TestProvider(provider_name);
}

}  // namespace life_orchestrator::ui::provider_setup
