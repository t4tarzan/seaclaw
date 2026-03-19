#!/usr/bin/env bash
# Sea-Claw macOS Interactive Installer
# Usage: curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install-mac.sh | bash
#
# Features:
#   - Interactive setup wizard with arrow-key menus
#   - LLM provider selection (OpenRouter, OpenAI, Gemini, Anthropic, Local)
#   - API key configuration
#   - Optional Telegram bot setup
#   - Optional fallback provider chain
#   - Builds from source, runs tests, installs binary
#   - Launches TUI on completion

set -euo pipefail

# ── Colors & Formatting ──────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
BOLD='\033[1m'
DIM='\033[2m'
RESET='\033[0m'
CLEAR_LINE='\033[2K'
MOVE_UP='\033[1A'

info()    { echo -e "${CYAN}  ▸${RESET} $*"; }
ok()      { echo -e "${GREEN}  ✓${RESET} $*"; }
warn()    { echo -e "${YELLOW}  ⚠${RESET} $*"; }
err()     { echo -e "${RED}  ✗${RESET} $*" >&2; }
die()     { err "$*"; exit 1; }
header()  { echo -e "\n${BOLD}${CYAN}  $*${RESET}\n"; }
divider() { echo -e "  ${DIM}─────────────────────────────────────────────${RESET}"; }

# ── Banner ───────────────────────────────────────────────────
show_banner() {
    clear
    echo ""
    echo -e "${BOLD}${CYAN}"
    echo "    ╔═══════════════════════════════════════════╗"
    echo "    ║                                           ║"
    echo "    ║   🦀  Sea-Claw macOS Installer  v2.0.0   ║"
    echo "    ║                                           ║"
    echo "    ║   Sovereign AI Agent Platform             ║"
    echo "    ║   Pure C11 · 56 Tools · Zero Dependencies ║"
    echo "    ║                                           ║"
    echo "    ╚═══════════════════════════════════════════╝"
    echo -e "${RESET}"
    echo -e "    ${DIM}docs: seaclaw.virtualgpt.cloud${RESET}"
    echo -e "    ${DIM}repo: github.com/t4tarzan/seaclaw${RESET}"
    echo ""
    divider
}

# ── Arrow-Key Menu ───────────────────────────────────────────
# Usage: menu_select RESULT_VAR "Prompt" "option1" "option2" ...
menu_select() {
    local -n _result=$1
    local prompt="$2"
    shift 2
    local options=("$@")
    local count=${#options[@]}
    local selected=0
    local key

    echo -e "\n${BOLD}  $prompt${RESET}"
    echo -e "  ${DIM}Use ↑/↓ arrow keys, Enter to select${RESET}\n"

    # Hide cursor
    tput civis 2>/dev/null || true

    # Draw initial menu
    for i in "${!options[@]}"; do
        if [[ $i -eq $selected ]]; then
            echo -e "  ${CYAN}❯ ${BOLD}${options[$i]}${RESET}"
        else
            echo -e "    ${DIM}${options[$i]}${RESET}"
        fi
    done

    while true; do
        # Read single keypress
        IFS= read -rsn1 key
        if [[ "$key" == $'\x1b' ]]; then
            read -rsn2 key
            case "$key" in
                '[A') # Up arrow
                    ((selected > 0)) && ((selected--))
                    ;;
                '[B') # Down arrow
                    ((selected < count - 1)) && ((selected++))
                    ;;
            esac
        elif [[ "$key" == "" ]]; then
            # Enter pressed
            break
        fi

        # Redraw menu
        for ((i = 0; i < count; i++)); do
            echo -en "${MOVE_UP}"
        done
        for i in "${!options[@]}"; do
            echo -e "${CLEAR_LINE}"
            if [[ $i -eq $selected ]]; then
                echo -e "  ${CYAN}❯ ${BOLD}${options[$i]}${RESET}"
            else
                echo -e "    ${DIM}${options[$i]}${RESET}"
            fi
        done
    done

    # Show cursor
    tput cnorm 2>/dev/null || true

    _result="${options[$selected]}"
    ok "Selected: ${BOLD}${_result}${RESET}"
}

