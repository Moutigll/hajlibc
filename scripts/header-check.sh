#!/bin/sh
# scripts/header-check.sh - check that files have a valid header.
#
# Usage:
#   ./scripts/header-check.sh [file|dir]...
#
# If no argument is given, checks src/ and include/.
#
# Exit status:
#   0 : all files have a complete header
#   1 : at least one file is missing a header field
#

set -e

if [ "$#" -eq 0 ]; then
    FILES=$(find src include -type f \
        \( -name '*.c' -o -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.S' -o -name '*.inl' \) \
        2>/dev/null)
else
    FILES=""
    for arg in "$@"; do
        if [ -d "$arg" ]; then
            FILES="$FILES $(find "$arg" -type f \
                \( -name '*.c' -o -name '*.h' -o -name '*.hpp' -o -name '*.cpp' -o -name '*.S' -o -name '*.inl' \) \
                2>/dev/null)"
        else
            FILES="$FILES $arg"
        fi
    done
fi

FAIL=0

# ---------------------------------------------------------------------------
# Check that a Doxygen tag has real content (not TODO/FIXME/empty).
#   $1 = file
#   $2 = tag (e.g. "@brief")
#   $3 = field name for MISSING (e.g. "@brief-content")
# Returns 0 if OK, 1 if placeholder or empty.
#
# Extracts the value after the tag using awk (robust), lowercases it,
# and rejects values that start with a placeholder keyword (TODO,
# FIXME, ...) or are too short.
# ---------------------------------------------------------------------------
check_content() {
    value=$(awk -v tag="$2" '
        index($0, tag) {
            sub(/^.*'"$2"'[ \t]*/, "")
            sub(/[ \t]*\*\/[ \t]*$/, "")
            sub(/[ \t]+$/, "")
            print
            exit
        }
    ' "$1")

    value_lc=$(printf '%s' "$value" | tr '[:upper:]' '[:lower:]')

    # Empty
    if [ -z "$value_lc" ]; then
        return 1
    fi

    # Placeholder keyword (prefix match)
    case "$value_lc" in
        todo*|fixme*|tbd*|xxx*|tba*|tbc*|wip*|placeholder*|\
        "to be done"*|"to do"*|"à faire"*|"à compléter"*|\
        none|n/a|"not implemented"*|unimplemented*|\
        "brief description."*|"description."*|\
        "..."*|"<..."*)
            return 1
            ;;
    esac

    # Minimum length: 5 characters
    [ "${#value_lc}" -lt 5 ] && return 1

    return 0
}

for f in $FILES; do
    [ ! -f "$f" ] && continue

    # Only process supported extensions
    case "$f" in
        *.c|*.h|*.hpp|*.cpp|*.S|*.inl) ;;
        *) continue ;;
    esac

    MISSING=""

    # License block (all files)
    head -30 "$f" | grep -q "SPDX-License-Identifier"    || MISSING="$MISSING SPDX"
    head -30 "$f" | grep -q "^ \* Copyright (C)"         || MISSING="$MISSING Copyright"
    head -30 "$f" | grep -q "This file is part of hajlib" || MISSING="$MISSING ProjectName"
    head -30 "$f" | grep -q "See LICENSE"                || MISSING="$MISSING LicenseRef"

    # Doxygen block (C/C++ only, not assembly)
    case "$f" in
        *.c|*.h|*.hpp|*.cpp|*.inl)
            head -30 "$f" | grep -q "@file "    || MISSING="$MISSING @file"
            head -30 "$f" | grep -q "@brief "   || MISSING="$MISSING @brief"
            head -30 "$f" | grep -q "@Created:" || MISSING="$MISSING @Created"
            head -30 "$f" | grep -q "@Updated:" || MISSING="$MISSING @Updated"

            # Real content of @brief (not TODO/FIXME/empty)
            if head -30 "$f" | grep -q "@brief "; then
                check_content "$f" "@brief" \
                    || MISSING="$MISSING @brief-content"
            fi

            # Real content of @Updated: (not empty, not TODO)
            if head -30 "$f" | grep -q "@Updated:"; then
                check_content "$f" "@Updated:" \
                    || MISSING="$MISSING @Updated-content"
            fi

            # Real content of @details if present
            if head -30 "$f" | grep -q "@details "; then
                check_content "$f" "@details" \
                    || MISSING="$MISSING @details-content"
            fi
            ;;
    esac

    if [ -n "$MISSING" ]; then
        echo "$f: MISSING$MISSING"
        FAIL=$((FAIL + 1))
    fi
done

if [ "$FAIL" -eq 0 ]; then
    echo "All files OK"
    exit 0
fi

echo "$FAIL file(s) with missing header fields"
exit 1
