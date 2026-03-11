#include "control_plane/control_plane.hpp"
#include "coordination/scheduling_coordination_stub_module.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void assert_true(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class AlternateCapabilityModule final : public life_orchestrator::modules::IModule {
public:
    AlternateCapabilityModule(std::string module_id, std::string capability)
        : descriptor_{.module_id = std::move(module_id),
                      .module_name = "Alt",
                      .module_class = life_orchestrator::core::ModuleClass::Coordination,
                      .capability_description = "alt",
                      .capabilities = {std::move(capability)},
                      .input_schema_description = "",
                      .output_schema_description = "",
                      .state_representation_description = "",
                      .dependencies = {},
                      .risk_tier = life_orchestrator::core::RiskTier::Suggestive} {}

    const life_orchestrator::core::ModuleDescriptor& descriptor() const override { return descriptor_; }

    bool supports_capability(const life_orchestrator::core::CapabilityId& capability_id) const override {
        return descriptor_.capabilities[0] == capability_id;
    }

    life_orchestrator::core::ActionResponse execute(const life_orchestrator::core::ActionRequest& request) override {
        return {request.request_id,
                life_orchestrator::core::ExecutionStatus::Succeeded,
                descriptor_.module_id,
                "ok",
                {},
                life_orchestrator::core::current_timestamp_utc()};
    }

private:
    life_orchestrator::core::ModuleDescriptor descriptor_;
};

void test_registry_accepts_first_registration() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    auto module = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    auto result = registry.register_module(module);
    assert_true(result.ok, "expected first registration to succeed");
}

void test_registry_rejects_duplicate_module_id() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    auto first = std::make_shared<AlternateCapabilityModule>("dup.module", "capability.a");
    auto second = std::make_shared<AlternateCapabilityModule>("dup.module", "capability.b");

    assert_true(registry.register_module(first).ok, "first module registration must succeed");
    auto duplicate_result = registry.register_module(second);
    assert_true(!duplicate_result.ok, "duplicate module id must be rejected");
}

void test_registry_rejects_duplicate_capability() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    auto first = std::make_shared<AlternateCapabilityModule>("module.a", "capability.shared");
    auto second = std::make_shared<AlternateCapabilityModule>("module.b", "capability.shared");

    assert_true(registry.register_module(first).ok, "first module registration must succeed");
    auto duplicate_result = registry.register_module(second);
    assert_true(!duplicate_result.ok, "duplicate capability ownership must be rejected");
}

void test_control_plane_dispatch_success_and_events() {
    const std::filesystem::path log_path = "artifacts/events/tests_success.ndjson";
    std::filesystem::remove(log_path);

    life_orchestrator::control_plane::ModuleRegistry registry;
    life_orchestrator::control_plane::EventLogger logger{log_path};
    auto module = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    assert_true(registry.register_module(module).ok, "module registration should succeed");

    life_orchestrator::control_plane::ControlPlane control_plane{registry, logger};
    life_orchestrator::core::ActionRequest request{.request_id = "req-1",
                                                   .capability_id = "scheduling.health_check",
                                                   .origin = "tests",
                                                   .requested_risk_tier =
                                                       life_orchestrator::core::RiskTier::Suggestive,
                                                   .parameters = {},
                                                   .created_at =
                                                       life_orchestrator::core::current_timestamp_utc()};
    auto response = control_plane.dispatch(request);

    assert_true(response.status == life_orchestrator::core::ExecutionStatus::Succeeded,
                "dispatch should succeed");
    assert_true(!logger.in_memory_events().empty(), "events should be recorded");

    std::ifstream log_file(log_path);
    std::string line;
    std::size_t lines = 0;
    while (std::getline(log_file, line)) {
        ++lines;
    }
    assert_true(lines >= 5, "successful flow should append multiple events");
}

void test_control_plane_not_found_for_missing_capability() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/tests_not_found.ndjson"};
    life_orchestrator::control_plane::ControlPlane control_plane{registry, logger};

    life_orchestrator::core::ActionRequest request{.request_id = "req-2",
                                                   .capability_id = "missing.capability",
                                                   .origin = "tests",
                                                   .requested_risk_tier =
                                                       life_orchestrator::core::RiskTier::Informational,
                                                   .parameters = {},
                                                   .created_at =
                                                       life_orchestrator::core::current_timestamp_utc()};

    auto response = control_plane.dispatch(request);
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::NotFound,
                "missing capability should return NotFound");
}

void test_risk_rule_rejects_lower_requested_tier() {
    life_orchestrator::control_plane::ModuleRegistry registry;
    life_orchestrator::control_plane::EventLogger logger{"artifacts/events/tests_risk.ndjson"};
    auto module = std::make_shared<life_orchestrator::coordination::SchedulingCoordinationStubModule>();
    assert_true(registry.register_module(module).ok, "module registration should succeed");

    life_orchestrator::control_plane::ControlPlane control_plane{registry, logger};
    life_orchestrator::core::ActionRequest request{.request_id = "req-3",
                                                   .capability_id = "scheduling.health_check",
                                                   .origin = "tests",
                                                   .requested_risk_tier =
                                                       life_orchestrator::core::RiskTier::Informational,
                                                   .parameters = {},
                                                   .created_at =
                                                       life_orchestrator::core::current_timestamp_utc()};

    auto response = control_plane.dispatch(request);
    assert_true(response.status == life_orchestrator::core::ExecutionStatus::InvalidRequest,
                "lower requested risk should be rejected");
}

}  // namespace

int main() {
    try {
        test_registry_accepts_first_registration();
        test_registry_rejects_duplicate_module_id();
        test_registry_rejects_duplicate_capability();
        test_control_plane_dispatch_success_and_events();
        test_control_plane_not_found_for_missing_capability();
        test_risk_rule_rejects_lower_requested_tier();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }

    std::cout << "All tests passed\n";
    return 0;
}
