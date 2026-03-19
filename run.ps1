$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $RepoRoot 'build'

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

$Target = 'life_orchestrator_app'
$RunArgs = @($args)

if ($args.Count -gt 0 -and ($args[0] -eq 'gui' -or $args[0] -eq 'admin-gui')) {
    if (-not $IsWindows) {
        Write-Error 'life_orchestrator_admin_gui is only available on Windows.'
        exit 1
    }

    $Target = 'life_orchestrator_admin_gui'
    if ($args.Count -gt 1) {
        $RunArgs = $args[1..($args.Count - 1)]
    }
    else {
        $RunArgs = @()
    }
}

& cmake -S $RepoRoot -B $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build $BuildDir --target $Target
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$AppPath = Join-Path $BuildDir ($Target + '.exe')
if (-not (Test-Path $AppPath)) {
    $AppPath = Join-Path $BuildDir $Target
}

& $AppPath @RunArgs
exit $LASTEXITCODE
