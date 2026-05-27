param(
    [string]$OutputDir = $env:COMPILE_COMMANDS_DIR
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$ProjectFile = Join-Path $RepoRoot "sample\UECommandForgeSample.uproject"

$UeRoot = if ($env:UE_ROOT) { $env:UE_ROOT } else { "C:\Program Files\Epic Games\UE_5.7" }
$RunUbt = Join-Path $UeRoot "Engine\Build\BatchFiles\RunUBT.bat"
if (-not (Test-Path -LiteralPath $RunUbt -PathType Leaf)) {
    [Console]::Error.WriteLine("[lint] RunUBT.bat not found: ${RunUbt}")
    [Console]::Error.WriteLine("[lint] Set UE_ROOT to your Unreal Engine install path.")
    exit 5
}

if (-not $OutputDir) {
    $OutputDir = $RepoRoot
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

& $RunUbt `
    -Mode=GenerateClangDatabase `
    UnrealEditor Win64 Development `
    "-Project=${ProjectFile}" `
    "-OutputDir=${OutputDir}" `
    -OutputFilename=compile_commands.json `
    "-Include=${RepoRoot}\sample\Plugins\UECommandForge\Source\..."
