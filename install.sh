#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Nothing Browser — Linux Installer
#
#  Usage:
#    # Full browser (installs system-wide, any directory):
#    curl -fsSL https://raw.githubusercontent.com/BunElysiaReact/nothing-browser/main/install.sh | sudo bash
#    sudo bash install.sh
#
#    # Headless daemon (downloads binary into your current project root):
#    bash install.sh --headless
#
#    # Headful (script-controlled visible window, downloads into project root):
#    bash install.sh --headful
# ─────────────────────────────────────────────────────────────────────────────

set -e

REPO="https://github.com/BunElysiaReact/nothing-browser"
RELEASES="https://api.github.com/repos/BunElysiaReact/nothing-browser/releases/latest"
INSTALL_DIR="/usr/local/bin"
APP_DIR="/usr/local/share/nothing-browser"
DESKTOP_DIR="/usr/share/applications"
PROJECT_ROOT="$(pwd)"

G="\033[0;32m"; Y="\033[0;33m"; R="\033[0;31m"; NC="\033[0m"; B="\033[1m"

info()    { echo -e "${G}[✓]${NC} $1"; }
warn()    { echo -e "${Y}[!]${NC} $1"; }
error()   { echo -e "${R}[✗]${NC} $1"; exit 1; }
heading() { echo -e "\n${B}$1${NC}"; }

# ── Parse mode ────────────────────────────────────────────────────────────────
MODE="full"
case "${1:-}" in
  --headless) MODE="headless" ;;
  --headful)  MODE="headful"  ;;
  "")         MODE="full"     ;;
  *) error "Unknown option: $1. Use --headless or --headful." ;;
esac

# ── Root check (only needed for full install) ─────────────────────────────────
if [[ "$MODE" == "full" && $EUID -ne 0 ]]; then
  error "Full install requires root: sudo bash install.sh"
fi

heading "Nothing Browser Installer"
echo    "  Does nothing... except everything that matters."
echo    "  Mode: $MODE"
echo

# ── Detect distro ─────────────────────────────────────────────────────────────
detect_distro() {
  if   command -v apt-get &>/dev/null; then echo "debian"
  elif command -v pacman  &>/dev/null; then echo "arch"
  elif command -v dnf     &>/dev/null; then echo "fedora"
  else                                      echo "unknown"
  fi
}

# ── Install deps (full mode only) ─────────────────────────────────────────────
if [[ "$MODE" == "full" ]]; then
  DISTRO=$(detect_distro)
  info "Detected distro: $DISTRO"
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
fi

# ── Resolve asset name from mode ─────────────────────────────────────────────
case "$MODE" in
  full)     ASSET_PATTERN="linux.*x86_64" ; BINARY_NAME="nothing-browser"          ;;
  headless) ASSET_PATTERN="headless.*linux.*x86_64" ; BINARY_NAME="nothing-browser-headless" ;;
  headful)  ASSET_PATTERN="headful.*linux.*x86_64"  ; BINARY_NAME="nothing-browser-headful"  ;;
esac

# ── Fetch binary ──────────────────────────────────────────────────────────────
heading "Fetching latest release..."

LATEST_URL=$(curl -s "$RELEASES" \
  | grep "browser_download_url.*${ASSET_PATTERN}" \
  | grep -v "headless\|headful" \
  | cut -d '"' -f 4 \
  | head -1)

# For headless/headful the grep above is too greedy — redo properly
if [[ "$MODE" == "headless" ]]; then
  LATEST_URL=$(curl -s "$RELEASES" \
    | grep "browser_download_url" \
    | grep "headless.*linux.*x86_64" \
    | cut -d '"' -f 4 \
    | head -1)
elif [[ "$MODE" == "headful" ]]; then
  LATEST_URL=$(curl -s "$RELEASES" \
    | grep "browser_download_url" \
    | grep "headful.*linux.*x86_64" \
    | cut -d '"' -f 4 \
    | head -1)
fi

TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

if [[ -n "$LATEST_URL" ]]; then
  info "Downloading: $LATEST_URL"
  curl -L "$LATEST_URL" -o "$TMP/archive.tar.gz"
  tar -xzf "$TMP/archive.tar.gz" -C "$TMP"
  EXTRACTED_BIN=$(find "$TMP" -type f -name "$BINARY_NAME" | head -1)
  [[ -z "$EXTRACTED_BIN" ]] && error "Binary '$BINARY_NAME' not found in archive."
else
  warn "No prebuilt binary found — building from source..."
  heading "Building from source..."

  git clone --depth=1 "$REPO" "$TMP/nothing-browser"
  mkdir -p "$TMP/nothing-browser/build"
  cd "$TMP/nothing-browser/build"
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make -j"$(nproc)" "$BINARY_NAME"
  EXTRACTED_BIN="$TMP/nothing-browser/build/$BINARY_NAME"
  cd /
fi

# ── Install to correct location ───────────────────────────────────────────────
if [[ "$MODE" == "full" ]]; then
  install -m 755 "$EXTRACTED_BIN" "$INSTALL_DIR/$BINARY_NAME"

  # Copy JAR if present alongside binary
  EXTRACTED_JAR=$(find "$TMP" -type f -name "newpipe-bridge.jar" | head -1)
  [[ -n "$EXTRACTED_JAR" ]] && install -m 644 "$EXTRACTED_JAR" "$INSTALL_DIR/newpipe-bridge.jar"

  heading "Installing desktop entry..."
  mkdir -p "$APP_DIR"

  cat > "$DESKTOP_DIR/nothing-browser.desktop" <<EOF
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
  chmod 644 "$DESKTOP_DIR/nothing-browser.desktop"
  update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true

  heading "Done!"
  info "Run: nothing-browser"
  info "Or find it in your application launcher."
  echo
  echo -e "  ${Y}Uninstall:${NC} sudo rm $INSTALL_DIR/$BINARY_NAME $DESKTOP_DIR/nothing-browser.desktop"

else
  # headless / headful → drop binary into project root
  DEST="$PROJECT_ROOT/$BINARY_NAME"
  install -m 755 "$EXTRACTED_BIN" "$DEST"

  heading "Done!"
  info "Binary saved to: $DEST"
  info "Run: ./$BINARY_NAME"
fi

echo