#pragma once

#include "core/contracts.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace life_orchestrator::control_plane {

class EventLogger {
public:
    explicit EventLogger(std::filesystem::path log_path);

    bool append(const core::StructuredEvent& event);
    const std::vector<core::StructuredEvent>& in_memory_events() const;
    const std::filesystem::path& log_path() const;

private:
    std::filesystem::path log_path_;
    std::vector<core::StructuredEvent> events_;
};

}  // namespace life_orchestrator::control_plane
