#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/ue_env.sh"

ENGINE_DIR="$(dirname "$(dirname "$(dirname "${UNREAL_EDITOR_CMD}")")")"
RUN_UAT="${ENGINE_DIR}/Build/BatchFiles/RunUAT.sh"
case "${UE_PLATFORM}" in
  Win64) RUN_UAT="${ENGINE_DIR}/Build/BatchFiles/RunUAT.bat" ;;
esac

PLUGIN="${REPO_ROOT}/sample/Plugins/UECommandForge/UECommandForge.uplugin"
OUT="${REPO_ROOT}/sample/Saved/PluginBuild"
mkdir -p "${OUT}"

exec "${RUN_UAT}" BuildPlugin \
  -Plugin="${PLUGIN}" \
  -Package="${OUT}" \
  -TargetPlatforms="${UE_PLATFORM}" \
  -Rocket
