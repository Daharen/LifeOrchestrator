#include "integration/inference/inference_transport_contracts.h"

namespace life_orchestrator::integration::inference {

std::string redact_secret(const std::string& value) {
    if (value.empty()) return "unset";
    if (value.size() <= 4) return "****";
    return value.substr(0, 2) + "***" + value.substr(value.size() - 2);
}

std::string summarize_transport_error(const InferenceTransportError& error) {
    return error.failure_class + ":" + error.message;
}

}  // namespace life_orchestrator::integration::inference
