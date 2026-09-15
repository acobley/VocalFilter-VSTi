#-----------------------------------------------------------------------------
# The three checks in build-installer.sh that can pass without checking
# anything, or fail while blaming the wrong thing.
#
# They are in their own file so that test-guards.sh can run THE SAME CODE
# against a deliberately broken tree. Both of these were written wrong the
# first time in the plug-in this was lifted from, and reading them is what
# failed to notice - a guard that matches nothing reports that nothing is
# wrong, which looks exactly like success.
#
# Sourced, not executed. Each function returns 0 for "this is sound" and 1
# for "do not ship this", and explains itself on stderr.
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# no_absolute_symlinks <dir> [dir...]
#
# Any symlink pointing at an absolute path is a link to THIS machine. The one
# that matters is Contents/Resources/plugin.vst3 inside the .component, which
# CMake points at the build tree and build-installer.sh replaces with a real
# copy - but this has to catch the one somebody adds in two years, not the one
# already known about.
#-----------------------------------------------------------------------------
no_absolute_symlinks () {
    local escapes="" link target

    while IFS= read -r link; do
        target="$(readlink "$link")"
        case "$target" in
            /*) escapes="$escapes
  $link -> $target" ;;
        esac
    done < <(find "$@" -type l)

    if [ -n "$escapes" ]; then
        echo "absolute symlinks in the payload - these point at THIS machine" >&2
        echo "and would dangle on any other:$escapes" >&2
        return 1
    fi
    return 0
}

#-----------------------------------------------------------------------------
# pkg_refs_resolve <expanded-archive-dir>
#
# Every component package the shipped Distribution names must actually be
# inside the archive. A pkg-ref that resolves to nothing is a distribution
# Installer cannot build its panes from, and what the user sees is
# "com.apple.installer.pagecontroller error -1", which names none of this.
#
# FLATTENED FIRST: pkg-ref elements are written across several lines, so a
# line-based scan finds nothing and then reports that nothing is missing.
#-----------------------------------------------------------------------------
pkg_refs_resolve () {
    local expanded="$1" refs missing="" ref

    if [ ! -f "$expanded/Distribution" ]; then
        echo "the product archive has no Distribution file." >&2
        return 1
    fi

    # `|| true` because a grep that matches nothing fails, and this
    # assignment then carries that status. MEASURED, both ways:
    #
    #   * inline at the top level of a script under `set -euo pipefail` -
    #     which is exactly where this code came from - the script dies on
    #     this line, before the message below is ever printed. The build
    #     fails, correctly, and says nothing whatever about why.
    #   * inside a function whose result is tested (`pkg_refs_resolve ... ||`)
    #     `set -e` is suspended for the whole body, so it survives either way.
    #
    # It is kept because the second is a property of how the function happens
    # to be CALLED, and a guard should not depend on that.
    refs=$(tr '\n' ' ' < "$expanded/Distribution" \
           | grep -o '<pkg-ref[^>]*>[^<]*\.pkg</pkg-ref>' \
           | sed 's/.*>\([^<>]*\.pkg\)<.*/\1/' || true)

    # A CHECK WITH NOTHING TO CHECK IS BROKEN, NOT SATISFIED.
    if [ -z "$refs" ]; then
        echo "found no pkg-ref in the shipped Distribution. Either the archive" >&2
        echo "is malformed or this check has stopped working; either way it" >&2
        echo "must not pass silently." >&2
        return 1
    fi

    for ref in $refs; do
        ref="${ref#\#}"                   # both "name.pkg" and "#name.pkg" are legal
        [ -e "$expanded/$ref" ] || missing="$missing $ref"
    done

    if [ -n "$missing" ]; then
        echo "the Distribution names packages that are not inside the" >&2
        echo "archive:$missing" >&2
        return 1
    fi
    return 0
}

#-----------------------------------------------------------------------------
# identity_present <wanted> <listing>
#
# Will `codesign --sign "<wanted>"` find a certificate? Asked BEFORE anything
# is staged or signed, because the answer arriving late looks like a build
# failure rather than a typo.
#
# <listing> is the output of `security find-identity`. Which one matters:
#   --sign-app        security find-identity -v -p codesigning
#   --sign-installer  security find-identity -v
# A Developer ID INSTALLER certificate is not a code-signing certificate and
# does not appear under -p codesigning at all. Checking it against that list
# reports a certificate you have as a certificate you do not.
#-----------------------------------------------------------------------------
identity_present () {
    local wanted="$1" listing="$2" names name want_squashed name_squashed

    # A 40-character hex SHA-1, as --list-identities prints beside each
    # certificate, is exact: it cannot suffer the whitespace problem below,
    # so it is matched as itself and passed straight through.
    if printf '%s' "$wanted" | grep -Eq '^[0-9A-Fa-f]{40}$'; then
        if printf '%s\n' "$listing" | grep -qi "$wanted"; then
            return 0
        fi
        echo "no certificate in this keychain has the hash" >&2
        echo "  $wanted" >&2
        return 1
    fi

    # codesign matches the string as a literal SUBSTRING of the common name,
    # so that is what is checked here - not equality.
    names="$(printf '%s\n' "$listing" | sed -n 's/.*"\(.*\)".*/\1/p')"

    while IFS= read -r name; do
        [ -n "$name" ] || continue
        case "$name" in *"$wanted"*) return 0 ;; esac
    done <<< "$names"

    # THE HALF-HOUR ONE. A doubled space after "Application:" - which is what
    # a copy and paste out of a terminal or a web page tends to produce -
    # makes codesign say "no identity found", which reads exactly like a
    # missing certificate and sends you into Keychain Access looking for
    # something that is sitting right there. So when the only difference is
    # whitespace, SAY SO, and print the string that would have worked.
    want_squashed="$(printf '%s' "$wanted" | tr -s '[:space:]' ' ')"
    while IFS= read -r name; do
        [ -n "$name" ] || continue
        name_squashed="$(printf '%s' "$name" | tr -s '[:space:]' ' ')"
        case "$name_squashed" in
            *"$want_squashed"*)
                echo "no identity matches that string as written, but one matches it" >&2
                echo "once the whitespace is squashed - you have a doubled or stray" >&2
                echo "space, almost certainly from a copy and paste:" >&2
                echo "" >&2
                echo "  you gave:   $wanted" >&2
                echo "  this works: $name" >&2
                return 1 ;;
        esac
    done <<< "$names"

    echo "no certificate in this keychain matches" >&2
    echo "  $wanted" >&2
    if [ -z "$names" ]; then
        echo "and in fact this keychain has no identities of that kind at all." >&2
    else
        echo "What is there:" >&2
        while IFS= read -r name; do
            [ -n "$name" ] && echo "  $name" >&2
        done <<< "$names"
    fi
    return 1
}
