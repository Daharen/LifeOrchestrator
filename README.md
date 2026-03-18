# LifeOrchestrator

## Canonical launch method
- Normal outer entrypoint: use the existing external updater-launcher.
- Repository-local launch contract: that outer launcher should detect and invoke `run.bat` or `run.ps1` at the repo root.
- Canonical executable target: `life_orchestrator_app`.

Examples from the repo root:
- `run.bat status`
- `run.bat list-modules`
- `run.bat bootstrap-check`
- `run.bat schedule-health-check`
