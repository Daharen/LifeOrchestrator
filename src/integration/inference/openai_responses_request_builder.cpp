#include "integration/inference/openai_responses_request_builder.h"

#include "integration/inference/openai_json_escape.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace life_orchestrator::integration::inference {
namespace {
std::string lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string schema_json() {
    return R"({"type":"object","additionalProperties":false,"required":["mode","matched_command","args","confidence","reasoning_summary","requires_confirmation","closest_commands","user_facing_message"],"properties":{"mode":{"type":"string"},"matched_command":{"type":"string"},"args":{"type":"string"},"confidence":{"type":"number"},"reasoning_summary":{"type":"string"},"requires_confirmation":{"type":"boolean"},"closest_commands":{"type":"string"},"user_facing_message":{"type":"string"}}})";
}

std::string grounding_instruction() {
    return "You are the intent router. Return only the canonical routing JSON object with fields mode, matched_command, args, confidence, reasoning_summary, requires_confirmation, closest_commands, user_facing_message. "
           "Allowed mode values: proposed or failure. matched_command must be either empty or one of the command names supplied in the prompt. "
           "Do not invent commands. For a household task such as Create laundry task, map to procedural-upsert-activity when grounded. "
           "For vague unsupported requests such as What can you do?, return mode failure with a helpful user_facing_message that nudges toward Help or exact commands.";
}
}  // namespace

std::string default_openai_responses_endpoint() {
    return "https://api.openai.com/v1/responses";
}

bool is_openai_like_provider_name(const std::string& provider_name) {
    const auto lowered = lower_copy(provider_name);
    return lowered == "openai" || lowered == "open-ai" || lowered == "openai-responses";
}

HttpRequestSpec build_openai_responses_request(const InferenceTransportRequest& request) {
    HttpRequestSpec http_request;
    http_request.url = request.options.endpoint_url.empty() ? default_openai_responses_endpoint() : request.options.endpoint_url;
    http_request.method = "POST";
    http_request.timeout_seconds = 30;
    http_request.headers = {{"Authorization", "Bearer " + request.options.api_key},
                            {"Content-Type", "application/json"},
                            {"Accept", "application/json"},
                            {"User-Agent", "LifeOrchestrator/2.0"}};

    std::ostringstream input;
    input << '[';
    for (std::size_t i = 0; i < request.messages.size(); ++i) {
        if (i > 0) input << ',';
        const auto content = request.messages[i].role == "system" ? grounding_instruction() + "\n" + request.messages[i].content : request.messages[i].content;
        input << "{\"role\":\"" << json_escape(request.messages[i].role)
              << "\",\"content\":[{\"type\":\"input_text\",\"text\":\"" << json_escape(content)
              << "\"}]}";
    }
    input << ']';

    std::ostringstream body;
    body << '{';
    body << "\"model\":\"" << json_escape(request.options.model_name) << "\",";
    body << "\"input\":" << input.str() << ',';
    body << "\"text\":{\"format\":{\"type\":\"json_schema\",\"name\":\"intent_routing_result\",\"schema\":" << schema_json() << ",\"strict\":true}}";
    body << '}';
    http_request.body = body.str();
    return http_request;
}

}  // namespace life_orchestrator::integration::inference
