#!/usr/bin/env bash
# Windows command wrapper static smoke test.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"

require_file() {
    local path="$1"
    test -f "${REPO_ROOT}/${path}"
}

require_contains() {
    local path="$1"
    local pattern="$2"
    grep -q -- "${pattern}" "${REPO_ROOT}/${path}"
}

require_not_contains() {
    local path="$1"
    local pattern="$2"
    ! grep -q -- "${pattern}" "${REPO_ROOT}/${path}"
}

require_windows_bootstrap_contains() {
    local expected_status="$1"
    local expected_pattern="$2"
    shift 2

    local output
    output="$(mktemp)"
    local command='$env:Path = "C:\Windows\System32"; $env:ProgramFiles = "Z:\uecf-missing"; $env:LocalAppData = "Z:\uecf-missing"; $env:UECF_AUTO_INSTALL_DEPS = $null; $env:UECF_SKIP_DEP_INSTALL = $null;'
    local assignment
    for assignment in "$@"; do
        command="${command} \$env:${assignment%%=*} = \"${assignment#*=}\";"
    done
    command="${command} cmd /c \"call tools\\windows\\bootstrap_dependencies.bat jq\"; exit \$LASTEXITCODE"

    set +e
    (
        cd "${REPO_ROOT}"
        "${POWERSHELL_EXE}" -NoProfile -Command "${command}"
    ) >"${output}" 2>&1
    local status=$?
    set -e

    test "${status}" -eq "${expected_status}"
    grep -q -- "${expected_pattern}" "${output}"
    rm -f "${output}"
}

require_windows_command_succeeds() {
    local expected_pattern="$1"
    shift

    local output
    output="$(mktemp)"
    set +e
    (
        cd "${REPO_ROOT}"
        "$@"
    ) >"${output}" 2>&1
    local status=$?
    set -e

    test "${status}" -eq 0
    grep -q -- "${expected_pattern}" "${output}"
    rm -f "${output}"
}

require_command_fails_with() {
    local expected_status="$1"
    local expected_pattern="$2"
    shift 2

    local output
    output="$(mktemp)"
    set +e
    (
        cd "${REPO_ROOT}"
        "$@"
    ) >"${output}" 2>&1
    local status=$?
    set -e

    test "${status}" -eq "${expected_status}"
    grep -q -- "${expected_pattern}" "${output}"
    rm -f "${output}"
}

for wrapper in \
    install-uecommandforge.bat \
    tools/lint/cpp_static_analysis.bat \
    tools/ue/ue_env.bat \
    tools/ue/run_commandlet.bat \
    tools/ue/hello.bat \
    tools/ue/snapshot_assets.bat \
    tools/ue/validate_asset_rules.bat \
    tools/ue/plan_asset_changes.bat \
    tools/ue/apply_asset_changes.bat \
    tools/ue/rollback_asset_changes.bat \
    tools/ue/create_project_folders.bat \
    tools/ue/generate_cpp_class.bat \
    tools/ue/validate_cpp_reflection.bat \
    tools/ue/validate_buildcs.bat \
    tools/ue/analyze_uht_log.bat \
    tools/ue/create_character_bp.bat \
    tools/ue/create_ai_controller.bat \
    tools/ue/compile_blueprints.bat \
    tools/ue/create_statetree.bat \
    tools/ue/bind_ai_flow.bat \
    tools/ue/validate_ai_flow.bat \
    tools/ue/validate_data_source.bat \
    tools/ue/import_data_source.bat \
    tools/ue/validate_datatable.bat \
    tools/ue/validate_config_rules.bat \
    tools/ue/validate_project_rules.bat \
    tools/ue/place_actor.bat \
    tools/ue/create_ai_flow.bat \
    tools/ue/setup_npc_character.bat \
    tools/ue/setup_patrol_ai.bat \
    tools/release/package_plugin.bat \
    tools/release/package_source.bat \
    tools/release/package_tools.bat \
    tools/release/install_into_sample_project.bat \
    tools/release/install_local.bat \
    tools/release/uninstall.bat \
    tools/release/update_install.bat \
    tools/release/verify_release_package.bat \
    tools/release/write_checksums.bat \
    tools/release/write_manifest.bat \
    tools/test/automation/run.bat \
    tools/test/smoke/installer_install_update_uninstall.bat \
    tools/test/smoke/release_package_install.bat \
    tools/test/smoke/prototype_automation.bat \
    tools/test/smoke/create_ai_flow.bat
do
    require_file "${wrapper}"
    require_contains "${wrapper}" '@echo off'
done

