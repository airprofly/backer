#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

TAG="${TAG:-latest}"
GH_PROXY=""
NO_CACHE=""
TARGET="default"

# ── Parse arguments ────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --target)
            TARGET="${2:?Usage: --target <name>}"
            shift 2
            ;;
        --proxy)
            GH_PROXY="${2:-https://ghproxy.net/}"
            shift ${2:+2}
            ;;
        --no-cache)
            NO_CACHE="--no-cache"
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [--target <name>] [--proxy [URL]] [--no-cache]"
            echo ""
            echo "  --target <name>  Bake target (default: default)"
            echo "  --proxy [URL]    GitHub proxy for Chinese users"
            echo "  --no-cache       Disable cache"
            echo ""
            echo "Examples:"
            echo "  TAG=v1.0 $0                      # build & tag"
            echo "  $0 --proxy                       # with proxy mirror"
            echo "  $0 --no-cache                    # fresh build"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [--help]"
            exit 1
            ;;
    esac
done

cd "$PROJECT_ROOT"

BAKE_ARGS=(
    --load
    "$TARGET"
)
[[ -n "$NO_CACHE" ]] && BAKE_ARGS+=(--no-cache)
[[ -n "$GH_PROXY" ]] && BAKE_ARGS+=(--set "*.args.GH_PROXY=$GH_PROXY")

echo ""
echo "Baking image (target: $TARGET, tag: $TAG, proxy: ${GH_PROXY:-(none)})"
echo ""

TAG="$TAG" docker buildx bake "${BAKE_ARGS[@]}"

echo ""
echo "Verifying binary..."
docker run --rm --entrypoint /usr/bin/ls "backer:${TAG}" \
    -la /usr/local/bin/backer

echo ""
echo "Build complete: backer:${TAG}"
echo ""
echo "  docker compose run --rm backer backup /data/source /data/backup"
echo "  docker run --rm backer:${TAG} --help"
