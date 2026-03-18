# Application Bootstrap Baseline

## What this lane implemented
- Added the canonical headless runtime executable target `life_orchestrator_app`.
- Added a thin application bootstrap composition root under `src/app/`.
- Added deterministic runtime path resolution, module registration, memory initialization, integration configuration initialization, and command execution wiring.
- Added permanent repo-root launcher wrappers `run.bat` and `run.ps1` for the existing outer updater-launcher chain.
- Hardened the repo-root launcher so it remains the permanent boundary for CMake configure/build plus Windows multi-config executable discovery.

## Canonical executable target
- Target: `life_orchestrator_app`
- Permanent launch chain: external updater-launcher -> repo-root `run.bat` or `run.ps1` -> `life_orchestrator_app`
- The outer updater-launcher should continue calling only the repo-root launcher surface and should not need to know CMake target layout details.

## Repo-root launcher convention
- `run.ps1` remains a thin launcher that resolves the repo root, configures CMake into `build`, builds the canonical `life_orchestrator_app` target, discovers the executable, forwards arguments, and returns the application exit code.
- On Windows multi-config generators, `run.ps1` builds `Debug` by default and checks `build\\Debug\\life_orchestrator_app.exe` before falling back through `Release`, `RelWithDebInfo`, `MinSizeRel`, and then single-config output locations.
- `run.bat` remains a thin delegating wrapper to `run.ps1` for the outer updater-launcher.

## Runtime bootstrap config
- `ApplicationBootstrapConfig`
- Fields:
  - `application_name`
  - `data_root_path`
  - `events_file_path`
  - `integration_config_root_path`
  - `memory_root_path`
  - `default_timezone`
  - `run_mode`
  - `allow_seed_data`
  - `log_startup_summary`

## Command surface
- `status`
- `list-modules`
- `bootstrap-check`
- `schedule-health-check`

## Path-resolution rule
1. Explicit command-line arguments.
2. `LIFE_ORCHESTRATOR_DATA_ROOT` environment variable for the application data root only.
3. Deterministic default under `<repo>/runtime`.

## Bootstrap composition order
1. Resolve config.
2. Initialize event logger.
3. Initialize file-backed memory store.
4. Initialize memory service.
5. Initialize integration configuration repository.
6. Initialize module registry.
7. Initialize control plane.
8. Register the real scheduling coordination module in deterministic order.
9. Execute the requested command.

## Deferred in this lane
- GUI or operator console.
- External API integrations.
- Behavioral triage.
- Background loops or daemons.
- Any second integration configuration system.

## Runtime authority note
Future GUI or operator-console work remains non-authoritative at runtime. The authoritative runtime bootstrap remains the headless application executable plus the repo-root launcher convention.

## Manual validation
- `cmake -S . -B build`
- `cmake --build build --target life_orchestrator_app`
- `./build/life_orchestrator_app status`
- On Windows: `run.ps1 status`
- On Windows: `run.bat status`
- On Windows: `run.bat list-modules`
- On Windows: `run.bat bootstrap-check`
- On Windows: `run.bat schedule-health-check`
- Most important validation: the existing external updater-launcher can continue invoking the repo-root launcher without any change.
