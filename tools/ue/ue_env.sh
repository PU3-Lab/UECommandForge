#!/usr/bin/env bash
# 현재 OS에 맞는 UNREAL_EDITOR_CMD와 PROJECT_FILE을 결정한다.
# 이 파일은 source로 불러올 것. 직접 실행 금지.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
if [ -n "${UECF_PROJECT_FILE:-}" ]; then
  PROJECT_FILE="${UECF_PROJECT_FILE}"
elif [ -n "${PROJECT_FILE:-}" ]; then
  PROJECT_FILE="${PROJECT_FILE}"
else
  PROJECT_FILE="${REPO_ROOT}/sample/UECommandForgeSample.uproject"
fi

resolve_ue_cmd() {
  if [ -n "${UNREAL_EDITOR_CMD:-}" ]; then
    return 0
  fi

  local ue_root="${UE_ROOT:-}"
  local platform
  case "$(uname -s)" in
    Darwin)
      ue_root="${ue_root:-/Users/Shared/Epic Games/UE_5.7}"
      UNREAL_EDITOR_CMD="${ue_root}/Engine/Binaries/Mac/UnrealEditor-Cmd"
      platform="Mac"
      ;;
    MINGW*|MSYS*|CYGWIN*)
      ue_root="${ue_root:-/c/Program Files/Epic Games/UE_5.7}"
      UNREAL_EDITOR_CMD="${ue_root}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      platform="Win64"
      ;;
    Linux)
      ue_root="${ue_root:-/opt/UnrealEngine/UE_5.7}"
      UNREAL_EDITOR_CMD="${ue_root}/Engine/Binaries/Linux/UnrealEditor-Cmd"
      platform="Linux"
      ;;
    *)
      echo "[ue_env] 지원하지 않는 OS: $(uname -s)" >&2
      return 5
      ;;
  esac

  if [ ! -x "${UNREAL_EDITOR_CMD}" ]; then
    echo "[ue_env] UnrealEditor-Cmd를 찾을 수 없습니다: ${UNREAL_EDITOR_CMD}" >&2
    echo "[ue_env] UE_ROOT 또는 UNREAL_EDITOR_CMD 환경변수로 재정의하세요." >&2
    return 5
  fi

  export UNREAL_EDITOR_CMD PROJECT_FILE
  export UE_PLATFORM="${platform}"
}

resolve_ue_cmd
