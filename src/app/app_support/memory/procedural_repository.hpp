#pragma once

#include "app/app_support/artifact_query_service.hpp"

namespace life_orchestrator::app::memory {
std::vector<ArtifactEnvelope> list_procedural_proposals(core::MemoryService& memory_service);
}
