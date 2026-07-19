#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Nothing Browser — Debian/Ubuntu .deb package builder
#  Run from repo root: bash scripts/build-deb.sh
# ─────────────────────────────────────────────────────────────────────────────
set -e

VERSION="0.1.0"
ARCH="amd64"
PACKAGE="nothing-browser"
MAINTAINER="BunElysiaReact <hello@nothingbrowser.dev>"
DESCRIPTION="Does nothing... except everything that matters. A meta scrapper browser."

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DIST_DIR="$ROOT_DIR/dist"
PKG_DIR="$DIST_DIR/${PACKAGE}_${VERSION}_${ARCH}"

echo "[*] Building .deb for Nothing Browser v$VERSION"

# ── Build binary first ────────────────────────────────────────────────────────
mkdir -p "$ROOT_DIR/build"
cd "$ROOT_DIR/build"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j"$(nproc)"
cd "$ROOT_DIR"

# ── Create deb structure ──────────────────────────────────────────────────────
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/local/bin"
mkdir -p "$PKG_DIR/usr/share/applications"
mkdir -p "$PKG_DIR/usr/share/nothing-browser"

# Binary
cp "$ROOT_DIR/build/nothing-browser" "$PKG_DIR/usr/local/bin/nothing-browser"
chmod 755 "$PKG_DIR/usr/local/bin/nothing-browser"

# Icon
cp "$ROOT_DIR/assets/icons/logo.svg" "$PKG_DIR/usr/share/nothing-browser/logo.svg"

# Desktop entry
cat > "$PKG_DIR/usr/share/applications/nothing-browser.desktop" << EOF
[Desktop Entry]
Name=Nothing Browser
Comment=$DESCRIPTION
Exec=/usr/local/bin/nothing-browser
Icon=/usr/share/nothing-browser/logo.svg
Terminal=false
Type=Application
Categories=Network;WebBrowser;Development;
Keywords=browser;scraper;devtools;network;
StartupWMClass=nothing-browser
EOF

# Control file
cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: $PACKAGE
Version: $VERSION
Architecture: $ARCH
Maintainer: $MAINTAINER
Depends: libqt6webenginewidgets6, libqt6webenginecore6, libqt6widgets6, libqt6network6
Description: Nothing Browser
 $DESCRIPTION
 .
 Built for scrapers, automation engineers, and API researchers.
EOF

# postinst — update desktop db
cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/sh
update-desktop-database /usr/share/applications || true
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

# postrm — clean up
cat > "$PKG_DIR/DEBIAN/postrm" << 'EOF'
#!/bin/sh
update-desktop-database /usr/share/applications || true
EOF
chmod 755 "$PKG_DIR/DEBIAN/postrm"

# ── Build the .deb ────────────────────────────────────────────────────────────
dpkg-deb --build --root-owner-group "$PKG_DIR"
echo "[✓] Built: $DIST_DIR/${PACKAGE}_${VERSION}_${ARCH}.deb"
echo "    Install: sudo dpkg -i $DIST_DIR/${PACKAGE}_${VERSION}_${ARCH}.deb"