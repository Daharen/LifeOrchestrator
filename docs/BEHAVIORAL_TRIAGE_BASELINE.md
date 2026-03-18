# Behavioral Triage Baseline

## Sprint 4 scope implemented
- Added an authoritative behavioral contract layer in `src/core/behavioral.hpp` and `src/core/behavioral.cpp`.
- Added deterministic behavioral triage persistence through the existing authoritative memory store under `data_root/memory/behavioral_triage/`.
- Added a real coordination module, deterministic triage engine, control-plane exposure, observability, tests, and app smoke commands.

## Authoritative behavioral contracts
The shared contract layer defines:
- Strong ids for proposals, decisions, backlog items, state snapshots, and interventions.
- Canonical enums for proposal type, priority, decision type, capacity level, psychological state, presentation mode, backlog status, and operation type.
- Deterministic structs for proposals, state snapshots, triage decisions, backlog items, intervention records, and behavioral memory summary.

## Persistence mapping
Behavioral triage extends the existing memory service and file-backed memory store rather than introducing a new store.
Persisted append-safe regions:
- `memory/behavioral_triage/proposals.ndjson`
- `memory/behavioral_triage/state_snapshots.ndjson`
- `memory/behavioral_triage/decisions.ndjson`
- `memory/behavioral_triage/backlog.ndjson`
- `memory/behavioral_triage/interventions.ndjson`

## Deterministic ROI and capacity rules
- ROI = `expected_benefit / max(1, estimated_behavioral_effort)`.
- Capacity falls back to one documented default snapshot if no recorded state exists.
- Capacity is derived from active interventions, backlog count, schedule density, compliance, failures, fatigue, and stress.
- Active intervention caps are fixed: `Low=1`, `Medium=2`, `High=3`, `Recovery=0`.
- Effort gates are fixed: `Recovery=0`, `Low=2`, `Medium=5`, `High=10`.
- Ranking is deterministic: highest priority, then highest ROI, then earliest relevant time, then stable proposal id.
- Expired proposals are rejected.
- Valuable but over-capacity proposals are deferred into backlog.
- Lower-value over-capacity proposals are backlogged.
- Approved proposals become intervention records; deferred and backlogged proposals remain auditable through backlog items and decision records.

## Command surface added
- `life_orchestrator_app behavioral-health-check`
- `life_orchestrator_app behavioral-list-backlog`

These validate the module through the canonical headless app shell without changing the documented launcher convention.

## Intentionally deferred
- Procedural Auditor implementation.
- Notification delivery systems.
- GUI work.
- Conversational or NLP behavior interpretation.
- External integrations that bypass triage.

## Sprint 5 flow
Sprint 5 Procedural Auditor outputs should enter Behavioral Triage as proposals and state-affecting evidence. Auditor outputs should not bypass triage approval, backlog handling, or intervention ordering.