require_file tools/windows/bootstrap_dependencies.bat
require_contains tools/windows/bootstrap_dependencies.bat 'Git.Git'
require_contains tools/windows/bootstrap_dependencies.bat 'jqlang.jq'
require_contains tools/windows/bootstrap_dependencies.bat 'LLVM.LLVM'
require_contains tools/windows/bootstrap_dependencies.bat 'Cppcheck.Cppcheck'
require_contains tools/windows/bootstrap_dependencies.bat 'UECF_SKIP_DEP_INSTALL'
require_contains tools/windows/bootstrap_dependencies.bat 'UECF_AUTO_INSTALL_DEPS'
require_contains tools/windows/bootstrap_dependencies.bat 'UECF_AUTO_INSTALL_DEPS=0'
require_contains tools/windows/bootstrap_dependencies.bat '--source winget'
require_contains tools/windows/bootstrap_dependencies.bat '--disable-interactivity'
require_contains tools/windows/bootstrap_dependencies.bat 'findstr.exe'
require_not_contains tools/windows/bootstrap_dependencies.bat '| find /i'
require_contains tools/windows/bootstrap_dependencies.bat 'cppcheck'
require_contains tools/windows/bootstrap_dependencies.bat 'clang-tidy'

require_file install-uecommandforge.ps1
require_contains install-uecommandforge.sh 'package_plugin.sh'
require_contains install-uecommandforge.sh 'package_tools.sh'
require_contains install-uecommandforge.sh 'UECF_INSTALL_SKIP_PLUGIN_BUILD'
require_command_fails_with 2 'Missing required option: --project' \
    ./install-uecommandforge.sh
