#!/bin/bash
set -e
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
        exec sea_claw --doctor
        ;;
    *)
        exec sea_claw "$@"
        ;;
esac
