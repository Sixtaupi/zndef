#!/usr/bin/env bash
# Copyright (c) 2026 Romain Pelletant
# SPDX-License-Identifier: Apache-2.0

# Keep build artifacts outside the source tree. Twister must run in a west workspace.
set -euo pipefail

project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output_dir=${1:-"${TMPDIR:-/tmp}/zndef-conformance"}
if (( $# > 0 )); then
    shift
fi

snapshot_file=$(mktemp "${TMPDIR:-/tmp}/zndef-inputs.XXXXXX.json")
trap 'rm -f -- "$snapshot_file"' EXIT
python3 "$project_dir/tests/scripts/build_evidence.py" --capture-inputs "$snapshot_file"

west twister \
    -T "$project_dir/tests" -T "$project_dir/samples" \
    -p qemu_x86 -p qemu_cortex_m3 \
    --inline-logs -O "$output_dir" "$@"

python3 "$project_dir/tests/scripts/build_evidence.py" \
    --report "$output_dir/twister.json" \
    --snapshot "$snapshot_file" \
    --output "$output_dir/conformance-evidence.json"

printf 'Evidence: %s\n' "$output_dir/conformance-evidence.json"
