# Scheduling Coordination Baseline

## Sprint 3 implementation
- Added one authoritative scheduling contract layer in `src/core/contracts.*` for schedule ids, enums, and deterministic records.
- Extended the file-backed memory layer with scheduling persistence under `data/memory/scheduling/*.ndjson` through the same `FileMemoryStore` used by prior memory layers.
- Replaced the scheduling stub with a real `SchedulingCoordinationModule` and a deterministic `SchedulingEngine`.
- Extended the smoke path so the control plane registers the real module, adds a commitment, proposes time blocks, commits one proposal, and persists artifacts.

## Authoritative scheduling contracts
- Core ids: `ScheduleItemId`, `ProposalId`, `ConstraintSetId`, `WindowId`, `ScheduleDecisionId`.
- Core enums: `ScheduleItemType`, `ScheduleStatus`, `ConflictType`, `SchedulingPriority`, `ProposalStatus`, `SchedulingOperationType`.
- Core records: `ScheduledCommitment`, `SchedulingTaskCandidate`, `AvailabilityWindow`, `SchedulingConstraintSet`, `SchedulingConflict`, `SchedulingProposal`, `SchedulingDecisionRecord`.

## Persistence mapping strategy
- Chosen strategy: extend the canonical memory layer directly.
- Scheduling records are persisted in the same authoritative store root under:
  - `memory/scheduling/commitments.ndjson`
  - `memory/scheduling/task_candidates.ndjson`
  - `memory/scheduling/availability_windows.ndjson`
  - `memory/scheduling/constraint_sets.ndjson`
  - `memory/scheduling/proposals.ndjson`
  - `memory/scheduling/decisions.ndjson`
  - `memory/scheduling/conflicts.ndjson`

## Deterministic proposal rule
- Gather commitments and availability windows for the horizon.
- Sort commitments by `start_time`, then stable id.
- Construct free gaps from each allowed availability window while applying minimum-gap padding around existing commitments.
- Filter gaps by task duration, buffers, earliest/latest bounds, and window constraints.
- Rank proposals by earliest valid start time first, then shortest slack, then proposal id.

## Supported Sprint 3 constraints
- Minimum gap between commitments.
- Allowed and blocked availability windows.
- Working-hours-only fallback when explicit windows are absent.
- Task `earliest_start`, `latest_end`, duration, and before/after buffers.
- Deterministic overlap and invalid-window conflict detection.

## Intentionally deferred
- External calendar adapters.
- Reminder or notification delivery.
- Full recurrence semantics.
- NLP parsing.
- Multi-objective optimization and adaptive behavior.
- Behavioral triage logic inside the scheduler.

## Sprint 4 placement
Sprint 4 behavioral triage should sit above scheduling outputs by consuming persisted proposals, decisions, conflicts, and commitments. It should not be embedded inside the deterministic scheduling engine.
