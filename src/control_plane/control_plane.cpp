#include "control_plane/control_plane.hpp"

namespace life_orchestrator::control_plane {

ControlPlane::ControlPlane(ModuleRegistry& registry, EventLogger& event_logger)
    : registry_(registry), event_logger_(event_logger) {}

void ControlPlane::emit_event(core::EventCategory category,
                              const core::ActionRequest& request,
                              const std::string& module_id,
                              const std::string& message,
                              std::unordered_map<std::string, std::string> fields) {
    event_logger_.append(core::StructuredEvent{
        .category = category,
        .occurred_at = core::current_timestamp_utc(),
        .request_id = request.request_id,
        .module_id = module_id,
        .capability_id = request.capability_id,
        .message = message,
        .fields = std::move(fields),
    });
}

core::ActionResponse ControlPlane::dispatch(const core::ActionRequest& request) {
    emit_event(core::EventCategory::RequestReceived, request, "", "Request received by control plane.");

    if (request.request_id.empty()) {
        emit_event(core::EventCategory::RequestRejected, request, "", "Request rejected: empty request id.");
        return {request.request_id, core::ExecutionStatus::InvalidRequest, "",
                "Request rejected: request_id must not be empty.", {}, core::current_timestamp_utc()};
    }

    if (request.capability_id.empty()) {
        emit_event(core::EventCategory::RequestRejected, request, "", "Request rejected: empty capability id.");
        return {request.request_id, core::ExecutionStatus::InvalidRequest, "",
                "Request rejected: capability_id must not be empty.", {}, core::current_timestamp_utc()};
    }

    auto module = registry_.find_module_by_capability(request.capability_id);
    if (!module) {
        emit_event(core::EventCategory::RequestRejected, request, "", "Request rejected: capability not found.");
        return {request.request_id, core::ExecutionStatus::NotFound, "",
                "No registered module for requested capability.", {}, core::current_timestamp_utc()};
    }

    emit_event(core::EventCategory::RequestValidated, request, module->descriptor().module_id,
               "Request validated.");

    const auto module_risk = module->descriptor().risk_tier;
    // Deterministic rule for Sprint Step 1: reject if the request risk tier is lower than
    // the module's declared baseline risk tier. This avoids silent escalation.
    if (request.requested_risk_tier < module_risk) {
        emit_event(core::EventCategory::RiskCheckPerformed,
                   request,
                   module->descriptor().module_id,
                   "Risk check failed.",
                   {{"requested_risk_tier", core::to_string(request.requested_risk_tier)},
                    {"module_risk_tier", core::to_string(module_risk)}});
        emit_event(core::EventCategory::RequestRejected,
                   request,
                   module->descriptor().module_id,
                   "Request rejected: requested risk tier lower than module policy.");
        return {request.request_id,
                core::ExecutionStatus::InvalidRequest,
                module->descriptor().module_id,
                "Request risk tier is lower than module required risk tier.",
                {},
                core::current_timestamp_utc()};
    }

    emit_event(core::EventCategory::RiskCheckPerformed,
               request,
               module->descriptor().module_id,
               "Risk check passed.",
               {{"requested_risk_tier", core::to_string(request.requested_risk_tier)},
                {"module_risk_tier", core::to_string(module_risk)}});

    emit_event(core::EventCategory::DispatchStarted,
               request,
               module->descriptor().module_id,
               "Dispatch started.");

    auto response = module->execute(request);
    if (response.status == core::ExecutionStatus::Succeeded) {
        emit_event(core::EventCategory::DispatchCompleted,
                   request,
                   module->descriptor().module_id,
                   "Dispatch completed successfully.");
    } else {
        emit_event(core::EventCategory::DispatchFailed,
                   request,
                   module->descriptor().module_id,
                   "Dispatch failed.",
                   {{"status", core::to_string(response.status)}});
    }

    return response;
}

}  // namespace life_orchestrator::control_plane
