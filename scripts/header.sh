#!/bin/sh
# scripts/header.sh - add a license header to a source file.
#
# Usage:
#   ./scripts/header.sh <file>
#
# Author info is read from (in order of priority):
#   1. Environment variables AUTHOR_NAME / AUTHOR_EMAIL
#   2. git config hajlib.author.name / hajlib.author.email
#   3. git config user.name / user.email

set -e

FILE="$1"

if [ -z "$FILE" ]; then
    echo "Usage: $0 <file>"
    exit 1
fi

if [ ! -f "$FILE" ]; then
    echo "Error: file '$FILE' does not exist"
    exit 1
fi

# Author info
: "${AUTHOR_NAME:=$(git config hajlib.author.name 2>/dev/null || git config user.name 2>/dev/null || echo 'Unknown')}"
: "${AUTHOR_EMAIL:=$(git config hajlib.author.email 2>/dev/null || git config user.email 2>/dev/null || echo 'unknown@example.com')}"

YEAR=$(date '+%Y')
DATE=$(date '+%Y/%m/%d %H:%M:%S')
FILENAME=$(basename "$FILE")

# Skip if already has a header
if head -5 "$FILE" | grep -q "SPDX-License-Identifier"; then
    echo "$FILE: already has a header"
    exit 0
fi

# File type
case "$FILE" in
    *.c|*.h|*.hpp|*.cpp|*.inl)
        # C/C++ source / header: license + doxygen
        cat > "$FILE.new" << EOF
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) $YEAR $AUTHOR_NAME <$AUTHOR_EMAIL>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

/**
 * @file $FILENAME
 * @brief TODO: brief description.
 * @Created: $DATE by $AUTHOR_NAME
 * @Updated: $DATE by $AUTHOR_NAME
 *
 * TODO: description.
 */

EOF
        ;;
    *.S|*.s|*.asm)
        # Assembly: license only
        cat > "$FILE.new" << EOF
/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) $YEAR $AUTHOR_NAME <$AUTHOR_EMAIL>
 *
 * This file is part of hajlib.
 * See LICENSE for the full license text.
 */

EOF
        ;;
    *)
        echo "Error: unsupported file type: $FILE"
        exit 1
        ;;
esac

cat "$FILE" >> "$FILE.new"
mv "$FILE.new" "$FILE"

echo "$FILE: header added"
