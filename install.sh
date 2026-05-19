#!/usr/bin/env bash
# ============================================================
#  EchoWall — One-line installer
#  Usage:  curl -sSL https://raw.githubusercontent.com/Khawrzm/echowall/main/install.sh | bash
#
#  Philosophy: fully offline after this script runs.
#  No telemetry. No cloud signup. No hidden dependencies.
# ============================================================
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

banner() {
  echo -e "${CYAN}${BOLD}"
  echo '  ███████╗ ██████╗██╗  ██╗ ██████╗ ██╗    ██╗ █████╗ ██╗     ██╗'
  echo '  ██╔════╝██╔════╝██║  ██║██╔═══██╗██║    ██║██╔══██╗██║     ██║'
  echo '  █████╗  ██║     ███████║██║   ██║██║ █╗ ██║███████║██║     ██║'
  echo '  ██╔══╝  ██║     ██╔══██║██║   ██║██║███╗██║██╔══██║██║     ██║'
  echo '  ███████╗╚██████╗██║  ██║╚██████╔╝╚███╔███╔╝██║  ██║███████╗███████╗'
  echo '  ╚══════╝ ╚═════╝╚═╝  ╚═╝ ╚═════╝  ╚══╝╚══╝ ╚═╝  ╚═╝╚══════╝╚══════╝'
  echo -e "  See through walls. No cameras. No cloud.${NC}"
  echo
}

step()  { echo -e "${CYAN}▶ $*${NC}"; }
ok()    { echo -e "${GREEN}✓ $*${NC}"; }
die()   { echo -e "${RED}✗ $*${NC}"; exit 1; }

banner

# ── Detect platform ────────────────────────────────────────────────────────
OS="$(uname -s)"
ARCH="$(uname -m)"
PLATFORM="linux"

if [[ "$OS" == "Darwin" ]]; then
  PLATFORM="mac"
elif grep -qi "raspberry" /proc/device-tree/model 2>/dev/null; then
  PLATFORM="rpi"
elif [[ "$ARCH" == "aarch64" || "$ARCH" == "armv7l" ]]; then
  PLATFORM="rpi"
fi

step "Detected platform: ${BOLD}${PLATFORM}${NC} (${OS} / ${ARCH})"

# ── Check Python ───────────────────────────────────────────────────────────
PYTHON=""
for cmd in python3.12 python3.11 python3.10 python3; do
  if command -v "$cmd" &>/dev/null; then
    PYTHON="$cmd"
    break
  fi
done
[[ -z "$PYTHON" ]] && die "Python 3.10+ not found. Install it first: https://python.org"
ok "Python: $(${PYTHON} --version)"

# ── Install echowall ───────────────────────────────────────────────────────
step "Installing echowall (pip — no internet required after this step)"
"$PYTHON" -m pip install --quiet echowall
ok "echowall installed"

# ── Platform-specific extras ───────────────────────────────────────────────
if [[ "$PLATFORM" == "rpi" ]]; then
  step "Raspberry Pi detected — installing nexmon_csi helper + paho-mqtt"
  "$PYTHON" -m pip install --quiet paho-mqtt
  ok "paho-mqtt installed (needed for Home Assistant integration)"
elif [[ "$PLATFORM" == "mac" ]]; then
  step "macOS detected — simulation mode only (no Wi-Fi CSI on Mac)"
fi

# ── Init config ────────────────────────────────────────────────────────────
step "Generating default config for platform: ${PLATFORM}"

CFG_PLATFORM="linux"
[[ "$PLATFORM" == "rpi" ]] && CFG_PLATFORM="rpi"

echowall init --mode "$CFG_PLATFORM" --output echowall.config.json
ok "Config written → echowall.config.json"

# ── Pre-fetch model (optional, fully offline after this) ───────────────────
step "Pre-fetching EchoNet model (one-time ~12 MB — runs offline forever after)"
"$PYTHON" - <<'PYEOF'
from echowall.model_loader import get_model_path
try:
    p = get_model_path("echonet-v1")
    print(f"  Model cached at: {p}")
except Exception as e:
    print(f"  Model pre-fetch skipped (will retry on first run): {e}")
PYEOF

# ── Done ───────────────────────────────────────────────────────────────────
echo
echo -e "${GREEN}${BOLD}═══════════════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}  EchoWall is ready. Run:${NC}"
echo
if [[ "$PLATFORM" == "rpi" || "$PLATFORM" == "linux" ]]; then
  echo -e "    ${BOLD}echowall run${NC}                    # real hardware (auto-detect)"
fi
echo -e "    ${BOLD}echowall run --simulate${NC}          # simulation mode (no hardware)"
echo -e "    ${BOLD}echowall calibrate${NC}               # calibrate empty room"
echo
echo -e "  Home Assistant (MQTT Discovery):"
echo -e "    ${BOLD}from echowall.integrations.homeassistant import HassPublisher${NC}"
echo -e "    ${BOLD}HassPublisher(broker=\"YOUR_HA_IP\").start(pipeline)${NC}"
echo
echo -e "  Docs: ${CYAN}https://github.com/Khawrzm/echowall${NC}"
echo -e "${GREEN}${BOLD}═══════════════════════════════════════════════════${NC}"
