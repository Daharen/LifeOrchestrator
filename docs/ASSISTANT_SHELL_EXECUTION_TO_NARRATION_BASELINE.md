# Assistant Shell Execution-to-Narration Baseline

## What this lane implemented
- Added an explicit Assistant Shell turn contract that separates routing diagnostics, authoritative runtime outcome, and primary narration.
- Updated shell submission and confirmation flows to build deterministic narration only after the runtime outcome is known.
- Kept routing/debug details available, but demoted them to secondary transcript content.

## Authoritative shell-facing outcome contract
- `AssistantShellRuntimeOutcome` is the shell-facing source of truth for what actually happened.
- `AssistantShellTurnResponse` carries:
  - the original user input,
  - machine-facing routing data (`AssistantShellExecutionSummary`),
  - authoritative runtime outcome,
  - primary narration text,
  - secondary diagnostics text.

## Machine-facing vs user-facing
- Machine-facing:
  - intent-routing diagnostics,
  - route lineage,
  - provider/debug metadata,
  - execution summary details.
- User-facing:
  - deterministic narration derived from `AssistantShellRuntimeOutcome`,
  - confirmation prompts,
  - artifact cards that remain supplemental to the narration.

## Deferred to later Step 8 lanes
- No second model pass for narration.
- No broad conversational mode or multi-turn planning.
- No shell-local execution authority or runtime-contract redesign.
- No visual redesign beyond making narration primary and diagnostics secondary.

## Expected validation
- Repository builds and existing tests pass.
- Assistant Shell still starts with the existing launcher behavior.
- Deterministic commands, provider-assisted routes, unresolved requests, and confirmation flows all end with primary narration grounded in runtime outcome.
