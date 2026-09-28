#!/bin/sh
# scripts/header-update.sh - ensure and update the header of source files.
#
# Usage:
#   ./scripts/header-update.sh <file>...
#
# For each file:
#   - If it has no license header, add one (via scripts/header.sh).
#   - If it has a header, update the @Updated line.
#
# Only files with these extensions are processed:
#   .c .h .hpp .cpp .S
#
# Files with other extensions are silently ignored.
#
# Author info is read from (in order of priority):
#   1. Environment variables AUTHOR_NAME / AUTHOR_EMAIL
#   2. git config hajlib.author.name / hajlib.author.email
#   3. git config user.name / user.email

set -e

if [ "$#" -eq 0 ]; then
    echo "Usage: $0 <file>..."
    exit 1
fi

: "${AUTHOR_NAME:=$(git config hajlib.author.name 2>/dev/null || git config user.name 2>/dev/null || echo 'Unknown')}"

DATE=$(date '+%Y/%m/%d %H:%M:%S')
SCRIPT_DIR=$(dirname "$0")

for f in "$@"; do
    [ ! -f "$f" ] && continue

    # Only process supported extensions
    case "$f" in
        *.c|*.h|*.hpp|*.cpp|*.S|*.inl) ;;
        *) continue ;;
    esac

    # If no license header, add one
    if ! head -5 "$f" | grep -q "SPDX-License-Identifier"; then
        "$SCRIPT_DIR/header.sh" "$f"
        continue
    fi

    # Otherwise, update the @Updated line (if present)
    if ! grep -q "@Updated:" "$f" 2>/dev/null; then
        continue
    fi

    sed "s|^ \* @Updated:.*| * @Updated: $DATE by $AUTHOR_NAME|" \
        "$f" > "$f.tmp"
    mv "$f.tmp" "$f"

    echo "$f: @Updated refreshed"
done
