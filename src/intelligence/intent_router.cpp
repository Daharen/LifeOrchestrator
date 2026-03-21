#include "intelligence/intent_router.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace life_orchestrator::app {
namespace {

std::string trim_copy(const std::string& value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
    return value.substr(start, end - start);
}

std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool try_parse_double(const std::string& value, double& parsed) {
    try {
        std::size_t consumed = 0;
        parsed = std::stod(value, &consumed);
        if (consumed != value.size() || !std::isfinite(parsed)) return false;
        if (parsed < 0.0) parsed = 0.0;
        if (parsed > 1.0) parsed = 1.0;
        return true;
    } catch (...) {
        parsed = 0.0;
        return false;
    }
}

std::string join_strings(const std::vector<std::string>& values, const std::string& delimiter) {
    std::ostringstream out;
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) out << delimiter;
        out << values[i];
    }
    return out.str();
}

bool parse_bool(const std::string& value) {
    return value == "true" || value == "1" || value == "yes";
}

std::vector<std::string> split_csv(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream input(value);
    std::string token;
    while (std::getline(input, token, ',')) {
        token = trim_copy(token);
        if (!token.empty()) result.push_back(token);
    }
    return result;
}

std::vector<std::string> split_args(const std::string& value) {
    std::istringstream input(value);
    std::vector<std::string> result;
    std::string token;
    while (input >> token) result.push_back(token);
    return result;
}

std::unordered_map<std::string, std::string> parse_key_values(const std::string& raw) {
    std::unordered_map<std::string, std::string> parsed;
    std::stringstream input(raw);
    std::string line;
    while (std::getline(input, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        parsed[trim_copy(line.substr(0, pos))] = trim_copy(line.substr(pos + 1));
    }
    return parsed;
}

bool is_high_risk_command(const IntentCommandContext& context, const std::string& command) {
    const auto it = std::find_if(context.commands.begin(), context.commands.end(), [&](const auto& item) { return item.command == command; });
    return it != context.commands.end() && it->high_risk;
}

bool is_known_command(const IntentCommandContext& context, const std::string& command) {
    return std::any_of(context.commands.begin(), context.commands.end(), [&](const auto& item) { return item.command == command; });
}

std::string normalize_mode_value(const std::string& value) {
    const auto lowered = lower_copy(trim_copy(value));
    if (lowered.empty()) return "failure";
    if (lowered == "proposed" || lowered == "command" || lowered == "proposal" || lowered == "success") return "proposed";
    if (lowered == "failure" || lowered == "no_match" || lowered == "error") return "failure";
    return {};
}

bool try_parse_bool_like(const std::string& value, bool& parsed) {
    const auto lowered = lower_copy(trim_copy(value));
    if (lowered == "true" || lowered == "1" || lowered == "yes") {
        parsed = true;
        return true;
    }
    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered.empty() || lowered == "null") {
        parsed = false;
        return true;
    }
    return false;
}

}  // namespace

IntentCommandContext build_intent_command_context(const std::vector<std::string>& commands,
                                                  const std::vector<std::pair<std::string, std::string>>& aliases,
                                                  const std::vector<ActionFormSpec>& action_forms) {
    IntentCommandContext context;
    context.action_forms = action_forms;
    for (const auto& [alias, command] : aliases) context.aliases.push_back({alias, command});
    for (const auto& command : commands) {
        IntentCommandDescriptor descriptor;
        descriptor.command = command;
        descriptor.high_risk = command == "integration-set-provider" || command == "scheduling-generate-proposals";
        for (const auto& [alias, target] : aliases) if (target == command) descriptor.aliases.push_back(alias);
        if (const auto form = std::find_if(action_forms.begin(), action_forms.end(), [&](const auto& spec) { return spec.canonical_command_target == command; }); form != action_forms.end()) {
            descriptor.example = form->example_payload;
            for (const auto& field : form->input_fields) {
                if (field.accepted_flags.empty()) continue;
                if (field.required) descriptor.required_flags.push_back(field.accepted_flags.front());
                else descriptor.optional_flags.push_back(field.accepted_flags.front());
            }
        }
        context.commands.push_back(descriptor);
    }
    return context;
}

std::string render_intent_prompt(const std::string& input,
                                 const IntentCommandContext& context) {
    std::ostringstream prompt;
    prompt << "Return structured output only as newline-delimited key=value pairs.\n";
    prompt << "Allowed keys: mode,matched_command,args,confidence,reasoning_summary,requires_confirmation,closest_commands,user_facing_message.\n";
    prompt << "Input=" << input << "\n";
    for (const auto& command : context.commands) {
        prompt << "command=" << command.command;
        if (!command.aliases.empty()) prompt << ";aliases=" << join_strings(command.aliases, ",");
        if (!command.required_flags.empty()) prompt << ";required=" << join_strings(command.required_flags, ",");
        if (!command.optional_flags.empty()) prompt << ";optional=" << join_strings(command.optional_flags, ",");
        if (!command.example.empty()) prompt << ";example=" << command.example;
        prompt << ";high_risk=" << (command.high_risk ? "true" : "false") << "\n";
    }
    return prompt.str();
}

