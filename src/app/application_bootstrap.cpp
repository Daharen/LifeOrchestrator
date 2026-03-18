#include "app/application_bootstrap.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace life_orchestrator::app {
namespace {

core::StructuredEvent make_event(core::EventCategory category,
                                 const std::string& message,
                                 const ApplicationBootstrapConfig& config,
                                 std::string capability_id = "application.bootstrap",
                                 core::StringMap fields = {}) {
    fields["application_name"] = config.application_name;
    fields["run_mode"] = to_string(config.run_mode);
    fields["data_root_path"] = config.data_root_path.string();
    return {category,
            core::current_timestamp_utc(),
            "app",
            "app.bootstrap",
            std::move(capability_id),
            std::move(message),
            std::move(fields)};
}

ApplicationRunMode parse_run_mode(const std::string& value) {
    if (value == "status") return ApplicationRunMode::Status;
    if (value == "list-modules") return ApplicationRunMode::ListModules;
    if (value == "schedule-health-check") return ApplicationRunMode::ScheduleHealthCheck;
    return ApplicationRunMode::BootstrapCheck;
}

std::string run_mode_name(ApplicationRunMode mode) {
    switch (mode) {
        case ApplicationRunMode::Status: return "status";
        case ApplicationRunMode::ListModules: return "list-modules";
        case ApplicationRunMode::ScheduleHealthCheck: return "schedule-health-check";
        case ApplicationRunMode::BootstrapCheck: return "bootstrap-check";
    }
    return "bootstrap-check";
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string value_after_equals(const std::string& arg) {
    const auto pos = arg.find('=');
    return pos == std::string::npos ? std::string{} : arg.substr(pos + 1);
}

}  // namespace

ApplicationRuntime::ApplicationRuntime(const ApplicationBootstrapConfig& bootstrap_config)
    : config(bootstrap_config),
      event_logger(bootstrap_config.events_file_path),
      memory_store(bootstrap_config.data_root_path, &event_logger),
      memory_service(memory_store),
      integration_repository(bootstrap_config.data_root_path),
      scheduling_module(std::make_shared<coordination::SchedulingCoordinationModule>(&memory_service)),
      control_plane(module_registry, event_logger) {}

ApplicationBootstrapResult resolve_bootstrap_config(const std::vector<std::string>& args,
                                                    const std::string& environment_data_root,
                                                    const std::filesystem::path& working_root) {
    // Precedence rule for runtime paths:
    // 1. explicit command line arguments,
    // 2. environment variable for the data root only,
    // 3. deterministic local default under the working root.
    std::filesystem::path data_root = working_root / "runtime";
    if (!environment_data_root.empty()) {
        data_root = environment_data_root;
    }

    std::string command = "status";
    bool command_set = false;
    bool log_startup_summary = true;
    bool allow_seed_data = true;
    std::string timezone = "UTC";

    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto& arg = args[i];
        if (arg == "status" || arg == "list-modules" || arg == "schedule-health-check" || arg == "bootstrap-check") {
            command = arg;
            command_set = true;
            continue;
        }
        if (starts_with(arg, "--data-root=")) {
            data_root = value_after_equals(arg);
            continue;
        }
        if (starts_with(arg, "--timezone=")) {
            timezone = value_after_equals(arg);
            continue;
        }
        if (arg == "--no-seed-data") {
            allow_seed_data = false;
            continue;
        }
        if (arg == "--quiet-startup") {
            log_startup_summary = false;
            continue;
        }
        if (starts_with(arg, "--command=")) {
            command = value_after_equals(arg);
            command_set = true;
            continue;
        }
        if (starts_with(arg, "--")) {
            return {false,
                    ApplicationExitCode::CommandValidationFailure,
                    "Unknown argument: " + arg,
                    {}};
        }
        if (!command_set) {
            command = arg;
            command_set = true;
            continue;
        }
        return {false,
                ApplicationExitCode::CommandValidationFailure,
                "Unexpected positional argument: " + arg,
                {}};
    }

    if (command != "status" && command != "list-modules" && command != "schedule-health-check" && command != "bootstrap-check") {
        return {false,
                ApplicationExitCode::CommandValidationFailure,
                "Unknown command: " + command,
                {}};
    }

    ApplicationBootstrapConfig config{.application_name = "life_orchestrator_app",
                                      .data_root_path = data_root,
                                      .events_file_path = data_root / "events" / "application.ndjson",
                                      .integration_config_root_path = data_root / "memory" / "integration_configuration",
                                      .memory_root_path = data_root / "memory",
                                      .default_timezone = timezone,
                                      .run_mode = parse_run_mode(command),
                                      .allow_seed_data = allow_seed_data,
                                      .log_startup_summary = log_startup_summary};
    return {true, ApplicationExitCode::Success, "ok", config};
}

