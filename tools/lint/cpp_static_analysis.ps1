param(
    [switch]$CheckTools,
    [switch]$CppcheckOnly,
    [switch]$ClangTidyOnly
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..\..")
$SourceRoot = Join-Path $RepoRoot "sample\Plugins\UECommandForge\Source"

function Get-CommandPath {
    param([string]$Name)

    $Command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($Command) {
        return $Command.Source
    }
    return $null
}

function First-Executable {
    param([string[]]$Candidates)

    foreach ($Candidate in $Candidates) {
        if ($Candidate -and (Test-Path -LiteralPath $Candidate -PathType Leaf)) {
            return $Candidate
        }
    }
    return $null
}

function Find-ClangTidy {
    $Candidates = @(
        $env:CLANG_TIDY,
        (Get-CommandPath "clang-tidy.exe"),
        (Get-CommandPath "clang-tidy"),
        "C:\Program Files\LLVM\bin\clang-tidy.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-tidy.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin\clang-tidy.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin\clang-tidy.exe"
    )
    return First-Executable $Candidates
}

function Find-Cppcheck {
    $Candidates = @(
        $env:CPPCHECK,
        (Get-CommandPath "cppcheck.exe"),
        (Get-CommandPath "cppcheck"),
        "C:\Program Files\Cppcheck\cppcheck.exe"
    )
    return First-Executable $Candidates
}

$ClangTidy = Find-ClangTidy
$Cppcheck = Find-Cppcheck

if ($CheckTools) {
    Write-Output "platform: windows"
    Write-Output "clang-tidy: $(if ($ClangTidy) { $ClangTidy } else { 'not found' })"
    Write-Output "cppcheck: $(if ($Cppcheck) { $Cppcheck } else { 'not found' })"
    if (-not $ClangTidy -or -not $Cppcheck) {
        exit 1
    }
    exit 0
}

$CppFiles = Get-ChildItem -Path $SourceRoot -Recurse -File -Include *.cpp |
    Sort-Object FullName |
    ForEach-Object { $_.FullName }

if (-not $ClangTidyOnly) {
    if (-not $Cppcheck) {
        [Console]::Error.WriteLine("[lint] cppcheck not found. Install it or set CPPCHECK.")
        exit 127
    }
    & $Cppcheck `
        --enable=warning,style,performance,portability `
        --inline-suppr `
        --std=c++20 `
        --suppress=missingIncludeSystem `
        --suppress=unknownMacro `
        --suppress=syntaxError `
        @CppFiles
}

if (-not $CppcheckOnly) {
    if (-not $ClangTidy) {
        [Console]::Error.WriteLine("[lint] clang-tidy not found. Install LLVM or set CLANG_TIDY.")
        exit 127
    }

    $CompileCommandsDir = if ($env:COMPILE_COMMANDS_DIR) { $env:COMPILE_COMMANDS_DIR } else { $RepoRoot }
    $CompileCommands = Join-Path $CompileCommandsDir "compile_commands.json"
    if (-not (Test-Path -LiteralPath $CompileCommands -PathType Leaf)) {
        Write-Output "[lint] compile_commands.json not found in ${CompileCommandsDir}; skipping clang-tidy."
        Write-Output "[lint] Generate one with UnrealBuildTool or set COMPILE_COMMANDS_DIR."
        exit 0
    }

    & $ClangTidy -p $CompileCommandsDir @CppFiles
}
