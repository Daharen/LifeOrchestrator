# LifeOrchestrator

## Canonical launch method
- Normal outer entrypoint: use the existing external updater-launcher.
- Repository-local launcher surface: the outer launcher should detect and invoke repo-root `run.bat` or `run.ps1`.
- Canonical executable target: `life_orchestrator_app`.
- Repo-root launchers own CMake configure, CMake build, configuration-aware executable discovery, argument forwarding, and exit-code propagation.
- Windows multi-config generators are supported: the launcher will build `Debug` by default and resolve `life_orchestrator_app` from config-specific output folders such as `build\\Debug` before falling back to single-config locations.
- The outer updater-launcher should not need to know CMake generator details or target output layout.

Examples from the repo root:
- `run.bat status`
- `run.bat list-modules`
- `run.bat bootstrap-check`
- `run.bat schedule-health-check`
- `run.ps1 status`

## Manual validation
- `cmake -S . -B build`
- `cmake --build build --target life_orchestrator_app`
- `./build/life_orchestrator_app status`
- On Windows: `run.ps1 status`
- On Windows: `run.bat status`
- On Windows: `run.bat list-modules`
- On Windows: `run.bat bootstrap-check`
- On Windows: `run.bat schedule-health-check`


## Additional validation commands
- `life_orchestrator_app behavioral-health-check`
- `life_orchestrator_app behavioral-list-backlog`
