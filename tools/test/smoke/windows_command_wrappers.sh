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
    tools/ue/ue_env.bat \
    tools/ue/run_commandlet.bat \
    tools/ue/hello.bat \
    tools/ue/snapshot_assets.bat \
    tools/ue/validate_asset_rules.bat \
    tools/ue/plan_asset_changes.bat \
    tools/ue/apply_asset_changes.bat \
    tools/ue/rollback_asset_changes.bat \
    tools/ue/create_project_folders.bat \
    tools/ue/create_character_bp.bat \
    tools/ue/create_ai_controller.bat \
    tools/ue/compile_blueprints.bat \
    tools/ue/create_statetree.bat \
    tools/ue/bind_ai_flow.bat \
    tools/ue/validate_ai_flow.bat \
    tools/ue/place_actor.bat \
    tools/ue/create_ai_flow.bat \
    tools/ue/setup_npc_character.bat \
    tools/ue/setup_patrol_ai.bat \
    tools/test/automation/run.bat \
    tools/test/smoke/create_ai_flow.bat
do
    require_file "${wrapper}"
    require_contains "${wrapper}" '@echo off'
done

require_contains tools/ue/ue_env.bat 'UnrealEditor-Cmd.exe'
require_contains tools/ue/run_commandlet.bat 'Saved\\CodexReports'
require_contains tools/ue/snapshot_assets.bat 'AssetSnapshot'
require_contains tools/ue/validate_asset_rules.bat 'ValidateAssetRules'
require_contains tools/ue/plan_asset_changes.bat 'PlanAssetChanges'
require_contains tools/ue/apply_asset_changes.bat 'ApplyAssetChanges'
require_contains tools/ue/rollback_asset_changes.bat 'RollbackAssetChanges'
require_contains tools/ue/create_project_folders.bat 'CreateProjectFolders'
require_contains tools/ue/create_ai_flow.bat 'CreateAIFlow'
require_contains tools/ue/setup_npc_character.bat 'specs\\profiles\\npc_character.json'
require_contains tools/ue/setup_patrol_ai.bat 'specs\\profiles\\patrol_ai.json'
require_contains tools/test/automation/run.bat 'Automation RunTests UECommandForge; Quit'
require_contains tools/test/smoke/create_ai_flow.bat 'created_assets'
require_contains tools/test/smoke/create_ai_flow.bat 'Missing asset'
