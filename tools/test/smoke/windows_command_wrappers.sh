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
    grep -q "${pattern}" "${REPO_ROOT}/${path}"
}

for wrapper in \
    install-uecommandforge.bat \
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

require_file install-uecommandforge.ps1
require_contains install-uecommandforge.bat 'install-uecommandforge.sh'
require_contains install-uecommandforge.ps1 'install-uecommandforge.sh'
require_contains tools/ue/ue_env.bat 'UnrealEditor-Cmd.exe'
require_contains tools/ue/ue_env.bat 'UECF_PROJECT_FILE'
require_contains tools/ue/run_commandlet.bat 'Saved\\CodexReports'
require_contains tools/ue/run_commandlet.bat 'UECF_REPORT_DIR'
require_contains tools/ue/run_commandlet.bat '%%~dpI'
require_contains tools/ue/snapshot_assets.bat 'AssetSnapshot'
require_contains tools/ue/validate_asset_rules.bat 'ValidateAssetRules'
require_contains tools/ue/plan_asset_changes.bat 'PlanAssetChanges'
require_contains tools/ue/apply_asset_changes.bat 'ApplyAssetChanges'
require_contains tools/ue/rollback_asset_changes.bat 'RollbackAssetChanges'
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
require_contains tools/release/package_source.bat 'package_source.sh'
require_contains tools/release/package_tools.bat 'package_tools.sh'
require_contains tools/release/install_into_sample_project.bat 'install_into_sample_project.sh'
require_contains tools/release/install_local.bat 'install_local.sh'
require_contains tools/release/uninstall.bat 'uninstall.sh'
require_contains tools/release/update_install.bat 'update_install.sh'
require_contains tools/release/verify_release_package.bat 'verify_release_package.sh'
require_contains tools/release/write_checksums.bat 'write_checksums.sh'
require_contains tools/release/write_manifest.bat 'write_manifest.sh'
require_contains tools/test/automation/run.bat 'Automation RunTests UECommandForge; Quit'
require_contains tools/test/smoke/installer_install_update_uninstall.bat 'installer_install_update_uninstall.sh'
require_contains tools/test/smoke/release_package_install.bat 'release_package_install.sh'
require_contains tools/test/smoke/prototype_automation.bat 'prototype_automation.sh'
require_contains tools/test/smoke/create_ai_flow.bat 'created_assets'
require_contains tools/test/smoke/create_ai_flow.bat 'Missing asset'
