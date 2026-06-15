#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ "$#" -lt 1 ]; then
  echo "사용법: $0 -AssetPolicy=<policy.json> [-BuildCsPolicy=<policy.json>] [-DataSchema=<schema.json> -DataSource=<source.csv|json>] [-ConfigRules=<schema.json> [-Config=<config.ini>]] [-PluginPolicy=<policy.json>] [-DiffPlatforms=<csv> [-DiffAllowlist=<allowlist.json>] [-DiffCategories=<csv>]] [extra args...]" >&2
  exit 2
fi

exec "${SCRIPT_DIR}/run_commandlet.sh" PrototypeAutomation "$@"
