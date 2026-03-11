#pragma once

#include "core/memory.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace life_orchestrator::integration {

class IntegrationConfigurationRepository {
public:
    explicit IntegrationConfigurationRepository(std::filesystem::path data_root);

    core::MemoryResult upsert(const core::IntegrationConfigurationRecord& record);
    core::MemoryResult load();
    core::MemoryResult persist_manifest() const;
    std::optional<core::IntegrationConfigurationRecord> get_by_id(
        const core::IntegrationConfigId& id) const;
    std::vector<core::IntegrationConfigurationRecord> list_all() const;

private:
    std::filesystem::path data_root_;
    std::unordered_map<core::IntegrationConfigId, core::IntegrationConfigurationRecord> by_id_;
};

}  // namespace life_orchestrator::integration
