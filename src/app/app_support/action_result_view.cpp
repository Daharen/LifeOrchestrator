#include "app/app_support/action_result_view.hpp"

#include "app/app_support/artifact_refresh_registry.hpp"

#include <sstream>

namespace life_orchestrator::app {
namespace {

std::vector<ActionResultFieldRow> parse_output_rows(const std::string& text) {
    std::vector<ActionResultFieldRow> rows;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        rows.push_back({line.substr(0, pos), line.substr(pos + 1)});
    }
    return rows;
}

std::string join_refresh_labels(const std::vector<ArtifactRefreshTarget>& targets) {
    std::string joined;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (index != 0) joined += index + 1 == targets.size() ? " and " : ", ";
        joined += targets[index].display_label;
    }
    return joined;
}

std::vector<std::string> query_args_for_target(const std::string& artifact_type, const std::size_t refresh_limit) {
    return {"artifact.query", "--artifact-type", artifact_type, "--limit", std::to_string(refresh_limit)};
}

}  // namespace

ActionExecutionResultView build_action_execution_result_view(const ActionFormSpec& spec,
                                                             const ApplicationInvocationResult& invocation) {
    ActionExecutionResultView view;
    view.action_id = spec.action_id;
    view.action_label = spec.display_label;
    view.canonical_command_id = spec.canonical_command_target;
    view.exit_code = invocation.exit_code;
    view.succeeded = invocation.exit_code == 0;
    view.refresh_targets = list_artifact_refresh_targets(spec);
    view.raw_output = invocation.exit_code == 0 ? invocation.standard_output : invocation.standard_error;
    if (view.raw_output.empty()) view.raw_output = invocation.exit_code == 0 ? invocation.standard_error : invocation.standard_output;
    view.output_rows = parse_output_rows(view.raw_output);
    if (view.refresh_targets.empty()) {
        view.next_state_hint = view.succeeded ? "No registered artifact refresh targets." : "Action failed before any artifact refresh.";
    } else if (view.succeeded) {
        view.next_state_hint = "Refresh target" + std::string(view.refresh_targets.size() == 1 ? ": " : "s: ") + join_refresh_labels(view.refresh_targets) + ".";
    } else {
        view.next_state_hint = "If retried successfully, refresh target" + std::string(view.refresh_targets.size() == 1 ? " would be " : "s would be ") + join_refresh_labels(view.refresh_targets) + ".";
    }
    return view;
}

ActionExecutionFeedback execute_action_form_command(const ActionFormSpec& spec,
                                                    const std::vector<std::string>& submitted_args,
                                                    const std::string& environment_data_root,
                                                    const std::filesystem::path& working_root,
                                                    const std::size_t refresh_limit) {
    auto command_args = submitted_args;
    command_args.push_back("--quiet-startup");
    ActionExecutionFeedback feedback;
    const auto invocation = invoke_application_command(command_args, environment_data_root, working_root);
    feedback.result_view = build_action_execution_result_view(spec, invocation);
    if (!feedback.result_view.succeeded) return feedback;

    for (const auto& target : feedback.result_view.refresh_targets) {
        auto args = query_args_for_target(target.artifact_type, refresh_limit);
        feedback.refreshed_artifacts.push_back({target, args, invoke_application_command(args, environment_data_root, working_root)});
    }
    return feedback;
}

}  // namespace life_orchestrator::app
