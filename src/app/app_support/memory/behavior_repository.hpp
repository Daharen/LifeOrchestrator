#pragma once

#include "app/app_support/artifact_query_service.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_behavioral_backlog(core::MemoryService& memory_service);
std::vector<ArtifactEnvelope> list_behavioral_interventions(core::MemoryService& memory_service);
std::vector<ArtifactEnvelope> list_behavioral_reevaluations(core::MemoryService& memory_service);
}
