#pragma once

#include "control_plane/control_plane.hpp"
#include "coordination/behavioral_triage_module.hpp"
#include "coordination/scheduling_coordination_module.hpp"
#include "core/contracts.hpp"
#include "core/memory.hpp"
#include "core/memory_service.hpp"
#include "integration/integration_configuration_repository.hpp"
#include "meta/procedural_auditor_module.hpp"

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace life_orchestrator::app {

enum class ApplicationRunMode {
    Status,
    ListModules,
    ScheduleHealthCheck,
    BehavioralHealthCheck,
    BehavioralListBacklog,
    BehavioralRecordState,
    BehavioralListInterventions,
    BehavioralReevaluateBacklog,
    BehavioralStatus,
    ProceduralHealthCheck,
    ProceduralListProposals,
    ProceduralUpsertActivity,
    ProceduralListActivities,
    ProceduralRunAudit,
    ProceduralListAuditRuns,
    BootstrapCheck
};

enum class ApplicationExitCode {
    Success = 0,
    BootstrapFailure = 1,
    CommandValidationFailure = 2,
    RuntimeOperationFailure = 3
};

struct ApplicationBootstrapConfig {
    std::string application_name;
    std::filesystem::path data_root_path;
    std::filesystem::path events_file_path;
    std::filesystem::path integration_config_root_path;
    std::filesystem::path memory_root_path;
    std::string default_timezone;
    ApplicationRunMode run_mode;
    bool allow_seed_data;
    bool log_startup_summary;
    core::StringMap command_parameters;
};

struct ApplicationBootstrapResult {
    bool ok;
    ApplicationExitCode exit_code;
    std::string message;
    ApplicationBootstrapConfig config;
};

struct ApplicationRuntime {
    ApplicationBootstrapConfig config;
    control_plane::EventLogger event_logger;
    core::FileMemoryStore memory_store;
    core::MemoryService memory_service;
    integration::IntegrationConfigurationRepository integration_repository;
    control_plane::ModuleRegistry module_registry;
    std::shared_ptr<coordination::SchedulingCoordinationModule> scheduling_module;
    std::shared_ptr<coordination::BehavioralTriageModule> behavioral_module;
    std::shared_ptr<meta::ProceduralAuditorModule> procedural_module;
    control_plane::ControlPlane control_plane;

    explicit ApplicationRuntime(const ApplicationBootstrapConfig& config);
};

ApplicationBootstrapResult resolve_bootstrap_config(const std::vector<std::string>& args,
                                                    const std::string& environment_data_root,
                                                    const std::filesystem::path& working_root);

ApplicationBootstrapResult initialize_runtime(ApplicationRuntime& runtime);
std::vector<std::shared_ptr<modules::IModule>> build_runtime_modules(ApplicationRuntime& runtime);
ApplicationExitCode execute_command(ApplicationRuntime& runtime,
                                    std::ostream& output,
                                    std::ostream& error);
int run_application(const std::vector<std::string>& args,
                    std::ostream& output,
                    std::ostream& error,
                    const std::string& environment_data_root,
                    const std::filesystem::path& working_root);

std::string to_string(ApplicationRunMode mode);
std::string to_string(ApplicationExitCode code);

}  // namespace life_orchestrator::app