# ── Secure Input (hidden for API keys) ───────────────────────
read_secret() {
    local -n _out=$1
    local prompt="$2"
    local default="${3:-}"

    if [[ -n "$default" ]]; then
        echo -en "  ${BOLD}$prompt${RESET} ${DIM}[${default:0:8}...]:${RESET} "
    else
        echo -en "  ${BOLD}$prompt${RESET}: "
    fi
    read -rs _out
    echo ""

    if [[ -z "$_out" && -n "$default" ]]; then
        _out="$default"
    fi
}

# ── Regular Input ────────────────────────────────────────────
read_input() {
    local -n _out=$1
    local prompt="$2"
    local default="${3:-}"

    if [[ -n "$default" ]]; then
        echo -en "  ${BOLD}$prompt${RESET} ${DIM}[$default]:${RESET} "
    else
        echo -en "  ${BOLD}$prompt${RESET}: "
    fi
    read -r _out

    if [[ -z "$_out" && -n "$default" ]]; then
        _out="$default"
    fi
}

# ── Yes/No Prompt ────────────────────────────────────────────
confirm() {
    local prompt="$1"
    local default="${2:-y}"
    local yn

    if [[ "$default" == "y" ]]; then
        echo -en "  ${BOLD}$prompt${RESET} ${DIM}[Y/n]:${RESET} "
    else
        echo -en "  ${BOLD}$prompt${RESET} ${DIM}[y/N]:${RESET} "
    fi
    read -r yn
    yn="${yn:-$default}"
    [[ "${yn,,}" == "y" || "${yn,,}" == "yes" ]]
}

# ── Provider Config Map ──────────────────────────────────────
get_provider_details() {
    local provider="$1"
    case "$provider" in
        "OpenRouter (recommended)")
            P_ID="openrouter"
            P_MODEL="moonshotai/kimi-k2.5"
            P_URL="https://openrouter.ai/api/v1/chat/completions"
            P_KEY_HINT="sk-or-..."
            P_KEY_ENV="OPENROUTER_API_KEY"
            P_SIGNUP="https://openrouter.ai/keys"
            ;;
        "OpenAI")
            P_ID="openai"
            P_MODEL="gpt-4o-mini"
            P_URL="https://api.openai.com/v1/chat/completions"
            P_KEY_HINT="sk-..."
            P_KEY_ENV="OPENAI_API_KEY"
            P_SIGNUP="https://platform.openai.com/api-keys"
            ;;
        "Google Gemini")
            P_ID="gemini"
            P_MODEL="gemini-2.0-flash"
            P_URL="https://generativelanguage.googleapis.com/v1beta/openai/chat/completions"
            P_KEY_HINT="AI..."
            P_KEY_ENV="GEMINI_API_KEY"
            P_SIGNUP="https://aistudio.google.com/apikey"
            ;;
        "Anthropic")
            P_ID="anthropic"
            P_MODEL="claude-3-5-sonnet-20241022"
            P_URL="https://api.anthropic.com/v1/messages"
            P_KEY_HINT="sk-ant-..."
            P_KEY_ENV="ANTHROPIC_API_KEY"
            P_SIGNUP="https://console.anthropic.com/settings/keys"
            ;;
        "Local (Ollama / LM Studio)")
            P_ID="local"
            P_MODEL="llama3.2"
            P_URL="http://localhost:11434/v1/chat/completions"
            P_KEY_HINT=""
            P_KEY_ENV=""
            P_SIGNUP=""
            ;;
    esac
}

# ══════════════════════════════════════════════════════════════
# MAIN INSTALLER
# ══════════════════════════════════════════════════════════════

show_banner

# ── Step 1: Pre-flight ───────────────────────────────────────
header "Step 1/6 — System Check"

[[ "$(uname -s)" == "Darwin" ]] || die "This installer is for macOS only. On Linux, run: curl -fsSL https://raw.githubusercontent.com/t4tarzan/seaclaw/main/install.sh | bash"
ok "macOS detected: $(sw_vers -productVersion) ($(uname -m))"

