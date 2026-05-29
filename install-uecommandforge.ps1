param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$InstallArgs
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Installer = Join-Path $ScriptDir "install-uecommandforge.sh"

$BashCandidates = @()
if ($env:UECF_BASH_EXE) {
  $BashCandidates += $env:UECF_BASH_EXE
}
$BashCandidates += @(
  (Join-Path $env:ProgramFiles "Git\bin\bash.exe"),
  (Join-Path $env:LocalAppData "Programs\Git\bin\bash.exe")
)
$BashCandidates += Get-Command bash.exe -All -ErrorAction SilentlyContinue |
  Where-Object { $_.Source -notlike "*\Microsoft\WindowsApps\bash.exe" } |
  ForEach-Object { $_.Source }

$Bash = $BashCandidates |
  Where-Object { $_ -and (Test-Path $_) } |
  Select-Object -First 1

if (-not $Bash) {
  Write-Error "[install-uecommandforge] Git Bash not found. Install Git for Windows or run install-uecommandforge.sh from Git Bash."
}

$GitRoot = Split-Path -Parent (Split-Path -Parent $Bash)
$GitUsrBin = Join-Path $GitRoot "usr\bin"
$GitBin = Join-Path $GitRoot "bin"
$PathParts = @($GitUsrBin, $GitBin) + ($env:Path -split ';')
$env:Path = ($PathParts | Where-Object { $_ } | Select-Object -Unique) -join ';'
$env:CHERE_INVOKING = "1"

& $Bash -l $Installer @InstallArgs
exit $LASTEXITCODE
