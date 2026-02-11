#!/usr/bin/env bash
# Sea-Claw Installer
# Usage: curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash
#
# This script:
#   1. Installs build dependencies (gcc, make, libcurl, libsqlite3)
#   2. Clones the Sea-Claw repository
#   3. Builds the release binary (~82KB)
#   4. Runs the test suite (61 tests)
#   5. Installs to /usr/local/bin/sea_claw
#   6. Creates a default config at ~/.config/seaclaw/config.json

set -euo pipefail

# ── Colors ───────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
RESET='\033[0m'

info()  { echo -e "${CYAN}[sea-claw]${RESET} $*"; }
ok()    { echo -e "${GREEN}[sea-claw]${RESET} $*"; }
err()   { echo -e "${RED}[sea-claw]${RESET} $*" >&2; }
die()   { err "$*"; exit 1; }

# ── Pre-flight checks ───────────────────────────────────────
info "Sea-Claw Installer v1.0.0"
info "─────────────────────────"

[[ "$(uname -s)" == "Linux" ]] || die "Sea-Claw currently supports Linux only."

# Check for root/sudo
SUDO=""
if [[ $EUID -ne 0 ]]; then
    command -v sudo &>/dev/null || die "Please run as root or install sudo."
    SUDO="sudo"
fi

# ── Install dependencies ────────────────────────────────────
info "Installing build dependencies..."

if command -v apt-get &>/dev/null; then
    $SUDO apt-get update -qq
    $SUDO apt-get install -y -qq build-essential libcurl4-openssl-dev libsqlite3-dev git
elif command -v dnf &>/dev/null; then
    $SUDO dnf install -y gcc make libcurl-devel sqlite-devel git
elif command -v yum &>/dev/null; then
    $SUDO yum install -y gcc make libcurl-devel sqlite-devel git
elif command -v pacman &>/dev/null; then
    $SUDO pacman -Sy --noconfirm gcc make curl sqlite git
elif command -v apk &>/dev/null; then
    $SUDO apk add gcc musl-dev make curl-dev sqlite-dev git
else
    die "Unsupported package manager. Install manually: gcc, make, libcurl-dev, libsqlite3-dev, git"
fi

ok "Dependencies installed."

# ── Clone or update ─────────────────────────────────────────
INSTALL_DIR="${SEACLAW_DIR:-$HOME/seaclaw}"

if [[ -d "$INSTALL_DIR/.git" ]]; then
    info "Updating existing installation at $INSTALL_DIR..."
    cd "$INSTALL_DIR"
    git pull --ff-only
else
    info "Cloning Sea-Claw to $INSTALL_DIR..."
    git clone https://github.com/t4tarzan/seaclaw.git "$INSTALL_DIR"
    cd "$INSTALL_DIR"
fi

# ── Build ────────────────────────────────────────────────────
info "Building release binary..."
make clean
make release

ok "Built successfully: $(ls -lh dist/sea_claw | awk '{print $5}')"

# ── Test ─────────────────────────────────────────────────────
info "Running test suite..."
make test
ok "All tests passed."

# ── Install binary ───────────────────────────────────────────
info "Installing to /usr/local/bin/sea_claw..."
$SUDO install -m 755 dist/sea_claw /usr/local/bin/sea_claw
ok "Installed: $(which sea_claw)"

# ── Create default config ────────────────────────────────────
CONFIG_DIR="$HOME/.config/seaclaw"
CONFIG_FILE="$CONFIG_DIR/config.json"

if [[ ! -f "$CONFIG_FILE" ]]; then
    mkdir -p "$CONFIG_DIR"
    cat > "$CONFIG_FILE" << 'EOF'
{
    "telegram_token": "",
    "telegram_chat_id": 0,
    "db_path": "~/.config/seaclaw/seaclaw.db",
    "log_level": "info",
    "arena_size_mb": 16,
    "llm_provider": "openrouter",
    "llm_api_key": "",
    "llm_model": "moonshotai/kimi-k2.5",
    "llm_api_url": "https://openrouter.ai/api/v1/chat/completions",
    "llm_fallbacks": [
        {
            "provider": "openai",
            "api_key": "",
            "model": "gpt-4o-mini",
            "api_url": "https://api.openai.com/v1/chat/completions"
        },
        {
            "provider": "gemini",
            "api_key": "",
            "model": "gemini-2.0-flash",
            "api_url": "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions"
        }
    ]
}
EOF
    ok "Created config at $CONFIG_FILE"
    info "Edit it with your API keys: nano $CONFIG_FILE"
else
    info "Config already exists at $CONFIG_FILE"
fi

# ── Create .env template ─────────────────────────────────────
ENV_FILE="$CONFIG_DIR/.env"
if [[ ! -f "$ENV_FILE" ]]; then
    cat > "$ENV_FILE" << 'EOF'
# Sea-Claw Environment Variables
# Add your API keys here

# OPENROUTER_API_KEY=sk-or-...
# OPENAI_API_KEY=sk-...
# GEMINI_API_KEY=AI...
# EXA_API_KEY=...
# TELEGRAM_BOT_TOKEN=...
# TELEGRAM_CHAT_ID=...
EOF
    ok "Created .env template at $ENV_FILE"
fi

# ── Done ─────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${GREEN}  ✓ Sea-Claw installed successfully!${RESET}"
echo ""
echo "  Quick start:"
echo "    sea_claw                              # Interactive TUI mode"
echo "    sea_claw --config $CONFIG_FILE        # With config"
echo "    sea_claw --telegram TOKEN --chat ID   # Telegram bot mode"
echo ""
echo "  Commands:"
echo "    /help          Full command reference"
echo "    /tools         List all 50 tools"
echo "    /exec echo hi  Execute a tool"
echo "    /status        System status"
echo ""
echo "  Docs: https://seaclaw.virtualgpt.cloud"
echo "  Repo: https://github.com/t4tarzan/seaclaw"
echo ""
