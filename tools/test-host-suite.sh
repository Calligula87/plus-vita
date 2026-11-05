#!/bin/sh
# All host journeys, no Vita package creation or network access beyond fixtures.
set -eu
cd "$(dirname "$0")/.."
for journey in tools/test-*.sh; do
 [ "$journey" != tools/test-host-suite.sh ] || continue
 printf '\nRunning %s\n' "$journey"
 sh "$journey"
done
python3 tools/prepare-menu-audio.py --check
if [ -n "${PLUS_VITA_TEST_VPK:-}" ]; then
 python3 tools/test-publication-gate.py "$PLUS_VITA_TEST_VPK"
else
 printf '\nPackage-gate fixtures skipped: set PLUS_VITA_TEST_VPK to an authorized candidate VPK.\n'
fi
printf '\nAll host journeys passed. Vita acceptance remains pending.\n'
