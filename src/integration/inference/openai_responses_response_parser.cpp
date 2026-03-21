#include "integration/inference/openai_responses_response_parser.h"

#include "integration/inference/openai_json_escape.h"

#include <sstream>

namespace life_orchestrator::integration::inference {
namespace {
std::string trim_quotes(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') return value.substr(1, value.size() - 2);
    return value;
}

std::string scalar_field(const std::string& json, const std::string& key, const std::string& fallback = {}) {
    if (const auto string_value = json_extract_string_field(json, key); string_value.has_value()) return *string_value;
    if (const auto raw_value = json_extract_raw_field(json, key); raw_value.has_value()) return trim_quotes(*raw_value);
    return fallback;
}
}

InferenceTransportUsage parse_openai_usage(const std::string& response_body) {
    InferenceTransportUsage usage;
    if (const auto raw = json_extract_raw_field(response_body, "input_tokens"); raw.has_value()) usage.input_tokens = std::stoi(*raw);
    if (const auto raw = json_extract_raw_field(response_body, "output_tokens"); raw.has_value()) usage.output_tokens = std::stoi(*raw);
    if (const auto raw = json_extract_raw_field(response_body, "total_tokens"); raw.has_value()) usage.total_tokens = std::stoi(*raw);
    return usage;
}

std::optional<std::string> parse_openai_structured_output_to_key_value(const std::string& response_body) {
    std::string structured_json;
    if (const auto direct = json_extract_raw_field(response_body, "mode"); direct.has_value()) {
        structured_json = response_body;
    } else if (const auto text_value = json_extract_string_field(response_body, "text"); text_value.has_value()) {
        structured_json = *text_value;
    } else if (const auto output_text = json_extract_string_field(response_body, "output_text"); output_text.has_value()) {
        structured_json = *output_text;
    } else {
        return std::nullopt;
    }

    const auto mode = scalar_field(structured_json, "mode");
    const auto matched_command = scalar_field(structured_json, "matched_command");
    const auto args = scalar_field(structured_json, "args");
    const auto confidence = scalar_field(structured_json, "confidence", "0");
    const auto reasoning_summary = scalar_field(structured_json, "reasoning_summary");
    const auto requires_confirmation = scalar_field(structured_json, "requires_confirmation", "false");
    const auto closest_commands = scalar_field(structured_json, "closest_commands");
    const auto user_facing_message = scalar_field(structured_json, "user_facing_message");
    if (mode.empty() && matched_command.empty() && user_facing_message.empty()) return std::nullopt;

    std::ostringstream out;
    out << "mode=" << mode << '\n'
        << "matched_command=" << matched_command << '\n'
        << "args=" << args << '\n'
        << "confidence=" << confidence << '\n'
        << "reasoning_summary=" << reasoning_summary << '\n'
        << "requires_confirmation=" << requires_confirmation << '\n'
        << "closest_commands=" << closest_commands << '\n'
        << "user_facing_message=" << user_facing_message << '\n';
    return out.str();
}

}  // namespace life_orchestrator::integration::inference
