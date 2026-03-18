#include "app/application_bootstrap.hpp"
#include "core/contracts.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

void assert_true(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void test_config_resolution_precedence() {
    namespace app = life_orchestrator::app;
    const auto explicit_config = app::resolve_bootstrap_config({"status", "--data-root=/tmp/explicit-root"},
                                                               "/tmp/env-root",
                                                               "/tmp/repo-root");
    assert_true(explicit_config.ok, "explicit config resolution should succeed");
    assert_true(explicit_config.config.data_root_path == std::filesystem::path("/tmp/explicit-root"),
                "explicit argument must win over environment");

    const auto env_config = app::resolve_bootstrap_config({"status"}, "/tmp/env-root", "/tmp/repo-root");
    assert_true(env_config.ok, "env config resolution should succeed");
    assert_true(env_config.config.data_root_path == std::filesystem::path("/tmp/env-root"),
                "environment root should win over default");

    const auto default_config = app::resolve_bootstrap_config({"status"}, "", "/tmp/repo-root");
    assert_true(default_config.ok, "default config resolution should succeed");
    assert_true(default_config.config.data_root_path == std::filesystem::path("/tmp/repo-root/runtime"),
                "default root should be deterministic under working root");
}

void test_bootstrap_initializes_directories_and_registry() {
    namespace app = life_orchestrator::app;
    const auto root = std::filesystem::path("artifacts/app/bootstrap_dirs");
    std::filesystem::remove_all(root);

    auto resolved = app::resolve_bootstrap_config({"status", "--data-root=" + root.string()}, "", std::filesystem::current_path());
    assert_true(resolved.ok, "bootstrap config should resolve");
    app::ApplicationRuntime runtime(resolved.config);
    const auto init = app::initialize_runtime(runtime);
    assert_true(init.ok, "bootstrap initialization should succeed");
    assert_true(std::filesystem::exists(resolved.config.data_root_path), "data root should be created");
    assert_true(std::filesystem::exists(resolved.config.memory_root_path), "memory root should be created");
    assert_true(std::filesystem::exists(resolved.config.integration_config_root_path), "integration root should be created");
    assert_true(runtime.module_registry.find_module_by_id("coordination.scheduling") != nullptr,
                "real scheduling module should be registered");
}

void test_status_command() {
    namespace app = life_orchestrator::app;
    const auto root = std::filesystem::path("artifacts/app/status");
    std::filesystem::remove_all(root);
    std::ostringstream out;
    std::ostringstream err;
    const int code = app::run_application({"status", "--data-root=" + root.string()}, out, err, "", std::filesystem::current_path());
    assert_true(code == 0, "status command should succeed");
    const auto text = out.str();
    assert_true(text.find("application_name=life_orchestrator_app") != std::string::npos, "status should print application name");
    assert_true(text.find("registered_module_count=1") != std::string::npos, "status should report module count");
}

void test_list_modules_command() {
    namespace app = life_orchestrator::app;
    std::ostringstream out;
    std::ostringstream err;
    const int code = app::run_application({"list-modules", "--data-root=artifacts/app/list_modules"}, out, err, "", std::filesystem::current_path());
    assert_true(code == 0, "list-modules should succeed");
    const auto text = out.str();
    assert_true(text.find("module_id=coordination.scheduling") != std::string::npos, "list-modules should include scheduling module");
    assert_true(text.find("capability=scheduling.add_commitment") != std::string::npos, "list-modules should include scheduling capabilities");
}

void test_bootstrap_check_command() {
    namespace app = life_orchestrator::app;
    const auto root = std::filesystem::path("artifacts/app/bootstrap_check");
    std::filesystem::remove_all(root);
    std::ostringstream out;
    std::ostringstream err;
    const int code = app::run_application({"bootstrap-check", "--data-root=" + root.string()}, out, err, "", std::filesystem::current_path());
    assert_true(code == 0, "bootstrap-check should succeed on clean root");
    assert_true(out.str().find("bootstrap_check=ok") != std::string::npos, "bootstrap-check output should be deterministic");
}

void test_schedule_health_check_command() {
    namespace app = life_orchestrator::app;
    const auto root = std::filesystem::path("artifacts/app/schedule_health");
    std::filesystem::remove_all(root);
    std::ostringstream out;
    std::ostringstream err;
    const int code = app::run_application({"schedule-health-check", "--data-root=" + root.string()}, out, err, "", std::filesystem::current_path());
    assert_true(code == 0, "schedule-health-check should succeed");
    const auto text = out.str();
    assert_true(text.find("schedule_health_check=ok") != std::string::npos, "schedule-health-check should report success");
    assert_true(text.find("proposal_count=") != std::string::npos, "schedule-health-check should prove scheduling connectivity");
}

void test_startup_and_command_events_are_appended() {
    namespace app = life_orchestrator::app;
    const auto root = std::filesystem::path("artifacts/app/events");
    std::filesystem::remove_all(root);
    std::ostringstream out;
    std::ostringstream err;
    const int code = app::run_application({"status", "--data-root=" + root.string()}, out, err, "", std::filesystem::current_path());
    assert_true(code == 0, "status should succeed for event test");

    const auto log_path = root / "events" / "application.ndjson";
    assert_true(std::filesystem::exists(log_path), "application log should be created");
    std::ifstream in(log_path);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    assert_true(contents.find("ApplicationBootstrapStarted") != std::string::npos, "startup event should be appended");
    assert_true(contents.find("ApplicationCommandCompleted") != std::string::npos, "command completion event should be appended");
}

void test_invalid_command_returns_exit_code_two() {
    namespace app = life_orchestrator::app;
    std::ostringstream out;
    std::ostringstream err;
    const int code = app::run_application({"unknown-command"}, out, err, "", std::filesystem::current_path());
    assert_true(code == 2, "invalid command should return deterministic exit code 2");
}

void test_launcher_convention_files_exist() {
    auto root = std::filesystem::current_path();
    if (root.filename() == "build") {
        root = root.parent_path();
    }
    assert_true(std::filesystem::exists(root / "run.bat"), "run.bat should exist at repo root");
    assert_true(std::filesystem::exists(root / "run.ps1"), "run.ps1 should exist at repo root");
}

}  // namespace

int main() {
    try {
        test_config_resolution_precedence();
        test_bootstrap_initializes_directories_and_registry();
        test_status_command();
        test_list_modules_command();
        test_bootstrap_check_command();
        test_schedule_health_check_command();
        test_startup_and_command_events_are_appended();
        test_invalid_command_returns_exit_code_two();
        test_launcher_convention_files_exist();
    } catch (const std::exception& e) {
        std::cerr << "Test failure: " << e.what() << '\n';
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