IntentRoutingResult route_with_provider(const std::string& input,
                                        const IntentCommandContext& context,
                                        const std::vector<std::string>& closest_commands,
                                        const IntentProvider& provider) {
    IntentRoutingResult result;
    result.mode = "failure";
    result.closest_commands = closest_commands;
    result.user_facing_message = "I couldn't map that request to a valid command.";

    result.raw_model_output = provider(render_intent_prompt(input, context));
    const auto parsed = parse_key_values(result.raw_model_output);
    if (parsed.empty()) {
        result.reasoning_summary = "Provider returned unstructured output.";
        return result;
    }

    if (const auto it = parsed.find("mode"); it != parsed.end()) result.mode = it->second;
    if (const auto it = parsed.find("matched_command"); it != parsed.end()) result.matched_command = it->second;
    if (const auto it = parsed.find("args"); it != parsed.end()) result.args = split_args(it->second);
    if (const auto it = parsed.find("confidence"); it != parsed.end() && !try_parse_double(it->second, result.confidence)) {
        result.reasoning_summary = "Provider returned malformed confidence.";
        result.user_facing_message = "The provider returned invalid confidence metadata.";
        result.matched_command.clear();
        return result;
    }
    if (const auto it = parsed.find("reasoning_summary"); it != parsed.end()) result.reasoning_summary = it->second;
    if (const auto it = parsed.find("requires_confirmation"); it != parsed.end()) {
        if (!try_parse_bool_like(it->second, result.requires_confirmation)) {
            result.reasoning_summary = "Provider returned malformed confirmation metadata.";
            result.user_facing_message = "The provider returned invalid confirmation metadata.";
            result.matched_command.clear();
            return result;
        }
    }
    if (const auto it = parsed.find("closest_commands"); it != parsed.end()) result.closest_commands = split_csv(it->second);
    if (const auto it = parsed.find("user_facing_message"); it != parsed.end()) result.user_facing_message = it->second;

    if (result.requires_confirmation || is_high_risk_command(context, result.matched_command)) result.requires_confirmation = true;
    return result;
}

IntentRouteNormalizationOutcome normalize_intent_routing_result(IntentRoutingResult result,
                                                                const IntentCommandContext& context,
                                                                const std::vector<std::string>& fallback_closest_commands) {
    IntentRouteNormalizationOutcome outcome;
    outcome.route = std::move(result);
    auto& route = outcome.route;

    route.mode = trim_copy(route.mode);
    route.matched_command = trim_copy(route.matched_command);
    route.reasoning_summary = trim_copy(route.reasoning_summary);
    route.user_facing_message = trim_copy(route.user_facing_message);
    route.closest_commands.erase(std::remove_if(route.closest_commands.begin(), route.closest_commands.end(), [](const std::string& value) { return trim_copy(value).empty(); }), route.closest_commands.end());
    if (route.closest_commands.empty()) route.closest_commands = fallback_closest_commands;
    if (!std::isfinite(route.confidence)) route.confidence = 0.0;
    if (route.confidence < 0.0) route.confidence = 0.0;
    if (route.confidence > 1.0) route.confidence = 1.0;

    const auto canonical_mode = normalize_mode_value(route.mode);
    if (!canonical_mode.empty()) route.mode = canonical_mode;
    else {
        route.mode = "failure";
        outcome.failure_class = "provider_output_unrecognized";
        outcome.acceptance_result = "rejected_unrecognized_mode";
    }

    if (!outcome.failure_class.empty()) {
        if (route.reasoning_summary.empty()) route.reasoning_summary = "Provider returned an unrecognized routing mode.";
        if (route.user_facing_message.empty()) route.user_facing_message = "I couldn't safely interpret that provider result. Try Help or an exact command.";
        route.matched_command.clear();
        route.args.clear();
    } else if (route.mode == "proposed") {
        if (route.matched_command.empty()) {
            route.mode = "failure";
            outcome.failure_class = "provider_output_incomplete";
            outcome.acceptance_result = "rejected_missing_command";
        } else if (!is_known_command(context, route.matched_command)) {
            route.mode = "failure";
            outcome.failure_class = "provider_output_ungrounded_command";
            outcome.acceptance_result = "rejected_ungrounded_command";
        } else {
            if (route.args.empty()) route.args.push_back(route.matched_command);
            if (route.args.front() != route.matched_command) route.args.insert(route.args.begin(), route.matched_command);
            if (is_high_risk_command(context, route.matched_command)) route.requires_confirmation = true;
            outcome.acceptance_result = "accepted_proposed";
        }
    } else {
        route.matched_command.clear();
        route.args.clear();
        outcome.failure_class = "provider_output_invalid";
        outcome.acceptance_result = "accepted_failure";
    }

    if (route.reasoning_summary.empty()) {
        route.reasoning_summary = route.mode == "failure" ? "Provider returned a recognized failure route." : "Provider route normalized successfully.";
    }
    if (route.user_facing_message.empty()) {
        route.user_facing_message = route.mode == "failure"
                                        ? "I couldn't map that request to a supported command. Try Help or an exact command."
                                        : "Mapped request safely.";
    }
    if (outcome.failure_class.empty() && route.mode == "failure") outcome.failure_class = "provider_output_invalid";
    if (outcome.acceptance_result.empty()) outcome.acceptance_result = route.mode == "proposed" ? "accepted_proposed" : "accepted_failure";
    return outcome;
}

std::string serialize_intent_routing_result(const IntentRoutingResult& result) {
    std::ostringstream output;
    output << "mode=" << result.mode << '\n';
    output << "matched_command=" << result.matched_command << '\n';
    output << "args=" << join_strings(result.args, " ") << '\n';
    output << "confidence=" << result.confidence << '\n';
    output << "reasoning_summary=" << result.reasoning_summary << '\n';
    output << "requires_confirmation=" << (result.requires_confirmation ? "true" : "false") << '\n';
    output << "closest_commands=" << join_strings(result.closest_commands, ",") << '\n';
    output << "user_facing_message=" << result.user_facing_message << '\n';
    return output.str();
}

}  // namespace life_orchestrator::app
