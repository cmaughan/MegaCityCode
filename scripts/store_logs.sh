#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR/.."
LOG_FILE="${1:-$ROOT/logs/mac-log.txt}"

mkdir -p "$(dirname "$LOG_FILE")"
echo "Running megacitycode, logging to $LOG_FILE ..."
"$ROOT/build/megacitycode" >"$LOG_FILE" 2>&1
echo "Done. Log saved to $LOG_FILE"
