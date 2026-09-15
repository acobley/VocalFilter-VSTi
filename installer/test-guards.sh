#!/bin/bash
#-----------------------------------------------------------------------------
# Run the two guards in guards.sh against trees that are deliberately broken,
# and against ones that are not.
#
# Reading a guard is not testing it. Both of these were written wrong the
# first time in the plug-in this installer was lifted from, and in both cases
# the wrong version PASSED everything - a scan that matches nothing reports
# that nothing is wrong. So each case below asserts the verdict, not just that
# the function ran.
#
# Runs anywhere bash, find and sed do: it needs no Mac, no build and no SDK,
# which means it can run before the .pkg exists and on the machine that never
# builds one.
#
#   installer/test-guards.sh
#-----------------------------------------------------------------------------
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$HERE/guards.sh"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

PASSED=0; FAILED=0

# expect <accept|reject> <description> -- <command...>
expect () {
    local want="$1" what="$2"; shift 3
    local out status
    out="$("$@" 2>&1)"; status=$?

    local got="accept"; [ "$status" -eq 0 ] || got="reject"

    if [ "$got" = "$want" ]; then
        printf '  ok      %s\n' "$what"
        PASSED=$((PASSED + 1))
    else
        printf '  FAIL    %s\n' "$what"
        printf '          wanted the guard to %s, it did %s\n' "$want" "$got"
        [ -n "$out" ] && printf '          it said: %s\n' "$(printf '%s' "$out" | head -1)"
        FAILED=$((FAILED + 1))
    fi
}

echo "guards.sh, against trees built to break it"
echo

#-----------------------------------------------------------------------------
echo "no_absolute_symlinks"
#-----------------------------------------------------------------------------
mkdir -p "$TMP/clean/VocalFilter.component/Contents/Resources/plugin.vst3/Contents/MacOS"
touch "$TMP/clean/VocalFilter.component/Contents/Resources/plugin.vst3/Contents/MacOS/VocalFilter"
expect accept "a payload with no symlinks at all" \
       -- no_absolute_symlinks "$TMP/clean"

# A RELATIVE link is fine: it travels with the bundle. Worth a case of its own,
# because a guard written as "reject every symlink" passes the broken-tree test
# below and then refuses payloads that are correct - the SDK puts relative
# links inside a versioned bundle as a matter of course.
cp -R "$TMP/clean" "$TMP/relative"
ln -s "Contents/Resources/plugin.vst3" "$TMP/relative/VocalFilter.component/here"
expect accept "a relative symlink inside the bundle" \
       -- no_absolute_symlinks "$TMP/relative"

# THE ONE THAT SHIPPED A DEAD AUDIO UNIT: exactly what CMake leaves behind.
cp -R "$TMP/clean" "$TMP/broken"
rm -rf "$TMP/broken/VocalFilter.component/Contents/Resources/plugin.vst3"
ln -s "/Users/andy/DXi-DEv/VocalFilter-VSTi/build/VST3/Release/VocalFilter.vst3" \
      "$TMP/broken/VocalFilter.component/Contents/Resources/plugin.vst3"
expect reject "the CMake build-tree symlink at plugin.vst3" \
       -- no_absolute_symlinks "$TMP/broken"

# Buried, and to a path that exists on this machine - so nothing about it
# looks wrong locally. This is the two-years-from-now case the guard is for.
cp -R "$TMP/clean" "$TMP/buried"
mkdir -p "$TMP/buried/VocalFilter.component/Contents/Resources/deep/deeper"
ln -s "/tmp" "$TMP/buried/VocalFilter.component/Contents/Resources/deep/deeper/scratch"
expect reject "an absolute symlink buried three levels down" \
       -- no_absolute_symlinks "$TMP/buried"

expect accept "two roots at once, both clean" \
       -- no_absolute_symlinks "$TMP/clean" "$TMP/relative"
expect reject "two roots at once, one of them broken" \
       -- no_absolute_symlinks "$TMP/clean" "$TMP/broken"

#-----------------------------------------------------------------------------
echo
echo "pkg_refs_resolve"
#-----------------------------------------------------------------------------
# pkg-ref written ACROSS LINES, as productbuild writes it. A line-based scan
# finds nothing here and then reports nothing missing.
write_distribution () {
    local dir="$1"; shift
    mkdir -p "$dir"
    { echo '<?xml version="1.0" encoding="utf-8"?>'
      echo '<installer-gui-script minSpecVersion="2">'
      for ref in "$@"; do
          printf '    <pkg-ref id="audio.vocalfilter.%s"\n' "${ref%.pkg}"
          printf '             version="1.0.0.1"\n'
          printf '             onConclusion="none">%s</pkg-ref>\n' "$ref"
      done
      echo '</installer-gui-script>'
    } > "$dir/Distribution"
}

