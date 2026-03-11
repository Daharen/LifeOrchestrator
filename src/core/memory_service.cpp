#include "core/memory_service.hpp"

namespace life_orchestrator::core {

MemoryService::MemoryService(IMemoryStore& store) : store_(store) {}

MemoryResult MemoryService::log_module_execution_episode(const SourceModuleId& source_module_id,
                                                         const std::string& event_type,
                                                         const std::string& summary,
                                                         const std::vector<EntityId>& associated_entities) {
    EpisodicMemoryRecord record{.record_id = source_module_id + ":" + current_timestamp_utc(),
                                .timestamp = current_timestamp_utc(),
                                .event_type = event_type,
                                .source_module_id = source_module_id,
                                .associated_entity_ids = associated_entities,
                                .summary = summary,
                                .details = {},
                                .version = 1};
    return store_.append_episodic_record(record);
}

}  // namespace life_orchestrator::core