std::vector<std::shared_ptr<modules::IModule>> build_runtime_modules(ApplicationRuntime& runtime) {
    return {runtime.scheduling_module};
}

ApplicationBootstrapResult initialize_runtime(ApplicationRuntime& runtime) {
    runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapStarted,
                                           "Application bootstrap started.",
                                           runtime.config));
    try {
        std::filesystem::create_directories(runtime.config.data_root_path);
        std::filesystem::create_directories(runtime.config.memory_root_path);
        std::filesystem::create_directories(runtime.config.integration_config_root_path);
        std::filesystem::create_directories(runtime.config.events_file_path.parent_path());

        const auto memory_result = runtime.memory_store.load_from_disk();
        if (!memory_result.ok) {
            runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapFailed,
                                                   memory_result.message,
                                                   runtime.config));
            return {false, ApplicationExitCode::BootstrapFailure, memory_result.message, runtime.config};
        }

        const auto integration_load = runtime.integration_repository.load();
        if (!integration_load.ok) {
            runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapFailed,
                                                   integration_load.message,
                                                   runtime.config));
            return {false, ApplicationExitCode::BootstrapFailure, integration_load.message, runtime.config};
        }

        if (runtime.config.allow_seed_data) {
            const auto manifest_result = runtime.integration_repository.persist_manifest();
            if (!manifest_result.ok) {
                runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapFailed,
                                                       manifest_result.message,
                                                       runtime.config));
                return {false, ApplicationExitCode::BootstrapFailure, manifest_result.message, runtime.config};
            }
        }

        for (const auto& module : build_runtime_modules(runtime)) {
            const auto result = runtime.module_registry.register_module(module);
            if (!result.ok) {
                runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapFailed,
                                                       result.message,
                                                       runtime.config));
                return {false, ApplicationExitCode::BootstrapFailure, result.message, runtime.config};
            }
        }

        runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapCompleted,
                                               "Application bootstrap completed.",
                                               runtime.config,
                                               "application.bootstrap",
                                               {{"registered_module_count",
                                                 std::to_string(runtime.module_registry.all_modules().size())}}));
        return {true, ApplicationExitCode::Success, "ok", runtime.config};
    } catch (const std::exception& e) {
        runtime.event_logger.append(make_event(core::EventCategory::ApplicationBootstrapFailed,
                                               e.what(),
                                               runtime.config));
        return {false, ApplicationExitCode::BootstrapFailure, e.what(), runtime.config};
    }
}

