#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../ue/ue_env.sh"

ENGINE_DIR="$(dirname "$(dirname "$(dirname "${UNREAL_EDITOR_CMD}")")")"
RUN_UBT="${ENGINE_DIR}/Build/BatchFiles/RunUBT.sh"
case "${UE_PLATFORM}" in
  Win64) RUN_UBT="${ENGINE_DIR}/Build/BatchFiles/RunUBT.bat" ;;
esac

OUTPUT_DIR="${COMPILE_COMMANDS_DIR:-${REPO_ROOT}}"
mkdir -p "${OUTPUT_DIR}"

exec "${RUN_UBT}" \
  -Mode=GenerateClangDatabase \
  UnrealEditor "${UE_PLATFORM}" Development \
  -Project="${PROJECT_FILE}" \
  -OutputDir="${OUTPUT_DIR}" \
  -OutputFilename=compile_commands.json \
  -Include="${REPO_ROOT}/sample/Plugins/UECommandForge/Source/..."
