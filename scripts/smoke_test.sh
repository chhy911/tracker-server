#!/usr/bin/env bash
# CI / 本地冒烟：需已运行的 tracker-server（API 默认 8081）
set -euo pipefail
API_PORT="${API_PORT:-8081}"
BASE="http://127.0.0.1:${API_PORT}"

curl -sf "${BASE}/api/health" | grep -q '"status":"ok"'
curl -sf "${BASE}/api/stats" | grep -q 'torrent_count'
curl -sf "${BASE}/api/metrics" | grep -q 'total_requests'
echo "smoke_test OK"
