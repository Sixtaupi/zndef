#!/usr/bin/env sh

# Analyze the complete library with its public declarations visible.
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$project_dir"
mkdir -p build-misra

# Never reuse dumps or a MISRA report from an earlier invocation.
rm -f ./src/*.c.dump build-misra/misra_report.xml

cppcheck_status=0
cppcheck \
    --dump \
    --std=c99 \
    --enable=warning,style,performance,portability \
    --error-exitcode=1 \
    --suppress=misra-config \
    --xml --xml-version=2 \
    --max-configs=1 \
    -I include \
    -DCONFIG_ZNDEF_VALIDATE_RTD=1 \
    -DCONFIG_ZNDEF_RTD_TEXT=1 \
    -DCONFIG_ZNDEF_RTD_URI=1 \
    -DCONFIG_ZNDEF_RTD_SP=1 \
    -DCONFIG_ZNDEF_RTD_SIG=1 \
    -DCONFIG_ZNDEF_RTD_HANDOVER=1 \
    -DCONFIG_ZNDEF_RTD_DI=1 \
    -DCONFIG_ZNDEF_ITEMS=1 \
    -DCONFIG_ZNDEF_TLV=1 \
    ./src/*.c >build-misra/cppcheck.log 2>build-misra/cppcheck_report.xml || cppcheck_status=$?

cat build-misra/cppcheck.log
cat build-misra/cppcheck_report.xml >&2

# Include only dumps produced for the current source files, in a single report.
set --
for source in ./src/*.c; do
    if [ ! -f "$source.dump" ]; then
        echo "error: Cppcheck did not generate $source.dump" >&2
        exit 1
    fi
    set -- "$@" "$source.dump"
done

# Run MISRA even when Cppcheck found issues, so both reports are available.
misra_status=0
python3 "${MISRA_ADDON:-/usr/local/share/Cppcheck/addons/misra.py}" \
    --rule-texts=tools/docker/cppcheck/misra2023_rules_ascii.txt \
    --suppress-rules="$(cat tools/docker/cppcheck/misra-suppression.txt)" \
    --junit-file-export=build-misra/misra_report.xml \
    "$@" >build-misra/misra.log 2>&1 || misra_status=$?

cat build-misra/misra.log

if [ "$cppcheck_status" -ne 0 ] || [ "$misra_status" -ne 0 ]; then
    exit 1
fi
