#pragma once

#include "app/app_support/action_form_spec.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace life_orchestrator::app {

struct IntentCommandAlias {
    std::string alias;
    std::string command;
};

struct IntentCommandDescriptor {
    std::string command;
    std::vector<std::string> aliases;
    std::vector<std::string> required_flags;
    std::vector<std::string> optional_flags;
    std::string example;
    bool high_risk = false;
};

struct IntentCommandContext {
    std::vector<IntentCommandDescriptor> commands;
    std::vector<IntentCommandAlias> aliases;
    std::vector<ActionFormSpec> action_forms;
};

struct IntentRoutingResult {
    std::string mode;
    std::string matched_command;
    std::vector<std::string> args;
    double confidence = 0.0;
    std::string reasoning_summary;
    bool requires_confirmation = false;
    std::vector<std::string> closest_commands;
    std::string user_facing_message;
    std::string raw_model_output;
};

using IntentProvider = std::function<std::string(const std::string& prompt)>;

IntentCommandContext build_intent_command_context(const std::vector<std::string>& commands,
                                                  const std::vector<std::pair<std::string, std::string>>& aliases,
                                                  const std::vector<ActionFormSpec>& action_forms);

std::string render_intent_prompt(const std::string& input,
                                 const IntentCommandContext& context);

IntentRoutingResult route_with_provider(const std::string& input,
                                        const IntentCommandContext& context,
                                        const std::vector<std::string>& closest_commands,
                                        const IntentProvider& provider);

std::string serialize_intent_routing_result(const IntentRoutingResult& result);

}  // namespace life_orchestrator::app
