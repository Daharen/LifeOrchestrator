#pragma once

#include "integration/inference/http_executor_contracts.h"
#include "integration/inference/inference_transport_contracts.h"

namespace life_orchestrator::integration::inference {

HttpRequestSpec build_openai_responses_request(const InferenceTransportRequest& request);
std::string default_openai_responses_endpoint();
bool is_openai_like_provider_name(const std::string& provider_name);

}  // namespace life_orchestrator::integration::inference
