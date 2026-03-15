#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Nothing Browser — Linux Installer
#  Usage:
#    curl -fsSL https://raw.githubusercontent.com/BunElysiaReact/nothing-browser/main/install.sh | sudo bash
#    or: sudo bash install.sh
# ─────────────────────────────────────────────────────────────────────────────

set -e

REPO="https://github.com/BunElysiaReact/nothing-browser"
RELEASES="https://api.github.com/repos/BunElysiaReact/nothing-browser/releases/latest"
BINARY_NAME="nothing-browser"
INSTALL_DIR="/usr/local/bin"
APP_DIR="/usr/local/share/nothing-browser"
DESKTOP_DIR="/usr/share/applications"

# ── Colors ────────────────────────────────────────────────────────────────────
G="\033[0;32m"; Y="\033[0;33m"; R="\033[0;31m"; NC="\033[0m"; B="\033[1m"

info()    { echo -e "${G}[✓]${NC} $1"; }
warn()    { echo -e "${Y}[!]${NC} $1"; }
error()   { echo -e "${R}[✗]${NC} $1"; exit 1; }
heading() { echo -e "\n${B}$1${NC}"; }

# ── Must run as root ──────────────────────────────────────────────────────────
[[ $EUID -ne 0 ]] && error "Run as root: sudo bash install.sh"

heading "Nothing Browser Installer"
echo    "  Does nothing... except everything that matters."
echo

# ── Detect distro ─────────────────────────────────────────────────────────────
detect_distro() {
    if   command -v apt-get &>/dev/null; then echo "debian"
    elif command -v pacman  &>/dev/null; then echo "arch"
    elif command -v dnf     &>/dev/null; then echo "fedora"
    else                                      echo "unknown"
    fi
}

DISTRO=$(detect_distro)
info "Detected: $DISTRO"

# ── Install Qt6 deps ──────────────────────────────────────────────────────────
heading "Installing dependencies..."

case "$DISTRO" in
debian)
    apt-get update -qq
    apt-get install -y \
        qt6-base-dev qt6-webengine-dev \
        libqt6webenginewidgets6 libqt6webenginecore6 \
        cmake build-essential git curl
    ;;
arch)
    pacman -Sy --noconfirm \
        qt6-base qt6-webengine \
        cmake base-devel git curl
    ;;
fedora)
    dnf install -y \
        qt6-qtbase-devel qt6-qtwebengine-devel \
        cmake gcc-c++ git curl
    ;;
*)
    warn "Unknown distro — skipping auto dependency install."
    warn "Make sure Qt6 WebEngine + cmake are installed manually."
    ;;
esac

info "Dependencies ready."

# ── Decide: download prebuilt binary or build from source ─────────────────────
heading "Fetching latest release..."

# Try to get prebuilt binary from GitHub Releases
LATEST_URL=$(curl -s "$RELEASES" \
    | grep "browser_download_url.*linux.*x86_64" \
    | cut -d '"' -f 4 \
    | head -1)

if [[ -n "$LATEST_URL" ]]; then
    info "Prebuilt binary found: $LATEST_URL"
    TMP=$(mktemp -d)
    curl -L "$LATEST_URL" -o "$TMP/nothing-browser.tar.gz"
    tar -xzf "$TMP/nothing-browser.tar.gz" -C "$TMP"
    install -m 755 "$TMP/$BINARY_NAME" "$INSTALL_DIR/$BINARY_NAME"
    rm -rf "$TMP"
    info "Binary installed to $INSTALL_DIR/$BINARY_NAME"
else
    warn "No prebuilt binary found — building from source..."
    heading "Building from source..."

    TMP=$(mktemp -d)
    git clone --depth=1 "$REPO" "$TMP/nothing-browser"
    mkdir -p "$TMP/nothing-browser/build"
    cd "$TMP/nothing-browser/build"
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make -j"$(nproc)"
    install -m 755 nothing-browser "$INSTALL_DIR/$BINARY_NAME"
    cd /
    rm -rf "$TMP"
    info "Built and installed from source."
fi

# ── Desktop entry ─────────────────────────────────────────────────────────────
heading "Installing desktop entry..."

mkdir -p "$APP_DIR"

cat > "$DESKTOP_DIR/nothing-browser.desktop" << EOF
[Desktop Entry]
Name=Nothing Browser
Comment=Does nothing... except everything that matters
Exec=$INSTALL_DIR/$BINARY_NAME
Icon=$APP_DIR/logo.svg
Terminal=false
Type=Application
Categories=Network;WebBrowser;Development;
Keywords=browser;scraper;devtools;network;
StartupWMClass=nothing-browser
EOF

# Copy logo if repo was cloned
if [[ -f "$TMP/nothing-browser/assets/icons/logo.svg" ]]; then
    cp "$TMP/nothing-browser/assets/icons/logo.svg" "$APP_DIR/logo.svg"
fi

chmod 644 "$DESKTOP_DIR/nothing-browser.desktop"
update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true

# ── Done ──────────────────────────────────────────────────────────────────────
heading "Done!"
info "Run:  nothing-browser"
info "Or find it in your application launcher."
echo
echo -e "  ${Y}Uninstall:${NC} sudo rm $INSTALL_DIR/$BINARY_NAME $DESKTOP_DIR/nothing-browser.desktop"
echo