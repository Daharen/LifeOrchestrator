cmake_minimum_required(VERSION 3.16)
project(LifeOrchestrator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

add_library(lifeorchestrator_core
    src/core/contracts.cpp
    src/core/memory.cpp
    src/core/memory_service.cpp
)
target_include_directories(lifeorchestrator_core PUBLIC src)

add_library(lifeorchestrator_integration
    src/integration/integration_configuration_repository.cpp
)
target_include_directories(lifeorchestrator_integration PUBLIC src)
target_link_libraries(lifeorchestrator_integration PUBLIC lifeorchestrator_core)

add_library(lifeorchestrator_control_plane
    src/control_plane/module_registry.cpp
    src/control_plane/event_logger.cpp
    src/control_plane/control_plane.cpp
    src/coordination/scheduling_coordination_stub_module.cpp
)
target_include_directories(lifeorchestrator_control_plane PUBLIC src)
target_link_libraries(lifeorchestrator_control_plane PUBLIC lifeorchestrator_core)

add_executable(control_plane_smoke src/control_plane/smoke_main.cpp)
target_link_libraries(control_plane_smoke PRIVATE lifeorchestrator_control_plane)

add_executable(lifeorchestrator_tests
    tests/test_main.cpp
)
target_link_libraries(lifeorchestrator_tests PRIVATE lifeorchestrator_control_plane lifeorchestrator_integration)

add_test(NAME lifeorchestrator_tests COMMAND lifeorchestrator_tests)
