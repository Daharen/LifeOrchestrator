#pragma once

#include "integration/inference/inference_transport_contracts.h"

#include <optional>
#include <string>

namespace life_orchestrator::integration::inference {

std::optional<std::string> parse_openai_structured_output_to_key_value(const std::string& response_body);
InferenceTransportUsage parse_openai_usage(const std::string& response_body);

}  // namespace life_orchestrator::integration::inference