ApplicationExitCode execute_command(ApplicationRuntime& runtime,
                                    std::ostream& output,
                                    std::ostream& error) {
    runtime.event_logger.append(make_event(core::EventCategory::ApplicationCommandStarted,
                                           "Application command started.",
                                           runtime.config,
                                           "application.command"));

    auto complete = [&](ApplicationExitCode code, const std::string& message) {
        runtime.event_logger.append(make_event(code == ApplicationExitCode::Success
                                                   ? core::EventCategory::ApplicationCommandCompleted
                                                   : core::EventCategory::ApplicationCommandFailed,
                                               message,
                                               runtime.config,
                                               "application.command",
                                               {{"exit_code", to_string(code)}}));
        return code;
    };

    switch (runtime.config.run_mode) {
        case ApplicationRunMode::Status: {
            const auto summary = runtime.memory_store.get_memory_summary();
            if (!summary.ok) {
                error << "error=" << summary.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, summary.message);
            }
            output << "application_name=" << runtime.config.application_name << '\n'
                   << "run_mode=" << to_string(runtime.config.run_mode) << '\n'
                   << "data_root_path=" << runtime.config.data_root_path.string() << '\n'
                   << "events_file_path=" << runtime.config.events_file_path.string() << '\n'
                   << "memory_root_path=" << runtime.config.memory_root_path.string() << '\n'
                   << "integration_config_root_path=" << runtime.config.integration_config_root_path.string() << '\n'
                   << "default_timezone=" << runtime.config.default_timezone << '\n'
                   << "registered_module_count=" << runtime.module_registry.all_modules().size() << '\n'
                   << "memory_available=1" << '\n'
                   << "integration_record_count=" << runtime.integration_repository.list_all().size() << '\n';
            return complete(ApplicationExitCode::Success, "Status command completed.");
        }
        case ApplicationRunMode::ListModules: {
            auto modules = runtime.module_registry.all_modules();
            std::sort(modules.begin(), modules.end(), [](const auto& a, const auto& b) {
                return a->descriptor().module_id < b->descriptor().module_id;
            });
            for (const auto& module : modules) {
                output << "module_id=" << module->descriptor().module_id << '\n';
                auto capabilities = module->descriptor().capabilities;
                std::sort(capabilities.begin(), capabilities.end());
                for (const auto& capability : capabilities) {
                    output << "capability=" << capability << '\n';
                }
            }
            return complete(ApplicationExitCode::Success, "List modules command completed.");
        }
        case ApplicationRunMode::BootstrapCheck: {
            const bool event_log_exists = std::filesystem::exists(runtime.config.events_file_path);
            output << "bootstrap_check=ok\n"
                   << "data_root_exists=" << std::filesystem::exists(runtime.config.data_root_path) << '\n'
                   << "memory_root_exists=" << std::filesystem::exists(runtime.config.memory_root_path) << '\n'
                   << "integration_root_exists=" << std::filesystem::exists(runtime.config.integration_config_root_path) << '\n'
                   << "event_log_exists=" << event_log_exists << '\n';
            return complete(ApplicationExitCode::Success, "Bootstrap check completed.");
        }
        case ApplicationRunMode::ScheduleHealthCheck: {
            core::AvailabilityWindow window{"app.health.window",
                                            "Health Window",
                                            "2026-03-18T09:00:00.000Z",
                                            "2026-03-18T17:00:00.000Z",
                                            runtime.config.default_timezone,
                                            "focus",
                                            "none",
                                            runtime.scheduling_module->descriptor().module_id,
                                            core::current_timestamp_utc(),
                                            core::current_timestamp_utc(),
                                            1};
            runtime.memory_service.upsert_availability_window(window);
            const auto response = runtime.control_plane.dispatch({"app-health-check",
                                                                  "scheduling.propose_time_blocks",
                                                                  "life_orchestrator_app",
                                                                  core::RiskTier::Suggestive,
                                                                  {{"schedule_item_id", "task.app.health"},
                                                                   {"title", "Health Check Task"},
                                                                   {"related_entity_id", "entity.app"},
                                                                   {"estimated_duration_minutes", "30"},
                                                                   {"earliest_start", "2026-03-18T09:00:00.000Z"},
                                                                   {"latest_end", "2026-03-18T17:00:00.000Z"}},
                                                                  core::current_timestamp_utc()});
            if (response.status != core::ExecutionStatus::Succeeded) {
                error << "schedule_health_check=failed\nmessage=" << response.message << '\n';
                return complete(ApplicationExitCode::RuntimeOperationFailure, response.message);
            }
            runtime.memory_store.persist_to_disk();
            output << "schedule_health_check=ok\n"
                   << "responding_module_id=" << response.responding_module_id << '\n'
                   << "proposal_count=" << response.output_data.at("proposal_count") << '\n'
                   << "first_proposal_id=" << response.output_data.at("first_proposal_id") << '\n';
            return complete(ApplicationExitCode::Success, "Schedule health check completed.");
        }
    }

    error << "error=unsupported_command\n";
    return complete(ApplicationExitCode::RuntimeOperationFailure, "Unsupported command.");
}

int run_application(const std::vector<std::string>& args,
                    std::ostream& output,
                    std::ostream& error,
                    const std::string& environment_data_root,
                    const std::filesystem::path& working_root) {
    const auto resolved = resolve_bootstrap_config(args, environment_data_root, working_root);
    if (!resolved.ok) {
        error << "error=" << resolved.message << '\n';
        return static_cast<int>(resolved.exit_code);
    }

    ApplicationRuntime runtime(resolved.config);
    const auto init = initialize_runtime(runtime);
    if (!init.ok) {
        error << "error=" << init.message << '\n';
        return static_cast<int>(init.exit_code);
    }

    if (runtime.config.log_startup_summary) {
        output << "bootstrap=ok\n";
    }

    return static_cast<int>(execute_command(runtime, output, error));
}

std::string to_string(ApplicationRunMode mode) { return run_mode_name(mode); }
std::string to_string(ApplicationExitCode code) {
    switch (code) {
        case ApplicationExitCode::Success: return "0";
        case ApplicationExitCode::BootstrapFailure: return "1";
        case ApplicationExitCode::CommandValidationFailure: return "2";
        case ApplicationExitCode::RuntimeOperationFailure: return "3";
    }
    return "3";
}

}  // namespace life_orchestrator::app
