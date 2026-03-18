$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $RepoRoot 'build'

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

& cmake -S $RepoRoot -B $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build $BuildDir --target life_orchestrator_app
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$AppPath = Join-Path $BuildDir 'life_orchestrator_app.exe'
if (-not (Test-Path $AppPath)) {
    $AppPath = Join-Path $BuildDir 'life_orchestrator_app'
}

& $AppPath @args
exit $LASTEXITCODE
