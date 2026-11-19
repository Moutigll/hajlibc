#!/bin/sh
# scripts/package.sh - build release artifacts for hajlib.
#
# Usage:
#   ./scripts/package.sh <version> <platform=file[,file...]>...
#
# Each file is a library shipped in lib/ of the platform's tarball:
#   path          copied as lib/<name of the real file>
#   path@name     copied as lib/<name>
# When path is a symbolic link (libft_malloc.so made by `make malloc`),
# the real file is copied and the link is recreated next to it.
#
# Example:
#   ./scripts/package.sh 0.5.0 \
#       linux-x86_64=libhajc.a,libft_malloc.so \
#       linux-aarch64=libhajc-aarch64.a@libhajc.a,libft_malloc-aarch64.so@libft_malloc_aarch64_Linux.so
#
# This script:
#   1. Verifies the git tag vX.Y.Z exists.
#   2. Creates a source tarball from the tag.
#   3. For each platform, creates a binary tarball containing
#      the given libraries, the public headers, and the build
#      configuration.
#   4. Computes SHA-256 hashes for all tarballs.
#
# Output is in releases/vX.Y.Z/.

set -e

# Argument parsing
VERSION="$1"
shift

if [ -z "$VERSION" ] || [ "$#" -eq 0 ]; then
    echo "Usage: $0 <version> <platform=file[,file...]>..."
    echo "Example: $0 0.5.0 linux-x86_64=libhajc.a,libft_malloc.so"
    exit 1
fi

case "$VERSION" in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *)
        echo "Error: invalid version '$VERSION' (expected X.Y.Z)"
        exit 1
        ;;
esac

TAG="v$VERSION"
PROJECT="libhajc"
RELEASE_DIR="releases/$TAG"

# Sanity checks
if ! git rev-parse "$TAG" >/dev/null 2>&1; then
    echo "Error: git tag '$TAG' does not exist"
    exit 1
fi

# entrySrc / entryName: split a "path[@name]" entry. The default name
# is the one of the real file, so a link ships under its target's name.
entrySrc() {
    printf '%s\n' "${1%%@*}"
}

entryName() {
    case "$1" in
        *@*) printf '%s\n' "${1#*@}" ;;
        *)   basename "$(readlink -f "$1")" ;;
    esac
}

# shipLib <entry> <libdir>: copy one library (and its link) into libdir.
shipLib() {
    src=$(entrySrc "$1")
    name=$(entryName "$1")
    cp -L "$src" "$2/$name"
    echo "    lib/$name"
    link=$(basename "$src")
    if [ -L "$src" ] && [ "$link" != "$name" ]; then
        ln -sf "$name" "$2/$link"
        echo "    lib/$link -> $name"
    fi
}

for arg in "$@"; do
    case "$arg" in
        ?*=?*) ;;
        *)
            echo "Error: invalid argument '$arg' (expected platform=file[,file...])"
            exit 1
            ;;
    esac
    names=""
    oldIFS=$IFS; IFS=,
    for entry in ${arg#*=}; do
        src=$(entrySrc "$entry")
        if [ ! -f "$src" ]; then
            echo "Error: library '$src' does not exist (platform ${arg%%=*})"
            exit 1
        fi
        name=$(entryName "$entry")
        case " $names " in
            *" $name "*)
                echo "Error: lib/$name given twice (platform ${arg%%=*})"
                exit 1
                ;;
        esac
        names="$names $name"
    done
    IFS=$oldIFS
done

if [ ! -d include ] || [ ! -d mk ]; then
    echo "Error: must be run from the hajlib source tree"
    exit 1
fi

# Source tarball
echo "=== Creating source tarball ==="
mkdir -p "$RELEASE_DIR"

git archive \
    --format=tar.gz \
    --prefix="$PROJECT-$VERSION/" \
    "$TAG" \
    -o "$RELEASE_DIR/$PROJECT-$VERSION.tar.gz"

# Binary tarballs
for arg in "$@"; do
    platform="${arg%%=*}"

    echo "=== Creating binary tarball for $platform ==="

    pkg="$PROJECT-$VERSION-$platform"
    stage="/tmp/$pkg"

    rm -rf "$stage"
    mkdir -p "$stage/lib"
    mkdir -p "$stage/include"

    oldIFS=$IFS; IFS=,
    for entry in ${arg#*=}; do
        IFS=$oldIFS
        shipLib "$entry" "$stage/lib"
    done
    IFS=$oldIFS
    cp -r include/* "$stage/include/"
    cp mk/config.mk "$stage/"
    cp mk/hajlib.mk "$stage/"
    cp LICENSE "$stage/" 2>/dev/null || true
    cp README.md "$stage/" 2>/dev/null || true
    cp CHANGELOG.md "$stage/" 2>/dev/null || true

    (cd /tmp && tar czf "$pkg.tar.gz" "$pkg")
    mv "/tmp/$pkg.tar.gz" "$RELEASE_DIR/"
    rm -rf "$stage"
done

# Hashes
echo "=== Computing SHA-256 hashes ==="
(cd "$RELEASE_DIR" && sha256sum *.tar.gz > SHA256SUMS)

# Summary
echo ""
echo "=== Release artifacts for $TAG ==="
ls -lh "$RELEASE_DIR"
echo ""
echo "--- SHA256SUMS ---"
cat "$RELEASE_DIR/SHA256SUMS"
echo ""
echo "=== To sign all artifacts ==="
echo ""
echo "    cd $RELEASE_DIR && for f in *.tar.gz; do gpg --armor --detach-sign \"\$f\"; done"
echo ""
