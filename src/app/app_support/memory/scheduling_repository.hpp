#pragma once

#include "app/app_support/artifact_query_service.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_scheduling_candidates(core::MemoryService& memory_service);
std::vector<ArtifactEnvelope> list_schedule_proposals(core::MemoryService& memory_service);
}
