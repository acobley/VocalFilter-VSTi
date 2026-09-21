#!/bin/bash
#-----------------------------------------------------------------------------
# Every suite this project has, in one command, so "run the tests" before a
# release is not a list to remember.
#
#   tools/run-tests.sh
#
# Nothing here needs Xcode, cmake or a built plug-in: the two C++ suites
# compile straight from source/, and the rest are scripts. That is deliberate
# - a check you can only run after a 20-minute build is a check that gets
# skipped. The SDK headers are needed for the parameter suite, so it is
# skipped (loudly) if external/vst3sdk is not there yet.
#-----------------------------------------------------------------------------
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SDK="$ROOT/external/vst3sdk"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

CXX="${CXX:-c++}"
FAILED=0

run () {
    local what="$1"; shift
    echo
    echo "=== $what"
    if "$@"; then
        return 0
    fi
    echo "!!! $what FAILED"
    FAILED=$((FAILED + 1))
    return 1
}

#-----------------------------------------------------------------------------
# The DSP suite. SDK-FREE, and that is not an accident: it cannot see the
# parameter table at all, which is why the second suite exists.
#-----------------------------------------------------------------------------
if run "building the DSP suite" \
       "$CXX" -std=c++17 -O2 -I"$ROOT/source" \
              "$ROOT/tests/DspTests.cpp" "$ROOT/source/VocalFilterDsp.cpp" \
              -o "$WORK/dsptests"; then
    run "DspTests" "$WORK/dsptests"
fi

#-----------------------------------------------------------------------------
# The parameter suite. Needs the SDK's HEADERS but none of its code, so it
# links against one translation unit and builds in seconds.
#-----------------------------------------------------------------------------
if [ -d "$SDK" ]; then
    if run "building the parameter suite" \
           "$CXX" -std=c++17 -O2 -I"$SDK" -I"$ROOT/source" \
                  "$ROOT/tests/ParamsTests.cpp" "$ROOT/source/VocalFilterParams.cpp" \
                  -o "$WORK/paramstests"; then
        run "ParamsTests" "$WORK/paramstests"
    fi
else
    echo
    echo "=== ParamsTests SKIPPED - no SDK at external/vst3sdk"
    echo "    ./setup-xcode.sh fetches it. This suite is the one that caught the"
    echo "    Voice switch bug, so do not ship a release without running it."
    FAILED=$((FAILED + 1))
fi

#-----------------------------------------------------------------------------
# The editor invariant no runtime test can reach.
#-----------------------------------------------------------------------------
run "check-editor" python3 "$HERE/check-editor.py"

#-----------------------------------------------------------------------------
# The installer's own guards, against deliberately broken trees. Needs no Mac.
#-----------------------------------------------------------------------------
if [ -x "$ROOT/installer/test-guards.sh" ]; then
    run "installer guards" "$ROOT/installer/test-guards.sh"
elif [ -f "$ROOT/installer/test-guards.sh" ]; then
    # The execute bit does not survive a copy, a zip or an AirDrop, and a
    # script that will not run is not a check that passed.
    echo
    echo "=== installer/test-guards.sh is NOT EXECUTABLE - running it anyway"
    echo "    chmod +x installer/*.sh installer/scripts-au/postinstall"
    run "installer guards" bash "$ROOT/installer/test-guards.sh"
fi

#-----------------------------------------------------------------------------
# The release kit: the version and maker's name agree everywhere, and the
# release scripts' own refusals still refuse. None of this needs a Mac.
#-----------------------------------------------------------------------------
printf '\n===== check-versions\n'
python3 "$ROOT/tools/check-versions.py" || FAILED=$((FAILED + 1))
for t in test-build-release test-publish; do
	printf '\n===== installer/%s\n' "$t"
	if [ -x "$ROOT/installer/$t.sh" ]; then
		"$ROOT/installer/$t.sh" || FAILED=$((FAILED + 1))
	else
		# A script that will not run is not a check that passed.
		echo "NOT EXECUTABLE: installer/$t.sh - chmod +x installer/*.sh"
		FAILED=$((FAILED + 1))
	fi
done

echo
if [ "$FAILED" -eq 0 ]; then
    echo "All suites passed."
    exit 0
fi
echo "$FAILED step(s) FAILED."
exit 1
