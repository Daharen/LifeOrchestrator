#pragma once

#include "integration/inference/inference_transport_contracts.h"

#include <optional>
#include <string>

namespace life_orchestrator::integration::inference {

struct OpenAIResponseParseResult {
    bool success = false;
    std::string normalized_output;
    std::optional<InferenceTransportError> error;
    InferenceTransportUsage usage;
};

std::optional<std::string> extract_openai_output_text_payload(const std::string& response_body);
std::optional<std::string> parse_openai_structured_output_to_key_value(const std::string& response_body);
InferenceTransportUsage parse_openai_usage(const std::string& response_body);
OpenAIResponseParseResult parse_openai_response_body(const std::string& response_body, int http_status, const std::string& request_id = {}, const std::string& body_preview = {});

}  // namespace life_orchestrator::integration::inference
