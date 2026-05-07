$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $repoRoot "build.ps1"

if (-not (Test-Path $buildScript)) {
    throw "build.ps1 is missing"
}

$content = Get-Content -Path $buildScript -Raw
$requiredTargets = @(
    "src\data_builder\build_video_catalog.cpp",
    "src\simulator\generate_behavior_data.cpp",
    "src\common\generate_vectors.cpp",
    "tests\test_behavior_simulator.cpp",
    "tests\test_vector_builder.cpp"
)

foreach ($target in $requiredTargets) {
    if ($content -notmatch [regex]::Escape($target)) {
        throw "build.ps1 does not mention required target: $target"
    }
}

Write-Output "build script smoke test passed"
