#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Nothing Browser — Release Script
#  Usage: bash scripts/release.sh 0.2.0 "Fixed WS capture, added Windows build"
#
#  Requirements: gh (GitHub CLI), dpkg-deb, cmake, qt6
# ─────────────────────────────────────────────────────────────────────────────
set -e

VERSION="${1:?Usage: release.sh <version> <notes>}"
NOTES="${2:?Usage: release.sh <version> <notes>}"
PACKAGE="nothing-browser"

G="\033[0;32m"; Y="\033[0;33m"; NC="\033[0m"; B="\033[1m"
info()    { echo -e "${G}[✓]${NC} $1"; }
heading() { echo -e "\n${B}$1${NC}"; }

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"

heading "Releasing Nothing Browser v$VERSION"

# ── 1. Update version.json ────────────────────────────────────────────────────
info "Updating version.json..."
# Use python for clean JSON edit (available everywhere)
python3 - << PYEOF
import json, sys
with open("version.json", "r") as f:
    data = json.load(f)
data["version"] = "$VERSION"
with open("version.json", "w") as f:
    json.dump(data, f, indent=2)
print("  version.json updated to $VERSION")
PYEOF

# ── 2. Commit + tag ───────────────────────────────────────────────────────────
info "Committing version bump..."
git add version.json
git commit -m "chore: bump version to v$VERSION"
git tag "v$VERSION"
git push origin main
git push origin "v$VERSION"

# ── 3. Build .deb ─────────────────────────────────────────────────────────────
heading "Building .deb package..."
bash "$ROOT_DIR/scripts/build-deb.sh"
DEB_FILE="$ROOT_DIR/dist/${PACKAGE}_${VERSION}_amd64.deb"

# ── 4. Build tar.gz of binary ─────────────────────────────────────────────────
heading "Packaging binary tarball..."
mkdir -p "$ROOT_DIR/dist"
cd "$ROOT_DIR/build"
tar -czf "$ROOT_DIR/dist/${PACKAGE}-${VERSION}-linux-x86_64.tar.gz" nothing-browser
cd "$ROOT_DIR"
info "Tarball: dist/${PACKAGE}-${VERSION}-linux-x86_64.tar.gz"

# ── 5. Create GitHub release ──────────────────────────────────────────────────
heading "Creating GitHub release v$VERSION..."
gh release create "v$VERSION" \
    --title "Nothing Browser v$VERSION" \
    --notes "$NOTES" \
    "$DEB_FILE" \
    "$ROOT_DIR/dist/${PACKAGE}-${VERSION}-linux-x86_64.tar.gz"

info "Release published: https://github.com/BunElysiaReact/nothing-browser/releases/tag/v$VERSION"
echo
echo "Users can now install with:"
echo "  curl -fsSL https://raw.githubusercontent.com/BunElysiaReact/nothing-browser/main/install.sh | sudo bash"