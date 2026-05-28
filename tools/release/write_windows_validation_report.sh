#!/usr/bin/env bash
set -euo pipefail

OUTPUT=""

while [ $# -gt 0 ]; do
  case "$1" in
    --output)
      OUTPUT="$2"
      shift 2
      ;;
    *)
      echo "[write_windows_validation_report] Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if [ -z "${OUTPUT}" ]; then
  echo "Usage: $0 --output <report.json>" >&2
  exit 2
fi

mkdir -p "$(dirname "${OUTPUT}")"

jq -n \
  --arg generated_at "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
  '{
    generated_at: $generated_at,
    status: "blocked",
    reason: "windows_host_required",
    static_checks: [
      "tools/test/smoke/windows_command_wrappers.sh"
    ],
    required_windows_commands: [
      "install-uecommandforge.bat",
      "install-uecommandforge.ps1",
      "tools\\release\\package_plugin.bat",
      "tools\\release\\package_source.bat",
      "tools\\release\\package_tools.bat",
      "tools\\release\\verify_release_package.bat",
      "tools\\release\\install_local.bat",
      "tools\\release\\uninstall.bat",
      "tools\\release\\update_install.bat",
      "tools\\test\\smoke\\release_package_install.bat",
      "tools\\test\\smoke\\installer_install_update_uninstall.bat"
    ],
    limitation: "This host cannot execute Windows Command Prompt .bat wrappers. Run the listed commands on a Windows host before declaring Windows package wrapper validation complete."
  }' > "${OUTPUT}"

echo "${OUTPUT}"
