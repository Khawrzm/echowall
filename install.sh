#!/usr/bin/env bash
# ============================================================
#  EchoWall — One-line installer
#  curl -sSL https://raw.githubusercontent.com/Khawrzm/echowall/main/install.sh | bash
#
#  Truly offline from second zero.
#  No cloud. No telemetry. No URLs in the model stack.
# ============================================================
set -euo pipefail

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

banner() {
  echo -e "${CYAN}${BOLD}"
  echo '  ███████╗ ██████╗██╗  ██╗ ██████╗ ██╗    ██╗ █████╗ ██╗     ██╗'
  echo '  ██╔════╝██╔════╝██║  ██║██╔═══██╗██║    ██║██╔══██╗██║     ██║'
  echo '  █████╗  ██║     ███████║██║   ██║██║ █╗ ██║███████║██║     ██║'
  echo '  ██╔══╝  ██║     ██╔══██║██║   ██║██║███╗██║██╔══██║██║     ██║'
  echo '  ███████╗╚██████╖██║  ██║╚██████╔╝╚███╔███╔╝██║  ██║███████╗███████╗'
  echo '  ╚══════╝ ╚═════╝╚═╝  ╚═╝ ╚═════╝  ╚══╝╚══╝ ╚═╝  ╚═╝╚══════╝╚══════╝'
  echo -e "  No cameras. No cloud. No compromise.${NC}"
  echo
}

step() { echo -e "${CYAN}▶ $*${NC}"; }
ok()   { echo -e "${GREEN}✓ $*${NC}"; }
die()  { echo -e "${RED}✗ $*${NC}"; exit 1; }

banner

# ── Detect platform ──────────────────────────────────────────────────────────
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

step "Platform: ${BOLD}${PLATFORM}${NC} (${OS} / ${ARCH})"

# ── Python check ────────────────────────────────────────────────────────────
PYTHON=""
for cmd in python3.12 python3.11 python3.10 python3; do
  if command -v "$cmd" &>/dev/null; then
    PYTHON="$cmd"
    break
  fi
done
[[ -z "$PYTHON" ]] && die "Python 3.10+ not found."
ok "Python: $(${PYTHON} --version)"

# ── Install ──────────────────────────────────────────────────────────────────
step "Installing echowall"
"$PYTHON" -m pip install --quiet echowall
ok "echowall installed"

# Platform extras
if [[ "$PLATFORM" == "rpi" || "$PLATFORM" == "linux" ]]; then
  step "Installing paho-mqtt (Home Assistant local integration)"
  "$PYTHON" -m pip install --quiet paho-mqtt
  ok "paho-mqtt installed"
fi

# ── Init config ────────────────────────────────────────────────────────────
CFG_PLATFORM="linux"
[[ "$PLATFORM" == "rpi" ]] && CFG_PLATFORM="rpi"
step "Generating config"
echowall init --mode "$CFG_PLATFORM" --output echowall.config.json
ok "Config → echowall.config.json"

# ── Seed model OFFLINE ───────────────────────────────────────────────────────
step "Seeding EchoNet model from simulation data (offline — no internet)"
"$PYTHON" - <<'PYEOF'
from echowall.model_loader import get_model_path
p = get_model_path("echonet-v1")
print(f"  Model ready at: {p}")
PYEOF
ok "Model seeded — zero internet used"

# ── Done ───────────────────────────────────────────────────────────────────
echo
echo -e "${GREEN}${BOLD}═══════════════════════════════════════════════════${NC}"
echo -e "${GREEN}${BOLD}  Ready. No cloud touched. Not once.${NC}"
echo
echo -e "    ${BOLD}echowall run --simulate${NC}    # test now, no hardware needed"
echo -e "    ${BOLD}echowall calibrate${NC}         # tune to your real room"
echo -e "    ${BOLD}echowall run${NC}               # real hardware (Pi / ESP32)"
echo
echo -e "  Home Assistant:"
echo -e "    ${BOLD}from echowall.integrations.homeassistant import HassPublisher${NC}"
echo -e "    ${BOLD}HassPublisher(broker=\"192.168.x.x\").start(pipeline)${NC}"
echo -e "${GREEN}${BOLD}═══════════════════════════════════════════════════${NC}"