# Xcode Command Line Tools check
if ! xcode-select -p &>/dev/null 2>&1; then
    info "Xcode Command Line Tools not found — installing..."
    warn "A dialog will appear. Click 'Install', wait for completion, then press Enter."
    xcode-select --install 2>/dev/null || true
    echo -ne "  Press Enter once Xcode CLT installation completes... "
    read -r
fi
ok "Xcode CLT: $(xcode-select -p)"

# Detect Homebrew prefix (Apple Silicon vs Intel)
if [[ "$(uname -m)" == "arm64" ]]; then
    BREW_PREFIX="/opt/homebrew"
else
    BREW_PREFIX="/usr/local"
fi
export PATH="$BREW_PREFIX/bin:$PATH"

# ── Step 2: Homebrew + Dependencies ──────────────────────────
header "Step 2/6 — Installing Build Dependencies"

if ! command -v brew &>/dev/null; then
    info "Homebrew not found — installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    export PATH="$BREW_PREFIX/bin:$PATH"
fi
ok "Homebrew: $(brew --version | head -1)"

info "Installing dependencies (curl, sqlite3, git)..."
brew install curl sqlite3 git 2>/dev/null || true
ok "Dependencies ready (curl, sqlite3, git)"

# ── Step 3: Clone & Build ────────────────────────────────────
header "Step 3/6 — Building Sea-Claw"

INSTALL_DIR="${SEACLAW_DIR:-$HOME/seaclaw}"

if [[ -d "$INSTALL_DIR/.git" ]]; then
    info "Updating existing installation..."
    cd "$INSTALL_DIR"
    git pull --ff-only 2>/dev/null
else
    info "Cloning repository..."
    git clone --depth 1 https://github.com/t4tarzan/seaclaw.git "$INSTALL_DIR" 2>/dev/null
    cd "$INSTALL_DIR"
fi

info "Compiling release binary..."
make clean 2>/dev/null
make release 2>&1 | tail -3

BINARY_SIZE=$(ls -lh dist/sea_claw | awk '{print $5}')
ok "Built: dist/sea_claw (${BINARY_SIZE})"

info "Running tests (12 suites)..."
make test 2>&1 | grep -E "(passed|failed|Results)" | tail -12
ok "All tests passed"

info "Installing to /usr/local/bin..."
sudo install -m 755 dist/sea_claw /usr/local/bin/sea_claw
ok "Installed: /usr/local/bin/sea_claw"

# ── Step 4: LLM Provider Setup ──────────────────────────────
header "Step 4/6 — LLM Provider Configuration"

echo -e "  ${DIM}Sea-Claw needs an LLM provider for AI chat.${RESET}"
echo -e "  ${DIM}You can change this later in the config file.${RESET}"

PROVIDER_CHOICE=""
menu_select PROVIDER_CHOICE "Select your primary LLM provider:" \
    "OpenRouter (recommended)" \
    "OpenAI" \
    "Google Gemini" \
    "Anthropic" \
    "Local (Ollama / LM Studio)"

get_provider_details "$PROVIDER_CHOICE"
LLM_PROVIDER="$P_ID"
LLM_MODEL="$P_MODEL"
LLM_URL="$P_URL"
LLM_KEY=""

if [[ "$P_ID" != "local" ]]; then
    echo ""
    if [[ -n "$P_SIGNUP" ]]; then
        echo -e "  ${DIM}Get your API key at: ${CYAN}$P_SIGNUP${RESET}"
    fi
    read_secret LLM_KEY "Enter your $P_ID API key" ""

    if [[ -z "$LLM_KEY" ]]; then
        warn "No API key provided. You can add it later in the config."
    else
        ok "API key saved (${#LLM_KEY} chars)"
    fi
else
    info "Local mode — make sure Ollama or LM Studio is running"
    read_input LLM_URL "Local API URL" "$P_URL"
    read_input LLM_MODEL "Model name" "$P_MODEL"
fi

# Custom model override
if [[ "$P_ID" != "local" ]]; then
    echo ""
    if confirm "Use default model ($LLM_MODEL)?" "y"; then
        : # keep default
    else
        read_input LLM_MODEL "Enter model name" "$LLM_MODEL"
    fi
fi

