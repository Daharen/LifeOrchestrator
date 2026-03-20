#pragma once

#include "app/app_support/action_form_registry.hpp"
#include "app/app_support/artifact_refresh_registry.hpp"
#include "app/application_bootstrap.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace life_orchestrator::app {

struct ActionResultFieldRow {
    std::string key;
    std::string value;
};


struct ArtifactRefreshResult {
    ArtifactRefreshTarget target;
    std::vector<std::string> query_args;
    ApplicationInvocationResult query_result;
};

struct ActionExecutionResultView {
    std::string action_id;
    std::string action_label;
    std::string canonical_command_id;
    int exit_code = 0;
    bool succeeded = false;
    std::vector<ActionResultFieldRow> output_rows;
    std::string raw_output;
    std::vector<ArtifactRefreshTarget> refresh_targets;
    std::string next_state_hint;
};

struct ActionExecutionFeedback {
    ActionExecutionResultView result_view;
    std::vector<ArtifactRefreshResult> refreshed_artifacts;
};

ActionExecutionResultView build_action_execution_result_view(const ActionFormSpec& spec,
                                                             const ApplicationInvocationResult& invocation);

ActionExecutionFeedback execute_action_form_command(const ActionFormSpec& spec,
                                                    const std::vector<std::string>& submitted_args,
                                                    const std::string& environment_data_root,
                                                    const std::filesystem::path& working_root,
                                                    std::size_t refresh_limit = 5);

}  // namespace life_orchestrator::app