require_contains install-uecommandforge.bat 'install-uecommandforge.sh'
require_contains install-uecommandforge.bat '-l'
require_contains install-uecommandforge.bat 'bootstrap_dependencies.bat'
require_contains install-uecommandforge.bat 'UECF_BASH_EXE'
require_contains install-uecommandforge.ps1 'Git\\bin\\bash.exe'
require_contains install-uecommandforge.ps1 'Microsoft\\WindowsApps\\bash.exe'
require_contains install-uecommandforge.ps1 'install-uecommandforge.sh'
require_contains tools/lint/cpp_static_analysis.bat 'bootstrap_dependencies.bat'
require_contains tools/lint/cpp_static_analysis.bat 'cppcheck'
require_contains tools/lint/cpp_static_analysis.bat 'clang-tidy'
require_contains tools/ue/ue_env.bat 'UnrealEditor-Cmd.exe'
require_contains tools/ue/ue_env.bat 'UECF_PROJECT_FILE'
require_contains tools/ue/run_commandlet.bat 'Saved\\CodexReports'
require_contains tools/ue/run_commandlet.bat 'UECF_REPORT_DIR'
require_contains tools/ue/run_commandlet.bat '%%~dpI'
require_contains tools/ue/run_commandlet.bat '-AssetPaths'
require_contains tools/ue/run_commandlet.bat '-AssetRootPaths'
require_contains tools/ue/run_commandlet.bat '-Markdown'
require_contains tools/ue/run_commandlet.bat '-Format'
require_contains tools/ue/run_commandlet.bat '-ApplyBuildCs'
require_contains tools/ue/run_commandlet.bat '-nullrhi'
require_contains tools/ue/run_commandlet.bat 'DisableDelayedExpansion'
require_contains tools/ue/run_commandlet.bat 'Unsafe cmd metacharacter'
require_contains tools/ue/run_commandlet.bat 'Unsafe cmd metacharacter in commandlet name'
require_contains tools/ue/run_commandlet.bat 'Unreal Python access is not allowed'
require_contains tools/ue/run_commandlet.bat 'executepythonscript'
require_contains tools/ue/run_commandlet.bat 'executepythoncommand'
require_contains tools/ue/run_commandlet.bat 'pythonscriptplugin'
require_contains tools/ue/run_commandlet.bat 'import\\s+unreal'
require_contains tools/ue/run_commandlet.bat 'unreal.'
require_contains tools/ue/run_commandlet.bat 'executepythonscript|executepythoncommand|pythonscriptplugin'
require_contains tools/ue/run_commandlet.bat '-replace'
require_contains tools/ue/run_commandlet.bat '(^| )py($| )'
require_contains tools/ue/run_commandlet.bat 'COMBINED_ARG:&='
require_not_contains tools/ue/validate_asset_rules.bat '!EXTRA_ARGS!'
require_not_contains tools/ue/create_project_folders.bat '!EXTRA_ARGS!'
require_contains tools/ue/hello.bat 'DisableDelayedExpansion'
require_contains tools/ue/snapshot_assets.bat 'DisableDelayedExpansion'
require_contains tools/ue/compile_blueprints.bat 'DisableDelayedExpansion'
require_contains tools/ue/snapshot_assets.bat 'AssetSnapshot'
require_contains tools/ue/validate_asset_rules.bat 'ValidateAssetRules'
require_contains tools/ue/plan_asset_changes.bat 'PlanAssetChanges'
require_contains tools/ue/plan_asset_changes.bat 'PLAN_PATH=%~f1'
require_contains tools/ue/apply_asset_changes.bat 'ApplyAssetChanges'
require_contains tools/ue/apply_asset_changes.bat 'PLAN_PATH=%~f1'
require_contains tools/ue/rollback_asset_changes.bat 'RollbackAssetChanges'
require_contains tools/ue/rollback_asset_changes.bat 'ROLLBACK_PATH=%~f1'
require_contains tools/ue/create_project_folders.bat 'CreateProjectFolders'
require_contains tools/ue/generate_cpp_class.bat 'GenerateCppClass'
require_contains tools/ue/validate_cpp_reflection.bat 'ValidateCppReflection'
require_contains tools/ue/validate_buildcs.bat 'ValidateBuildCs'
require_contains tools/ue/analyze_uht_log.bat 'AnalyzeUhtLog'
require_contains tools/ue/create_ai_flow.bat 'CreateAIFlow'
require_contains tools/ue/validate_data_source.bat 'ValidateDataSource'
require_contains tools/ue/import_data_source.bat 'ImportDataSource'
require_contains tools/ue/validate_datatable.bat 'ValidateDataTable'
require_contains tools/ue/validate_config_rules.bat 'ValidateConfigRules'
require_contains tools/ue/validate_project_rules.bat 'PrototypeAutomation'
require_contains tools/ue/setup_npc_character.bat 'specs\\profiles\\npc_character.json'
require_contains tools/ue/setup_patrol_ai.bat 'specs\\profiles\\patrol_ai.json'
require_contains tools/release/package_plugin.bat 'package_plugin.sh'
require_contains tools/release/package_plugin.bat 'bootstrap_dependencies.bat'
require_contains tools/release/package_source.bat 'package_source.sh'
require_contains tools/release/package_source.bat 'bootstrap_dependencies.bat'
require_contains tools/release/package_tools.bat 'package_tools.sh'
require_contains tools/release/package_tools.bat 'bootstrap_dependencies.bat'
require_contains tools/release/install_into_sample_project.bat 'install_into_sample_project.sh'
require_contains tools/release/install_into_sample_project.bat 'bootstrap_dependencies.bat'
require_contains tools/release/install_local.bat 'install_local.sh'
require_contains tools/release/install_local.bat 'bootstrap_dependencies.bat'
require_contains tools/release/uninstall.bat 'uninstall.sh'
require_contains tools/release/uninstall.bat 'bootstrap_dependencies.bat'
require_contains tools/release/update_install.bat 'update_install.sh'
require_contains tools/release/update_install.bat 'bootstrap_dependencies.bat'
require_contains tools/release/verify_release_package.bat 'verify_release_package.sh'
require_contains tools/release/verify_release_package.bat 'bootstrap_dependencies.bat'
require_contains tools/release/write_checksums.bat 'write_checksums.sh'
require_contains tools/release/write_checksums.bat 'bootstrap_dependencies.bat'
require_contains tools/release/write_manifest.bat 'write_manifest.sh'
require_contains tools/release/write_manifest.bat 'bootstrap_dependencies.bat'
require_contains tools/test/automation/run.bat 'Automation RunTests UECommandForge; Quit'
require_contains tools/test/automation/run.bat 'bootstrap_dependencies.bat'
require_contains tools/test/automation/run.bat 'jq'
require_contains tools/test/smoke/installer_install_update_uninstall.bat 'installer_install_update_uninstall.sh'
require_contains tools/test/smoke/installer_install_update_uninstall.bat 'bootstrap_dependencies.bat'
require_contains tools/test/smoke/release_package_install.bat 'release_package_install.sh'
require_contains tools/test/smoke/release_package_install.bat 'bootstrap_dependencies.bat'
require_contains tools/test/smoke/prototype_automation.bat 'prototype_automation.sh'
require_contains tools/test/smoke/prototype_automation.bat 'bootstrap_dependencies.bat'
require_contains tools/test/smoke/create_ai_flow.bat 'created_assets'
require_contains tools/test/smoke/create_ai_flow.bat 'bootstrap_dependencies.bat'
require_contains tools/test/smoke/create_ai_flow.bat 'jq'
require_contains tools/test/smoke/create_ai_flow.bat 'Missing asset'

if command -v powershell.exe >/dev/null 2>&1; then
    POWERSHELL_EXE="$(command -v powershell.exe)"
elif command -v pwsh.exe >/dev/null 2>&1; then
    POWERSHELL_EXE="$(command -v pwsh.exe)"
else
    POWERSHELL_EXE=""
fi

if [[ -n "${POWERSHELL_EXE}" ]]; then
    require_windows_bootstrap_contains 2 'winget not found'
    require_windows_bootstrap_contains 2 'UECF_AUTO_INSTALL_DEPS=0 is set' UECF_AUTO_INSTALL_DEPS=0
    require_windows_bootstrap_contains 2 'UECF_SKIP_DEP_INSTALL=1 is set' UECF_SKIP_DEP_INSTALL=1
fi

if command -v cmd.exe >/dev/null 2>&1; then
    require_windows_command_succeeds 'Usage:' cmd.exe /c install-uecommandforge.bat --help
fi

if [[ -n "${POWERSHELL_EXE}" ]]; then
    require_windows_command_succeeds 'Usage:' "${POWERSHELL_EXE}" -NoProfile -ExecutionPolicy Bypass -File install-uecommandforge.ps1 --help
fi
