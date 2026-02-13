#!/bin/bash
set -e

CONFIG_DIR="/root/.config/seaclaw"
CONFIG_FILE="$CONFIG_DIR/config.json"

case "${1:-run}" in
    test)
        echo "=== Running tests (Docker-safe, no ASan) ==="
        cd /seaclaw
        make test-docker 2>&1
        echo ""
        echo "=== All tests passed ==="
        ;;
    shell)
        exec /bin/bash
        ;;
    build)
        echo "=== Rebuilding release ==="
        cd /seaclaw
        make clean && make release 2>&1
        echo "=== Done ==="
        ;;
    doctor)
        exec sea_claw --doctor --config "$CONFIG_FILE"
        ;;
    --help|-h)
        exec sea_claw --help
        ;;
    --onboard)
        # Force re-run onboarding
        mkdir -p "$CONFIG_DIR"
        cd "$CONFIG_DIR"
        exec sea_claw --onboard
        ;;
    *)
        # First launch: if no config exists, run onboarding wizard
        if [ ! -f "$CONFIG_FILE" ]; then
            echo ""
            echo "  ┌─────────────────────────────────────────┐"
            echo "  │  🦀 Welcome to Sea-Claw (Docker)        │"
            echo "  │  No configuration found.                 │"
            echo "  │  Starting first-run setup wizard...      │"
            echo "  └─────────────────────────────────────────┘"
            echo ""
            mkdir -p "$CONFIG_DIR"
            cd "$CONFIG_DIR"
            sea_claw --onboard
            echo ""
            echo "  Config saved to $CONFIG_FILE"
            echo "  Starting Sea-Claw..."
            echo ""
        fi
        exec sea_claw --config "$CONFIG_FILE" "$@"
        ;;
esac
