#pragma once

#include "core/memory.hpp"

namespace life_orchestrator::core {

class MemoryService {
public:
    explicit MemoryService(IMemoryStore& store);

    MemoryResult log_module_execution_episode(const SourceModuleId& source_module_id,
                                              const std::string& event_type,
                                              const std::string& summary,
                                              const std::vector<EntityId>& associated_entities);

private:
    IMemoryStore& store_;
};

}  // namespace life_orchestrator::core