# ── Fallback Providers ───────────────────────────────────────
FALLBACKS_JSON=""
echo ""
if confirm "Add fallback LLM providers? (recommended)" "y"; then
    FALLBACK_LIST=()

    # Offer providers not already selected
    ALL_PROVIDERS=("OpenRouter (recommended)" "OpenAI" "Google Gemini" "Anthropic" "Local (Ollama / LM Studio)")
    AVAILABLE=()
    for p in "${ALL_PROVIDERS[@]}"; do
        get_provider_details "$p"
        if [[ "$P_ID" != "$LLM_PROVIDER" ]]; then
            AVAILABLE+=("$p")
        fi
    done
    AVAILABLE+=("Done — no more fallbacks")

    for i in 1 2 3; do
        if [[ ${#AVAILABLE[@]} -le 1 ]]; then
            break
        fi

        FB_CHOICE=""
        menu_select FB_CHOICE "Fallback #$i:" "${AVAILABLE[@]}"

        if [[ "$FB_CHOICE" == "Done — no more fallbacks" ]]; then
            break
        fi

        get_provider_details "$FB_CHOICE"
        FB_KEY=""

        if [[ "$P_ID" != "local" ]]; then
            read_secret FB_KEY "Enter $P_ID API key" ""
        fi

        FALLBACK_LIST+=("{\"provider\":\"$P_ID\",\"api_key\":\"$FB_KEY\",\"model\":\"$P_MODEL\",\"api_url\":\"$P_URL\"}")
        ok "Fallback #$i: $P_ID ($P_MODEL)"

        # Remove selected from available
        NEW_AVAILABLE=()
        for a in "${AVAILABLE[@]}"; do
            if [[ "$a" != "$FB_CHOICE" ]]; then
                NEW_AVAILABLE+=("$a")
            fi
        done
        AVAILABLE=("${NEW_AVAILABLE[@]}")
    done

    if [[ ${#FALLBACK_LIST[@]} -gt 0 ]]; then
        FALLBACKS_JSON=$(IFS=,; echo "${FALLBACK_LIST[*]}")
    fi
fi

# ── Step 5: Telegram Bot Setup ───────────────────────────────
header "Step 5/6 — Telegram Bot (Optional)"

echo -e "  ${DIM}Connect a Telegram bot for mobile/remote access.${RESET}"
echo -e "  ${DIM}Create one at: ${CYAN}https://t.me/BotFather${RESET}"
echo ""

TG_TOKEN=""
TG_CHAT_ID="0"

if confirm "Set up Telegram bot?" "n"; then
    read_secret TG_TOKEN "Bot token from @BotFather" ""

    if [[ -n "$TG_TOKEN" ]]; then
        ok "Bot token saved"
        echo ""
        echo -e "  ${DIM}To restrict the bot to your chat only, enter your chat ID.${RESET}"
        echo -e "  ${DIM}How to get it:${RESET}"
        echo -e "  ${DIM}  1. Message ${CYAN}@userinfobot${RESET}${DIM} on Telegram — it replies with your ID${RESET}"
        echo -e "  ${DIM}  2. Or open: ${CYAN}https://api.telegram.org/bot${TG_TOKEN}/getUpdates${RESET}"
        echo -e "  ${DIM}     (send your bot a message first, then look for \"chat\":{\"id\":...})${RESET}"
        echo -e "  ${DIM}Leave blank (0) to allow all chats.${RESET}"
        read_input TG_CHAT_ID "Telegram chat ID" "0"
    fi
else
    info "Skipped — you can add Telegram later in the config"
fi

# ── Step 6: Write Config & Launch ────────────────────────────
header "Step 6/6 — Saving Configuration"

CONFIG_DIR="$HOME/.config/seaclaw"
CONFIG_FILE="$CONFIG_DIR/config.json"
ENV_FILE="$CONFIG_DIR/.env"
DB_PATH="$CONFIG_DIR/seaclaw.db"

mkdir -p "$CONFIG_DIR"

# Write config.json
cat > "$CONFIG_FILE" << JSONEOF
{
    "telegram_token": "$TG_TOKEN",
    "telegram_chat_id": $TG_CHAT_ID,
    "db_path": "$DB_PATH",
    "log_level": "info",
    "arena_size_mb": 16,
    "llm_provider": "$LLM_PROVIDER",
    "llm_api_key": "$LLM_KEY",
    "llm_model": "$LLM_MODEL",
    "llm_api_url": "$LLM_URL",
    "llm_fallbacks": [$(echo "$FALLBACKS_JSON" | sed 's/},{/},\n        {/g')]
}
JSONEOF

ok "Config saved: $CONFIG_FILE"

# Write .env
cat > "$ENV_FILE" << ENVEOF
# Sea-Claw Environment — auto-generated by installer
ENVEOF

# Map provider keys to env vars
if [[ -n "$LLM_KEY" ]]; then
    get_provider_details "$PROVIDER_CHOICE"
    if [[ -n "$P_KEY_ENV" ]]; then
        echo "$P_KEY_ENV=$LLM_KEY" >> "$ENV_FILE"
    fi
fi

if [[ -n "$TG_TOKEN" ]]; then
    echo "TELEGRAM_BOT_TOKEN=$TG_TOKEN" >> "$ENV_FILE"
    echo "TELEGRAM_CHAT_ID=$TG_CHAT_ID" >> "$ENV_FILE"
fi

chmod 600 "$CONFIG_FILE" "$ENV_FILE"
ok "Environment saved: $ENV_FILE"

# ── Summary & Launch ─────────────────────────────────────────
echo ""
divider
echo ""
echo -e "${BOLD}${GREEN}    🦀 Sea-Claw installed successfully!${RESET}"
echo ""
echo -e "    ${BOLD}Binary:${RESET}    /usr/local/bin/sea_claw (${BINARY_SIZE})"
echo -e "    ${BOLD}Config:${RESET}    $CONFIG_FILE"
echo -e "    ${BOLD}Database:${RESET}  $DB_PATH"
echo -e "    ${BOLD}Provider:${RESET}  $LLM_PROVIDER ($LLM_MODEL)"
if [[ -n "$FALLBACKS_JSON" ]]; then
echo -e "    ${BOLD}Fallbacks:${RESET} configured"
fi
if [[ -n "$TG_TOKEN" ]]; then
echo -e "    ${BOLD}Telegram:${RESET}  enabled (chat: $TG_CHAT_ID)"
fi
echo ""
echo -e "    ${DIM}Docs:${RESET} https://seaclaw.virtualgpt.cloud"
echo -e "    ${DIM}Repo:${RESET} https://github.com/t4tarzan/seaclaw"
echo ""
divider
echo ""

# Launch options
LAUNCH_CHOICE=""
if [[ -n "$TG_TOKEN" ]]; then
    menu_select LAUNCH_CHOICE "What would you like to do?" \
        "Launch Sea-Claw TUI (interactive mode)" \
        "Launch Telegram Bot" \
        "Exit (launch later)"
else
    menu_select LAUNCH_CHOICE "What would you like to do?" \
        "Launch Sea-Claw TUI (interactive mode)" \
        "Exit (launch later)"
fi

case "$LAUNCH_CHOICE" in
    "Launch Sea-Claw TUI (interactive mode)")
        echo ""
        info "Starting Sea-Claw TUI..."
        echo -e "  ${DIM}Type /help for commands, /tools to see all 56 tools${RESET}"
        echo ""
        exec sea_claw --config "$CONFIG_FILE"
        ;;
    "Launch Telegram Bot")
        echo ""
        info "Starting Telegram bot..."
        echo -e "  ${DIM}Press Ctrl+C to stop${RESET}"
        echo ""
        exec sea_claw --config "$CONFIG_FILE" --telegram "$TG_TOKEN" --chat "$TG_CHAT_ID"
        ;;
    *)
        echo ""
        echo -e "  Run anytime with:"
        echo -e "    ${CYAN}sea_claw${RESET}                              # TUI mode"
        echo -e "    ${CYAN}sea_claw --config $CONFIG_FILE${RESET}"
        echo -e "    ${CYAN}sea_claw --doctor${RESET}                     # Diagnose config"
        if [[ -n "$TG_TOKEN" ]]; then
        echo -e "    ${CYAN}sea_claw --telegram${RESET}                   # Bot mode"
        fi
        echo ""
        ;;
esac
