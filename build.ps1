param(
    [string]$Compiler = "g++"
)

$ErrorActionPreference = "Stop"

$repoRoot = $PSScriptRoot

$compilerCommand = Get-Command $Compiler -ErrorAction Stop

$targets = @(
    @{
        Name = "build_video_catalog"
        Output = "src\data_builder\build_video_catalog.exe"
        Sources = @(
            "src\data_builder\build_video_catalog.cpp",
            "src\data_builder\video_cleaner.cpp"
        )
    },
    @{
        Name = "generate_behavior_data"
        Output = "src\simulator\generate_behavior_data.exe"
        Sources = @(
            "src\simulator\generate_behavior_data.cpp",
            "src\simulator\behavior_simulator.cpp",
            "src\data_builder\video_cleaner.cpp"
        )
    },
    @{
        Name = "generate_vectors"
        Output = "src\common\generate_vectors.exe"
        Sources = @(
            "src\common\generate_vectors.cpp",
            "src\common\vector_builder.cpp",
            "src\data_builder\video_cleaner.cpp"
        )
    },
    @{
        Name = "test_behavior_simulator"
        Output = "tests\test_behavior_simulator.exe"
        Sources = @(
            "tests\test_behavior_simulator.cpp",
            "src\simulator\behavior_simulator.cpp",
            "src\data_builder\video_cleaner.cpp"
        )
    },
    @{
        Name = "test_vector_builder"
        Output = "tests\test_vector_builder.exe"
        Sources = @(
            "tests\test_vector_builder.cpp",
            "src\common\vector_builder.cpp",
            "src\data_builder\video_cleaner.cpp"
        )
    }
)

foreach ($target in $targets) {
    Write-Host "Compiling $($target.Name)..."

    $arguments = @(
        "-std=c++17"
    )

    foreach ($source in $target.Sources) {
        $arguments += (Join-Path $repoRoot $source)
    }

    $arguments += @(
        "-o",
        (Join-Path $repoRoot $target.Output)
    )

    & $compilerCommand.Source @arguments

    if ($LASTEXITCODE -ne 0) {
        throw "Compilation failed for $($target.Name)"
    }
}

Write-Host "Build completed successfully."
