# Control Plane Baseline (Sprint Step 1)

## Implemented in this lane
- Deterministic C++20/CMake kernel baseline with authoritative contracts under `src/core`.
- Single authoritative module contract (`IModule`), registry (`ModuleRegistry`), event schema (`StructuredEvent`), and append-safe NDJSON logger (`EventLogger`).
- Deterministic synchronous `ControlPlane::dispatch` sequence with explicit event emission.
- One stub module (`SchedulingCoordinationStubModule`) proving end-to-end registration and dispatch.
- Minimal tests validating registry rules, dispatch flow, events, not-found handling, and risk handling.

## Authoritative contracts
- Shared enums/types: `ModuleClass`, `RiskTier`, `ExecutionStatus`, `EventCategory`, aliases for module/capability/request/timestamp.
- `ModuleDescriptor` as the canonical module metadata shape.
- `ActionRequest` and `ActionResponse` as canonical request/response envelopes.
- `StructuredEvent` as canonical observability event record.

## Deterministic dispatch sequence
1. Emit `RequestReceived`.
2. Validate request structure.
3. Emit `RequestValidated` or `RequestRejected`.
4. Resolve capability to module.
5. Perform risk check.
6. Emit `RiskCheckPerformed`.
7. Emit `DispatchStarted`.
8. Execute module.
9. Emit `DispatchCompleted` or `DispatchFailed`.
10. Return `ActionResponse`.

## Chosen risk rule for step 1
- If a request declares a risk tier lower than the module descriptor's `risk_tier`, dispatch is rejected as `InvalidRequest`.
- This is intentionally strict to avoid silent risk escalation in the baseline.

## Deferred intentionally
- Real external integrations, memory backends, graph persistence, LLM/provider plumbing, advanced behavioral triage, procedural auditing, UI, async job orchestration.

## Step 2 memory connection
- Step 2 memory components can attach behind `IModule` implementations and use `ActionRequest`/`ActionResponse` plus `StructuredEvent` to capture memory reads/writes deterministically without changing the control-plane contract surface.
