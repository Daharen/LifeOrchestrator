cmake_minimum_required(VERSION 3.16)
project(LifeOrchestrator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(lifeorchestrator_core
    src/core/contracts.cpp
    src/core/behavioral.cpp
    src/core/procedural.cpp
    src/core/memory.cpp
    src/core/memory_service.cpp
)
target_include_directories(lifeorchestrator_core PUBLIC src)

add_library(lifeorchestrator_integration
    src/integration/integration_configuration_repository.cpp
    src/integration/inference/inference_transport_contracts.cpp
    src/integration/inference/inference_transport_client.cpp
    src/integration/inference/provider_http_transport.cpp
    src/integration/inference/provider_transport_registry.cpp
)
target_include_directories(lifeorchestrator_integration PUBLIC src)
target_link_libraries(lifeorchestrator_integration PUBLIC lifeorchestrator_core)

add_library(lifeorchestrator_control_plane
    src/control_plane/module_registry.cpp
    src/control_plane/event_logger.cpp
    src/control_plane/control_plane.cpp
    src/coordination/scheduling_engine.cpp
    src/coordination/scheduling_coordination_module.cpp
    src/coordination/behavioral_triage_engine.cpp
    src/coordination/behavioral_triage_module.cpp
    src/meta/procedural_auditor_engine.cpp
    src/meta/procedural_auditor_module.cpp
)
target_include_directories(lifeorchestrator_control_plane PUBLIC src)
target_link_libraries(lifeorchestrator_control_plane PUBLIC lifeorchestrator_core)

add_library(lifeorchestrator_app_support
    src/app/application_bootstrap.cpp
    src/app/assistant_shell/assistant_shell_surface_contracts.cpp
    src/app/assistant_shell/assistant_shell_surface_service.cpp
    src/app/provider_setup/provider_setup_contracts.cpp
    src/app/provider_setup/provider_setup_service.cpp
    src/ui/provider_setup/provider_setup_controller.cpp
    src/intelligence/intent_router.cpp
    src/ui/assistant_shell/assistant_shell_composer_input.cpp
    src/app/app_support/action_form_registry.cpp
    src/app/app_support/action_result_view.cpp
    src/app/app_support/artifact_presentation_registry.cpp
    src/app/app_support/artifact_refresh_registry.cpp
    src/app/app_support/artifact_query_service.cpp
    src/app/app_support/artifact_query_module.cpp
    src/app/app_support/memory/activity_repository.cpp
    src/app/app_support/memory/behavior_repository.cpp
    src/app/app_support/memory/scheduling_repository.cpp
    src/app/app_support/memory/procedural_repository.cpp
    src/app/app_support/memory/integration_repository.cpp
    src/ui/artifact_renderer.cpp
    src/ui/artifact_panels/artifact_panel.cpp
    src/ui/artifact_panels/activity_inventory_panel.cpp
    src/ui/artifact_panels/procedural_proposals_panel.cpp
    src/ui/artifact_panels/behavioral_backlog_panel.cpp
    src/ui/artifact_panels/behavioral_interventions_panel.cpp
    src/ui/artifact_panels/scheduling_candidates_panel.cpp
    src/ui/artifact_panels/schedule_proposals_panel.cpp
    src/ui/artifact_panels/behavioral_reevaluations_panel.cpp
    src/ui/artifact_panels/provider_config_panel.cpp
    src/ui/artifact_panels/artifact_panels_registry.cpp
)
target_include_directories(lifeorchestrator_app_support PUBLIC src)
target_link_libraries(lifeorchestrator_app_support PUBLIC lifeorchestrator_control_plane lifeorchestrator_integration)

add_executable(life_orchestrator_app src/app/main.cpp)
set_target_properties(life_orchestrator_app PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
)
target_link_libraries(life_orchestrator_app PRIVATE lifeorchestrator_app_support)

if(WIN32)
    add_executable(life_orchestrator_admin_gui WIN32 src/ui/admin_gui_win32.cpp)
    set_target_properties(life_orchestrator_admin_gui PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    )
    target_link_libraries(life_orchestrator_admin_gui PRIVATE lifeorchestrator_app_support user32 gdi32 comctl32)
endif()

if(WIN32)
    add_executable(life_orchestrator_assistant_shell WIN32
        src/ui/assistant_shell/assistant_shell_main.cpp
        src/ui/assistant_shell/assistant_shell_window.cpp
        src/ui/assistant_shell/assistant_shell_controller.cpp
        src/ui/assistant_shell/assistant_shell_view_models.cpp
        src/ui/assistant_shell/assistant_shell_tool_panel.cpp
        src/ui/assistant_shell/assistant_shell_confirmation_surface.cpp
        src/ui/assistant_shell/assistant_shell_message_surface.cpp
        src/ui/assistant_shell/assistant_shell_status_bar.cpp
        src/ui/provider_setup/provider_setup_window.cpp
        src/ui/provider_setup/provider_setup_controller.cpp
        src/ui/provider_setup/provider_setup_view_models.cpp
    )
    set_target_properties(life_orchestrator_assistant_shell PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Debug"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Release"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/RelWithDebInfo"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/MinSizeRel"
    )
    target_link_libraries(life_orchestrator_assistant_shell PRIVATE lifeorchestrator_app_support user32 gdi32 comctl32)
endif()

add_executable(control_plane_smoke src/control_plane/smoke_main.cpp)
target_link_libraries(control_plane_smoke PRIVATE lifeorchestrator_control_plane)

add_executable(lifeorchestrator_tests
    tests/test_main.cpp
)
target_link_libraries(lifeorchestrator_tests PRIVATE lifeorchestrator_app_support)

add_test(NAME lifeorchestrator_tests COMMAND lifeorchestrator_tests)
