param(
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
$buildDirectory = Join-Path $PSScriptRoot 'build'

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCommand) {
    $cmake = $cmakeCommand.Source
} else {
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    )
    $cmake = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if (-not $cmake) {
    throw 'CMake was not found. Install Visual Studio (2022 or newer) with the Desktop development with C++ workload.'
}

# No -G: CMake picks the newest installed Visual Studio (2022 locally,
# whatever the windows-latest CI image ships).
& $cmake -S $PSScriptRoot -B $buildDirectory -A x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $cmake --build $buildDirectory --config Release --target smart-grind-sim --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Test) {
    & $cmake --build $buildDirectory --config Release --target RUN_TESTS
    exit $LASTEXITCODE
}

Write-Host "Simulator built: $buildDirectory\Release\smart-grind-sim.exe"
