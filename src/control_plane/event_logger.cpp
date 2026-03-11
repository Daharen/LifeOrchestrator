#include "control_plane/event_logger.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace life_orchestrator::control_plane {
namespace {

std::string escape_json(const std::string& value) {
    std::ostringstream out;
    for (const char ch : value) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << ch; break;
        }
    }
    return out.str();
}

std::string to_ndjson(const core::StructuredEvent& event) {
    std::vector<std::pair<std::string, std::string>> field_pairs(event.fields.begin(), event.fields.end());
    std::sort(field_pairs.begin(), field_pairs.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

    std::ostringstream out;
    out << '{'
        << "\"category\":\"" << escape_json(core::to_string(event.category)) << "\"," 
        << "\"occurred_at\":\"" << escape_json(event.occurred_at) << "\"," 
        << "\"request_id\":\"" << escape_json(event.request_id) << "\"," 
        << "\"module_id\":\"" << escape_json(event.module_id) << "\"," 
        << "\"capability_id\":\"" << escape_json(event.capability_id) << "\"," 
        << "\"message\":\"" << escape_json(event.message) << "\"," 
        << "\"fields\":{";

    bool first = true;
    for (const auto& [key, value] : field_pairs) {
        if (!first) {
            out << ',';
        }
        first = false;
        out << '"' << escape_json(key) << "\":\"" << escape_json(value) << '"';
    }

    out << "}}";
    return out.str();
}

}  // namespace

EventLogger::EventLogger(std::filesystem::path log_path) : log_path_(std::move(log_path)) {}

bool EventLogger::append(const core::StructuredEvent& event) {
    if (log_path_.has_parent_path()) {
        std::filesystem::create_directories(log_path_.parent_path());
    }

    std::ofstream out(log_path_, std::ios::app);
    if (!out.is_open()) {
        return false;
    }

    out << to_ndjson(event) << '\n';
    events_.push_back(event);
    return static_cast<bool>(out);
}

const std::vector<core::StructuredEvent>& EventLogger::in_memory_events() const { return events_; }

const std::filesystem::path& EventLogger::log_path() const { return log_path_; }

}  // namespace life_orchestrator::control_plane
