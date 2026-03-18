$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $RepoRoot 'build'
$CachePath = Join-Path $BuildDir 'CMakeCache.txt'
$TargetName = 'life_orchestrator_app'
$SelectedConfig = $null
$IsMultiConfig = $false

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

& cmake -S $RepoRoot -B $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$generator = Get-CMakeCacheValue -CacheFile $CachePath -EntryName 'CMAKE_GENERATOR'
$configurationTypes = Get-CMakeCacheValue -CacheFile $CachePath -EntryName 'CMAKE_CONFIGURATION_TYPES'
if ($configurationTypes) {
    $IsMultiConfig = $true
    $SelectedConfig = 'Debug'
}

$buildArgs = @('--build', $BuildDir, '--target', $TargetName)
if ($IsMultiConfig) {
    $buildArgs += @('--config', $SelectedConfig)
}

& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$candidatePaths = @()
if ($IsMultiConfig) {
    foreach ($configName in @($SelectedConfig, 'Release', 'RelWithDebInfo', 'MinSizeRel')) {
        $candidatePath = Join-Path (Join-Path $BuildDir $configName) "$TargetName.exe"
        if ($configName -and -not ($candidatePaths -contains $candidatePath)) {
            $candidatePaths += $candidatePath
        }
    }
}
$candidatePaths += @(
    (Join-Path $BuildDir "$TargetName.exe"),
    (Join-Path $BuildDir $TargetName)
)

$AppPath = $candidatePaths | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $AppPath) {
    $attemptedConfig = if ($SelectedConfig) { $SelectedConfig } else { 'default' }
    $mode = if ($IsMultiConfig) { 'multi-config' } else { 'single-config' }
    $checkedPaths = $candidatePaths | ForEach-Object { "  - $_" }
    Write-Error ((
        "Unable to locate built executable for target '$TargetName'.`n" +
        "Generator: $generator`n" +
        "Launcher mode: $mode`n" +
        "Attempted build configuration: $attemptedConfig`n" +
        "Checked paths:`n" +
        ($checkedPaths -join "`n")
    ))
}

& $AppPath @args
exit $LASTEXITCODE
