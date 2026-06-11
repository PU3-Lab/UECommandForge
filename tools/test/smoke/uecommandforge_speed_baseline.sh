#!/usr/bin/env bash
set -euo pipefail

AGENT_HOME_VAL="${AGENT_HOME:-${CODEX_HOME:-$HOME/.codex}}"
CODEX_UECF="${AGENT_HOME_VAL}/UECommandForge"

# 만약 전역 설치 경로에 없으면 현재 개발 리포지토리의 로컬 경로를 대체하여 사용할 수 있게 처리
if [ ! -d "${CODEX_UECF}" ]; then
  REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
  CODEX_UECF="${REPO_ROOT}"
fi

echo "[speed-baseline] installed tools: ${CODEX_UECF}/tools/ue"
test -f "${CODEX_UECF}/tools/ue/bench/baseline_npc_setup.sh"
test -f "${CODEX_UECF}/tools/ue/bench/batch_npc_setup.sh"

echo "[speed-baseline] baseline fixture exists"
echo "[speed-baseline] batch fixture exists"
echo "[speed-baseline] bench scripts are timing fixtures, not production asset generators"
