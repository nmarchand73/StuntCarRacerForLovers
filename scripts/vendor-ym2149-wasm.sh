#!/usr/bin/env bash
# Vendor ym2149-wasm into package/ym2149 for local web builds and CI.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${ROOT}/package/ym2149"
VERSION="${YM2149_WASM_VERSION:-0.8.1}"
PACK="/tmp/ym2149-wasm-${VERSION}.tgz"

mkdir -p "${DEST}"
npm pack "ym2149-wasm@${VERSION}" --pack-destination /tmp
tar -xzf "${PACK}" -C /tmp
cp /tmp/package/ym2149_wasm.js /tmp/package/ym2149_wasm_bg.wasm "${DEST}/"
echo "Vendored ym2149-wasm ${VERSION} -> ${DEST}"
