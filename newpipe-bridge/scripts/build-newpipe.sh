#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────────────────
#  Nothing Browser — NewPipe Bridge Builder
#  Run from repo root: bash scripts/build-newpipe.sh
#
#  Requirements: JDK 11+
#  Output: newpipe-bridge/build/libs/newpipe-bridge-1.0.0.jar
#          Copy it next to nothing-browser binary as newpipe-bridge.jar
# ─────────────────────────────────────────────────────────────────────────────
set -e

# in newpipe-bridge/scripts/build-newpipe.sh, replace lines 12-13 with:
BRIDGE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BRIDGE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
ROOT_DIR="$(cd "$BRIDGE_DIR/.." && pwd)"

G="\033[0;32m"; R="\033[0;31m"; NC="\033[0m"
ok()  { echo -e "${G}[✓]${NC} $1"; }
err() { echo -e "${R}[✗]${NC} $1"; exit 1; }

echo "[*] Building NewPipe Bridge JAR..."

# Check Java
java -version 2>/dev/null || err "java not found. Install JDK 11+: sudo apt install openjdk-17-jdk"

cd "$BRIDGE_DIR"

# Download Gradle wrapper if missing
if [ ! -f "gradlew" ]; then
    echo "[*] Setting up Gradle wrapper..."
    gradle wrapper --gradle-version 8.5 2>/dev/null || {
        # Fallback: download manually
        curl -fsSL https://services.gradle.org/distributions/gradle-8.5-bin.zip -o /tmp/gradle.zip
        unzip -q /tmp/gradle.zip -d /tmp/
        export PATH="/tmp/gradle-8.5/bin:$PATH"
        gradle wrapper
    }
fi

chmod +x gradlew

echo "[*] Downloading dependencies and building (first run may take 2-3 min)..."
./gradlew shadowJar --no-daemon -q

JAR="$BRIDGE_DIR/build/libs/newpipe-bridge-1.0.0.jar"

if [ ! -f "$JAR" ]; then
    err "Build failed — jar not found at $JAR"
fi

ok "JAR built: $JAR"

# Copy next to nothing-browser binary
if [ -f "$ROOT_DIR/build/nothing-browser" ]; then
    cp "$JAR" "$ROOT_DIR/build/newpipe-bridge.jar"
    ok "Copied to build/newpipe-bridge.jar"
fi

echo ""
echo "Test it:"
echo "  java -jar $JAR search 'never gonna give you up'"
echo ""
echo "Bridge JAR path for YoutubeTab:"
echo "  Put newpipe-bridge.jar next to the nothing-browser binary"