write_distribution "$TMP/dist-ok" VocalFilter-VST3.pkg VocalFilter-AU.pkg
touch "$TMP/dist-ok/VocalFilter-VST3.pkg" "$TMP/dist-ok/VocalFilter-AU.pkg"
expect accept "both pkg-refs present, written across lines" \
       -- pkg_refs_resolve "$TMP/dist-ok"

write_distribution "$TMP/dist-missing" VocalFilter-VST3.pkg VocalFilter-AU.pkg
touch "$TMP/dist-missing/VocalFilter-VST3.pkg"
expect reject "the AU package named but not embedded" \
       -- pkg_refs_resolve "$TMP/dist-missing"

# A CHECK WITH NOTHING TO CHECK IS BROKEN, NOT SATISFIED - and under
# `set -euo pipefail` this is also where an unguarded grep kills the script
# before it can say so.
write_distribution "$TMP/dist-empty"
expect reject "a Distribution with no pkg-ref at all" \
       -- pkg_refs_resolve "$TMP/dist-empty"

mkdir -p "$TMP/dist-none"
expect reject "an archive with no Distribution file" \
       -- pkg_refs_resolve "$TMP/dist-none"

#-----------------------------------------------------------------------------
echo
echo "identity_present"
#-----------------------------------------------------------------------------
# A FAKE `security find-identity`, so the whole matrix can be exercised on a
# machine with no certificates at all - including this Linux one. The listings
# below are the real format, down to the numbering and the trailing count.
CODESIGNING='  1) 1F2E3D4C5B6A79889706F5E4D3C2B1A099887766 "Apple Development: A. E. Cobley (9ZZ9ZZ9ZZ9)"
  2) A1B2C3D4E5F60718293A4B5C6D7E8F9012345678 "Developer ID Application: A. E. Cobley (ABCDE12345)"
     2 valid identities found'

# The INSTALLER certificate is only in the unfiltered list. A check that asks
# the -p codesigning list about it reports a certificate you have as missing,
# which is the whole reason the two calls in build-installer.sh differ.
ALL="$CODESIGNING
  3) 99AABBCCDDEEFF00112233445566778899AABBCC \"Developer ID Installer: A. E. Cobley (ABCDE12345)\""

expect accept "the app identity, written exactly" \
       -- identity_present "Developer ID Application: A. E. Cobley (ABCDE12345)" "$CODESIGNING"
expect accept "a substring of it, which is how codesign matches" \
       -- identity_present "Developer ID Application: A. E. Cobley" "$CODESIGNING"
expect accept "its SHA-1 hash, passed straight through" \
       -- identity_present "A1B2C3D4E5F60718293A4B5C6D7E8F9012345678" "$CODESIGNING"

# THE HALF-HOUR ONE: a doubled space after the colon.
expect reject "a doubled space after Application:" \
       -- identity_present "Developer ID Application:  A. E. Cobley (ABCDE12345)" "$CODESIGNING"
expect reject "a certificate that genuinely is not installed" \
       -- identity_present "Developer ID Application: Someone Else (ZZZZZ99999)" "$CODESIGNING"
expect reject "a SHA-1 hash nothing in the keychain has" \
       -- identity_present "0000000000000000000000000000000000000000" "$CODESIGNING"
expect reject "any identity at all, on a keychain with none" \
       -- identity_present "Developer ID Application: A. E. Cobley (ABCDE12345)" ""

expect accept "the installer identity, against the unfiltered list" \
       -- identity_present "Developer ID Installer: A. E. Cobley (ABCDE12345)" "$ALL"
expect reject "the installer identity, against -p codesigning" \
       -- identity_present "Developer ID Installer: A. E. Cobley (ABCDE12345)" "$CODESIGNING"

# And the diagnosis has to NAME the working string, not just refuse: the
# message is the entire value of catching this case separately.
# Captured first, not piped: the function returns 1 here (that is the point),
# and under `set -o pipefail` a pipeline carries that failure however well the
# grep goes - which fails the assertion for the wrong reason.
diagnosis="$(identity_present "Developer ID Application:  A. E. Cobley (ABCDE12345)" "$CODESIGNING" 2>&1)"
if printf '%s' "$diagnosis" | grep -q "this works: Developer ID Application: A. E. Cobley (ABCDE12345)"; then
    printf '  ok      %s\n' "and it prints the string that would have worked"
    PASSED=$((PASSED + 1))
else
    printf '  FAIL    %s\n' "it refused the doubled space without naming the fix"
    FAILED=$((FAILED + 1))
fi

#-----------------------------------------------------------------------------
echo
if [ "$FAILED" -eq 0 ]; then
    echo "$PASSED checks passed."
    exit 0
fi
echo "$FAILED of $((PASSED + FAILED)) FAILED - the guards are not guarding."
exit 1
