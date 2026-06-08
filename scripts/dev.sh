#!/usr/bin/env bash
# dev.sh — 开发辅助脚本：RP2040 (PlatformIO) + ESP32-S3 (ESP-IDF)
# 支持级联命令：./scripts/dev.sh esp build flash monitor
set -euo pipefail

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RP2040_DIR="$PROJ_ROOT/rp2040"
IDF_EXPORT="$HOME/esp/v5.4.3/esp-idf/export.sh"
ESP_TARGET="esp32s3"

# ─── 颜色 ────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { echo -e "${GREEN}[INFO]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*"; }
die()   { echo -e "${RED}[ERROR]${NC} $*" >&2; exit 1; }

# ─── ESP-IDF 环境 ────────────────────────────────────────────
getidf() {
    if command -v idf.py &>/dev/null; then
        return 0
    fi
    [[ -f "$IDF_EXPORT" ]] || die "未找到 ESP-IDF: $IDF_EXPORT"
    info "加载 ESP-IDF 环境..."
    # shellcheck disable=SC1090
    source "$IDF_EXPORT"
    info "ESP-IDF $(idf.py --version) 就绪"
}

# ─── ESP32-S3: 级联执行 ──────────────────────────────────────
# 收集所有动作，拼成一条 idf.py 命令，由 idf.py 自己处理级联
esp_run() {
    local port="" actions=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -p|--port)  port="$2"; shift 2 ;;
            build|flash|monitor|clean|fullclean|menuconfig|set-target)
                actions+=("$1"); shift ;;
            *)  die "ESP: 未知动作 '$1'" ;;
        esac
    done

    [[ ${#actions[@]} -gt 0 ]] || die "ESP: 未指定动作（build/flash/monitor/...）"

    getidf
    cd "$PROJ_ROOT"

    # 确保 target 已设置
    idf.py -B build set-target "$ESP_TARGET" 2>/dev/null || true

    local cmd=(idf.py -B build)
    [[ -n "$port" ]] && cmd+=(-p "$port")
    cmd+=("${actions[@]}")

    info "ESP32-S3: ${actions[*]}${port:+ (端口: $port)}"
    "${cmd[@]}"
}

# ─── RP2040: 级联执行 ─────────────────────────────────────────
# pio 不支持级联，逐个执行
rp_run() {
    local actions=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            build|flash|monitor|clean)
                actions+=("$1"); shift ;;
            *)  die "RP2040: 未知动作 '$1'" ;;
        esac
    done

    [[ ${#actions[@]} -gt 0 ]] || die "RP2040: 未指定动作（build/flash/monitor/clean）"

    for act in "${actions[@]}"; do
        case "$act" in
            build)
                info "RP2040: 编译..."
                pio run -d "$RP2040_DIR" ;;
            flash)
                info "RP2040: 烧录..."
                pio run -d "$RP2040_DIR" -t upload ;;
            monitor)
                info "RP2040: 串口监视器 (Ctrl+C 退出)"
                pio device monitor -d "$RP2040_DIR" ;;
            clean)
                info "RP2040: 清理..."
                pio run -d "$RP2040_DIR" -t clean ;;
        esac
    done
}

# ─── 使用帮助 ─────────────────────────────────────────────────
usage() {
    cat <<'EOF'
用法: ./scripts/dev.sh <目标> <动作...> [选项]

支持级联多个动作，按顺序执行：

  ./scripts/dev.sh esp build flash monitor
  ./scripts/dev.sh rp  build flash
  ./scripts/dev.sh build flash monitor        # 省略目标 = esp

目标:
  esp               ESP32-S3 (ESP-IDF)
  rp                RP2040 (PlatformIO)
  省略              默认 esp

ESP 动作:
  build             编译
  flash             烧录
  monitor           串口监视器 (Ctrl+])
  clean             清理构建
  fullclean         全量清理（含 sdkconfig）
  menuconfig        打开 menuconfig

RP2040 动作:
  build             编译
  flash             烧录
  monitor           串口监视器 (Ctrl+C)
  clean             清理构建

选项:
  -p, --port <端口>  指定串口端口（仅 ESP）

示例:
  ./scripts/dev.sh esp build                  # 仅编译
  ./scripts/dev.sh esp flash monitor          # 烧录后自动开监视器
  ./scripts/dev.sh esp build flash monitor    # 完整流程
  ./scripts/dev.sh esp flash -p /dev/cu.xxx   # 指定端口烧录
  ./scripts/dev.sh rp flash monitor           # RP2040 烧录+监视
  ./scripts/dev.sh build flash monitor        # 快捷写法 = esp
  ./scripts/dev.sh monitor                    # 快捷写法 = esp monitor
EOF
}

# ─── 命令分发 ─────────────────────────────────────────────────
case "${1:-}" in
    esp)    shift; esp_run "$@" ;;
    rp)     shift; rp_run "$@" ;;
    -h|--help|help|"")
            usage ;;
    # 无目标前缀 → 默认 esp
    build|flash|monitor|clean|fullclean|menuconfig|set-target|-p|--port)
            esp_run "$@" ;;
    *)      die "未知命令: $1（用 --help 查看帮助）" ;;
esac
