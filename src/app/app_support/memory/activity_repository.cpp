#include "app/app_support/memory/activity_repository.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_activity_inventory(core::MemoryService& memory_service) {
    std::vector<ArtifactEnvelope> artifacts;
    const auto records = memory_service.list_activity_inventory_items();
    if (!records.ok || !records.value) return artifacts;
    for (const auto& item : *records.value) {
        artifacts.push_back({"activity_inventory",
                             item.activity_inventory_item_id,
                             {{"activity_inventory_item_id", item.activity_inventory_item_id},
                              {"title", item.title},
                              {"description", item.description},
                              {"domain_source", item.domain_source},
                              {"frequency", item.frequency},
                              {"duration_minutes", std::to_string(item.duration_minutes)},
                              {"effort_estimate", std::to_string(item.effort_estimate)},
                              {"outcome_value", std::to_string(item.outcome_value)},
                              {"source_module_id", item.source_module_id},
                              {"created_at", item.created_at},
                              {"updated_at", item.updated_at},
                              {"version", std::to_string(item.version)}},
                             {{"attributes", item.attributes}}});
    }
    return artifacts;
}
}  // namespace life_orchestrator::app::memory
