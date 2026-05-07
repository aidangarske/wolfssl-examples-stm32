#!/bin/bash
# fips_fetch.sh — Download wolfSSL FIPS Ready bundle and overlay latest stable.
#
# 1. Downloads wolfssl-5.9.1-gplv3-fips-ready.zip from wolfssl.com
# 2. Extracts to third_party/wolfssl-fips-ready/
# 3. Clones wolfssl/wolfssl, checks out latest master-latest-stable tag
# 4. Overlays non-FIPS-boundary files from latest stable onto the FIPS tree
#
# Result: third_party/wolfssl-fips-ready/ has the FIPS module files from 5.9.1
# with everything else updated to the latest stable release.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TOP="$(dirname "$SCRIPT_DIR")"
FIPS_DIR="$TOP/third_party/wolfssl-fips-ready"
TMP_DIR="$TOP/third_party/.fips-tmp"
FIPS_URL="https://www.wolfssl.com/wolfssl-5.9.1-gplv3-fips-ready.zip"

# --- FIPS boundary files (must NOT be overwritten by latest stable) ---
FIPS_BOUNDARY_FILES=(
    "wolfcrypt/src/fips.c"
    "wolfcrypt/src/fips_test.c"
    "wolfcrypt/src/wolfcrypt_first.c"
    "wolfcrypt/src/wolfcrypt_last.c"
)

echo "=== Step 1: Download FIPS Ready bundle ==="
mkdir -p "$TOP/third_party"
if [ -d "$FIPS_DIR" ]; then
    echo "  $FIPS_DIR already exists — skipping download"
    echo "  (delete it and re-run to force a fresh download)"
else
    mkdir -p "$TMP_DIR"
    echo "  Downloading $FIPS_URL ..."
    curl -L -o "$TMP_DIR/fips-ready.zip" "$FIPS_URL"
    echo "  Extracting..."
    unzip -q "$TMP_DIR/fips-ready.zip" -d "$TMP_DIR"
    # The zip extracts to a subdir like wolfssl-5.9.1-gplv3-fips-ready/
    EXTRACTED=$(ls -d "$TMP_DIR"/wolfssl-* 2>/dev/null | head -1)
    if [ -z "$EXTRACTED" ]; then
        echo "ERROR: could not find extracted wolfssl dir in $TMP_DIR"
        exit 1
    fi
    mv "$EXTRACTED" "$FIPS_DIR"
    rm -rf "$TMP_DIR"
    echo "  Extracted to $FIPS_DIR"
fi

echo ""
echo "=== Step 2: Overlay latest stable onto FIPS tree ==="
if [ ! -d "$TOP/third_party/wolfssl-stable" ]; then
    echo "  Cloning wolfssl/wolfssl..."
    git clone --depth=100 https://github.com/wolfSSL/wolfssl.git \
        "$TOP/third_party/wolfssl-stable"
fi

cd "$TOP/third_party/wolfssl-stable"
git fetch --tags
LATEST_TAG=$(git tag -l 'v*-stable' | sort -V | tail -1)
if [ -z "$LATEST_TAG" ]; then
    echo "ERROR: no v*-stable tag found"
    exit 1
fi
echo "  Latest stable tag: $LATEST_TAG"
git checkout "$LATEST_TAG"

echo "  Saving FIPS boundary files..."
for f in "${FIPS_BOUNDARY_FILES[@]}"; do
    if [ -f "$FIPS_DIR/$f" ]; then
        cp "$FIPS_DIR/$f" "$FIPS_DIR/$f.fips-save"
    fi
done

echo "  Syncing non-FIPS files from $LATEST_TAG..."
rsync -a --exclude='.git' \
    "$TOP/third_party/wolfssl-stable/" "$FIPS_DIR/"

echo "  Restoring FIPS boundary files..."
for f in "${FIPS_BOUNDARY_FILES[@]}"; do
    if [ -f "$FIPS_DIR/$f.fips-save" ]; then
        mv "$FIPS_DIR/$f.fips-save" "$FIPS_DIR/$f"
        echo "    restored: $f"
    else
        echo "    WARNING: $f not found in FIPS tree (may not exist in this bundle)"
    fi
done

echo ""
echo "=== Done ==="
echo "FIPS Ready tree: $FIPS_DIR"
echo "FIPS boundary files preserved from 5.9.1 bundle"
echo "Non-FIPS files updated to: $LATEST_TAG"
echo ""
echo "Next: make CONFIG=fips test"
