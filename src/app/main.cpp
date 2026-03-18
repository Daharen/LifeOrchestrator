#include "app/application_bootstrap.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }

    const char* env = std::getenv("LIFE_ORCHESTRATOR_DATA_ROOT");
    const std::string environment_data_root = env == nullptr ? std::string{} : std::string{env};
    return life_orchestrator::app::run_application(args, std::cout, std::cerr, environment_data_root, std::filesystem::current_path());
}
