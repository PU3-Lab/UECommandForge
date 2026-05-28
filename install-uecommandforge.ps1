param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$InstallArgs
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Installer = Join-Path $ScriptDir "install-uecommandforge.sh"

if (-not (Get-Command bash -ErrorAction SilentlyContinue)) {
  Write-Error "[install-uecommandforge] bash not found. Install Git for Windows or run install-uecommandforge.sh from Git Bash."
}

& bash $Installer @InstallArgs
exit $LASTEXITCODE
