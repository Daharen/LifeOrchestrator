# Memory Layer Baseline (Sprint Step 2)

## Implemented in this sprint
- Canonical memory contracts for life graph, episodic, preferences, relationship memory, project memory, behavioral history, retrieval index placeholder, and integration configuration.
- Deterministic `IMemoryStore` interface plus file-backed `FileMemoryStore` implementation.
- Headless `IntegrationConfigurationRepository` for canonical integration config records.
- Deterministic query surface for entity lookup, relationship traversal, project lookup, preference lookup, episodic recency, behavioral history lookup, and memory summary.
- Memory observability events written through the existing event logger.

## Authoritative contracts
- All canonical memory aliases, enums, and record structs are centralized in `src/core/memory.hpp`.
- Integration configuration schema is part of the canonical memory contract (`IntegrationConfigurationRecord`) and is persisted by `src/integration/integration_configuration_repository.*`.

## Storage format and load strategy
- Storage uses append-only line records in auditable NDJSON-like files under one data root:
  - `memory/life_graph/entities.ndjson`
  - `memory/life_graph/relationships.ndjson`
  - `memory/episodic/records.ndjson`
  - `memory/preferences/records.ndjson`
  - `memory/relationship_memory/records.ndjson`
  - `memory/project_memory/records.ndjson`
  - `memory/behavioral_history/records.ndjson`
  - `memory/retrieval_index/records.ndjson`
  - `memory/integration_configuration/records.ndjson`
  - `memory/metadata/store_manifest.json`
- Startup load materializes authoritative in-memory state by replaying files and applying deterministic last-write-wins semantics for upsert layers.

## Versioning and overwrite strategy
- Every canonical record includes source attribution and version fields.
- Strategy is append-only history with deterministic in-memory materialization of latest upserts.
- Corrupt or malformed lines fail load with explicit errors rather than silent skips.

## Observability
- Memory events emit through shared `StructuredEvent` + `EventLogger` with categories:
  `MemoryWriteStarted`, `MemoryWriteCompleted`, `MemoryWriteFailed`, `MemoryReadPerformed`,
  `MemoryQueryPerformed`, `MemoryLoadStarted`, `MemoryLoadCompleted`, `MemoryLoadFailed`.

## Intentionally deferred
- No vector search, embeddings, semantic retrieval, graph databases, or external API integrations.
- No GUI runtime dependency.
- No secret vault integration beyond placeholder/reference fields.

## Sprint Step 3 consumption path
- Modules can consume `MemoryService` + `IMemoryStore` deterministic query API.
- Scheduling coordination can read structured life entities/relationships and append episodic outcomes.

## Integration configuration operator model
- Integration configuration is headless-readable/writable and persisted as authoritative records.
- Future administrative GUI is expected to edit these records, but GUI is not required at runtime.
