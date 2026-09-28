#!/bin/sh
set -e

# Usage: ./scripts/release.sh <version>
# Example: ./scripts/release.sh 0.2.0

VERSION="$1"

if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 0.2.0"
    exit 1
fi

# Validate version format (X.Y.Z)
case "$VERSION" in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *)
        echo "Error: invalid version '$VERSION' (expected X.Y.Z)"
        exit 1
        ;;
esac

# Check branch
BRANCH=$(git rev-parse --abbrev-ref HEAD)
case "$BRANCH" in
    hajlibc|main|dev|release*) ;;
    *) echo "Branch $BRANCH not allowed" >&2 ; exit 1 ;;
esac

# Check clean working tree
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: working tree is not clean"
    git status --short
    exit 1
fi

# Parse version
MAJOR=$(echo "$VERSION" | cut -d. -f1)
MINOR=$(echo "$VERSION" | cut -d. -f2)
PATCH=$(echo "$VERSION" | cut -d. -f3)

VERSION_HEADER="include/haj/version.h"

# Update version.h
sed -i.bak \
    -e "s/^# define HAJ_VERSION_MAJOR[[:space:]].*/# define HAJ_VERSION_MAJOR	$MAJOR/" \
    -e "s/^# define HAJ_VERSION_MINOR[[:space:]].*/# define HAJ_VERSION_MINOR	$MINOR/" \
    -e "s/^# define HAJ_VERSION_PATCH[[:space:]].*/# define HAJ_VERSION_PATCH	$PATCH/" \
    "$VERSION_HEADER"
rm -f "$VERSION_HEADER.bak"

# Commit
git add "$VERSION_HEADER"
git commit -m "chore(release): bump version to $VERSION"

# Tag
git tag -a "v$VERSION" -m "Release $VERSION"

echo ""
echo "Release v$VERSION prepared."
echo ""
echo "Next steps:"
echo "  1. Review the commit: git show HEAD"
echo "  2. Push: git push origin $BRANCH --tags"
echo "  3. Update CHANGELOG.md if not done"
