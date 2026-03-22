$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $RepoRoot 'build'
$CachePath = Join-Path $BuildDir 'CMakeCache.txt'

if (-not (Test-Path $BuildDir)) {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

function Get-CMakeCacheValue {
    param(
        [string]$CacheFile,
        [string]$EntryName
    )

    if (-not (Test-Path $CacheFile)) {
        return $null
    }

    $escapedEntryName = [regex]::Escape($EntryName)
    $line = Select-String -Path $CacheFile -Pattern "^${escapedEntryName}(:[^=]+)?=(.*)$" | Select-Object -First 1
    if (-not $line) {
        return $null
    }

    return $line.Matches[0].Groups[2].Value
}

function Test-IsWindowsHost {
    if ($null -ne $IsWindows -and $IsWindows) {
        return $true
    }

    if ($env:OS -eq 'Windows_NT') {
        return $true
    }

    return $false
}

$isWindowsHost = Test-IsWindowsHost
$Target = 'life_orchestrator_app'
$RunArgs = @($args)

if ($args.Count -eq 0 -and $isWindowsHost) {
    $Target = 'life_orchestrator_assistant_shell'
    $RunArgs = @()
}
elseif ($args.Count -gt 0) {
    $firstArg = $args[0]

    if ($firstArg -eq 'gui' -or $firstArg -eq 'admin-gui') {
        if (-not $isWindowsHost) {
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
    elseif ($firstArg -eq 'assistant' -or $firstArg -eq 'assistant-shell' -or $firstArg -eq 'shell') {
        if (-not $isWindowsHost) {
            Write-Error 'life_orchestrator_assistant_shell is only available on Windows.'
            exit 1
        }

        $Target = 'life_orchestrator_assistant_shell'
        if ($args.Count -gt 1) {
            $RunArgs = $args[1..($args.Count - 1)]
        }
        else {
            $RunArgs = @()
        }
    }
    elseif ($firstArg -eq 'cli' -or $firstArg -eq 'headless' -or $firstArg -eq 'app') {
        $Target = 'life_orchestrator_app'
        if ($args.Count -gt 1) {
            $RunArgs = $args[1..($args.Count - 1)]
        }
        else {
            $RunArgs = @()
        }
    }
}

& cmake -S $RepoRoot -B $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$generator = Get-CMakeCacheValue -CacheFile $CachePath -EntryName 'CMAKE_GENERATOR'
$configurationTypes = Get-CMakeCacheValue -CacheFile $CachePath -EntryName 'CMAKE_CONFIGURATION_TYPES'
$IsMultiConfig = -not [string]::IsNullOrWhiteSpace($configurationTypes)

$SelectedConfig = $env:LIFE_ORCHESTRATOR_BUILD_CONFIG
if ([string]::IsNullOrWhiteSpace($SelectedConfig)) {
    $SelectedConfig = 'Debug'
}

$buildArgs = @('--build', $BuildDir, '--target', $Target)
if ($IsMultiConfig) {
    $buildArgs += @('--config', $SelectedConfig)
}

& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$candidatePaths = @()

if ($IsMultiConfig) {
    foreach ($configName in @($SelectedConfig, 'Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')) {
        if ([string]::IsNullOrWhiteSpace($configName)) {
            continue
        }

        $candidateExe = Join-Path (Join-Path $BuildDir $configName) ($Target + '.exe')
        $candidateBare = Join-Path (Join-Path $BuildDir $configName) $Target

        if (-not ($candidatePaths -contains $candidateExe)) {
            $candidatePaths += $candidateExe
        }
        if (-not ($candidatePaths -contains $candidateBare)) {
            $candidatePaths += $candidateBare
        }
    }
}

$candidatePaths += @(
    (Join-Path $BuildDir ($Target + '.exe')),
    (Join-Path $BuildDir $Target)
)

$AppPath = $candidatePaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $AppPath) {
    $attemptedConfig = if ($IsMultiConfig) { $SelectedConfig } else { 'default' }
    $mode = if ($IsMultiConfig) { 'multi-config' } else { 'single-config' }
    $checkedPaths = $candidatePaths | ForEach-Object { "  - $_" }

    Write-Error ((
        "Unable to locate built executable for target '$Target'.`n" +
        "Generator: $generator`n" +
        "Launcher mode: $mode`n" +
        "Attempted build configuration: $attemptedConfig`n" +
        "Checked paths:`n" +
        ($checkedPaths -join "`n")
    ))
    exit 1
}

$IsGuiTarget = $Target -eq 'life_orchestrator_assistant_shell' -or $Target -eq 'life_orchestrator_admin_gui'

if ($isWindowsHost -and $IsGuiTarget) {
    $process = Start-Process -FilePath $AppPath -ArgumentList $RunArgs -WorkingDirectory $RepoRoot -PassThru
    Start-Sleep -Milliseconds 750

    if ($process.HasExited) {
        $exitCode = $process.ExitCode
        Write-Error "GUI target '$Target' exited immediately after launch. ExitCode=$exitCode AppPath=$AppPath"
        exit 1
    }

    exit 0
}

& $AppPath @RunArgs
exit $LASTEXITCODE
