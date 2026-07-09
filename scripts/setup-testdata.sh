#!/bin/bash
# ═══════════════════════════════════════════════════════════════════
# setup-testdata.sh — 生成所有已验证功能的测试数据
#
# 使用方式:
#   bash scripts/setup-testdata.sh            # 生成全部测试数据
#   bash scripts/setup-testdata.sh --clean    # 清理所有生成的测试数据
#
# 目录结构对应 src/ 下的模块:
#   core/     → 核心备份/还原
#   fs/       → 特殊文件 + 元数据
#   filter/   → 6 维度自定义筛选
#   pack/     → 打包 (Tar + Zip)
#   naming/   → 特殊命名文件
#
# 生成位置: data/source/  (被 .gitignore 忽略)
# ═══════════════════════════════════════════════════════════════════
set -euo pipefail

error_handler() {
    echo "❌ Error at line ${1}: '${2}' failed with exit code ${3}"
}
trap 'error_handler ${LINENO} "$BASH_COMMAND" $?' ERR

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_DIR="$BASE_DIR/data/source"

# ── Clean mode ────────────────────────────────────────────────────
if [[ "${1:-}" == "--clean" ]]; then
    echo "🧹 Cleaning test data..."
    rm -rf "$TARGET_DIR"
    echo "✅ Done — removed $TARGET_DIR"
    exit 0
fi

rm -rf "$TARGET_DIR"
echo ""
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║      Generating test data for ALL implemented features      ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo ""

# ═══════════════════════════════════════════════════════════════════
# [core] 核心备份/还原 — 基础目录结构 + 各种常规文件
# ═══════════════════════════════════════════════════════════════════
echo "━━━ [core] 核心备份/还原 ━━━"

# -- 常规文件
mkdir -p "$TARGET_DIR/core"
echo "Hello, Backer! 你好，备份！" > "$TARGET_DIR/core/hello.txt"
touch "$TARGET_DIR/core/empty.txt"
dd if=/dev/urandom bs=1024 count=10 2>/dev/null \
    | base64 > "$TARGET_DIR/core/random_10k.txt"  # ~14 KB
echo "core/hello.txt         ← 中文+英文混合内容"
echo "core/empty.txt         ← 空文件（0 字节）"
echo "core/random_10k.txt    ← ~14 KB 随机内容"

# -- 嵌套目录结构（测试递归遍历）
mkdir -p "$TARGET_DIR/core/nested/a/b"
echo "deeply nested file" > "$TARGET_DIR/core/nested/a/b/leaf.txt"
echo "core/nested/a/b/leaf.txt  ← 3 层嵌套目录"

# -- 多个同层文件（测试批量处理）
for i in $(seq 1 5); do
    echo "file_${i}_content" > "$TARGET_DIR/core/file_${i}.txt"
done
echo "core/file_1..5.txt    ← 5 个同层文件"

echo ""

# ═══════════════════════════════════════════════════════════════════
# [fs-special] 特殊文件 — symlink / FIFO / device / socket
# ═══════════════════════════════════════════════════════════════════
echo "━━━ [fs] 特殊文件 ━━━"

mkdir -p "$TARGET_DIR/special"

# -- 符号链接
echo "symlink_target_content" > "$TARGET_DIR/special/target.txt"
ln -sf "target.txt" "$TARGET_DIR/special/symlink_file.txt"
ln -sf "nonexistent_target" "$TARGET_DIR/special/broken_symlink.txt"
echo "special/symlink_file.txt    ← 正常符号链接 → target.txt"
echo "special/broken_symlink.txt  ← 断裂符号链接 → 不存在的文件"

# -- 命名管道 FIFO（如果支持）
if mkfifo "$TARGET_DIR/special/pipe.fifo" 2>/dev/null; then
    echo "special/pipe.fifo          ← 命名管道 (FIFO)"
else
    echo "special/pipe.fifo          ← (跳过 - 当前环境不支持 FIFO)"
fi

# -- 块/字符设备（仅 root 可创建，尝试但不报错）
if touch "$TARGET_DIR/special/.dev_test" 2>/dev/null; then
    rm -f "$TARGET_DIR/special/.dev_test"
fi
echo "special/ (device nodes) ← (需 root 权限才可创建，跳过)"

# -- Socket 占位文件
echo "socket placeholder (not a real socket)" > "$TARGET_DIR/special/socket_placeholder.txt"
echo "special/socket_placeholder.txt ← socket 占位（非真实 socket）"

echo ""

# ═══════════════════════════════════════════════════════════════════
# [fs-meta] 元数据 — 权限 / 属主 / 时间戳
# ═══════════════════════════════════════════════════════════════════
echo "━━━ [fs] 元数据 ━━━"

mkdir -p "$TARGET_DIR/meta"

# -- 不同权限的文件
echo "#!/bin/bash" > "$TARGET_DIR/meta/executable.sh"
echo "echo 'I am executable'" >> "$TARGET_DIR/meta/executable.sh"
chmod 755 "$TARGET_DIR/meta/executable.sh"
echo "meta/executable.sh     ← 可执行权限 (755)"

umask 177  # 临时设置 umask 确保 600
touch "$TARGET_DIR/meta/private.key" 2>/dev/null || true
chmod 600 "$TARGET_DIR/meta/private.key" 2>/dev/null || true
umask 022  # 恢复默认
echo "private key (should be 600)" > "$TARGET_DIR/meta/private.key"
echo "meta/private.key       ← 私有权限 (600)"

echo "world readable" > "$TARGET_DIR/meta/world_readable.txt"
chmod 444 "$TARGET_DIR/meta/world_readable.txt" 2>/dev/null || true
echo "meta/world_readable.txt ← 只读权限 (444)"

# -- sticky bit 目录
mkdir -p "$TARGET_DIR/meta/sticky_dir"
chmod 1777 "$TARGET_DIR/meta/sticky_dir" 2>/dev/null || true
echo "meta/sticky_dir/       ← sticky bit 目录 (1777)"

# -- setuid/setgid 模拟
cp "$TARGET_DIR/meta/executable.sh" "$TARGET_DIR/meta/setuid_test"
chmod 4755 "$TARGET_DIR/meta/setuid_test" 2>/dev/null || true
echo "meta/setuid_test       ← setuid 权限 (4755)"

echo ""

# ═══════════════════════════════════════════════════════════════════
# [filter] 6 维度自定义筛选
#   测试: --include-path / --exclude-path
#         --include-type / --exclude-type
#         --include-name / --exclude-name
#         --mtime-before / --mtime-after
#         --size-min / --size-max
#         --owner
# ═══════════════════════════════════════════════════════════════════
echo "━━━ [filter] 6 维度筛选 ━━━"

mkdir -p "$TARGET_DIR/filter"

# 维度 1: 路径筛选 — 不同路径下的文件
mkdir -p "$TARGET_DIR/filter/documents"
mkdir -p "$TARGET_DIR/filter/images"
mkdir -p "$TARGET_DIR/filter/logs"
mkdir -p "$TARGET_DIR/filter/temp"
mkdir -p "$TARGET_DIR/filter/.hidden_dir"

for i in 1 2 3; do
    echo "document $i content" > "$TARGET_DIR/filter/documents/doc_${i}.txt"
done
echo "filter/documents/      ← 3 个 .txt 文档"

for i in 1 2; do
    # 生成小型 PNG 魔数文件（不是真图片，但有正确魔数头）
    printf '\x89PNG\r\n\x1a\n' > "$TARGET_DIR/filter/images/photo_${i}.png"
    dd if=/dev/urandom bs=64 count=1 2>/dev/null >> "$TARGET_DIR/filter/images/photo_${i}.png" || true
done
echo "filter/images/         ← 2 个 .png 文件"

echo "$(date) - system log entry" > "$TARGET_DIR/filter/logs/system.log"
echo "$(date) - debug output" > "$TARGET_DIR/filter/logs/debug.log"
echo "filter/logs/           ← 2 个 .log 文件"

echo "temporary cache data" > "$TARGET_DIR/filter/temp/cache.tmp"
echo "filter/temp/           ← .tmp 临时文件"

echo "hidden config" > "$TARGET_DIR/filter/.hidden_dir/config.ini"
echo "filter/.hidden_dir/    ← 隐藏目录"

# 维度 2: 文件类型筛选 — 不同 FileType
mkdir -p "$TARGET_DIR/filter/types"
echo "regular content" > "$TARGET_DIR/filter/types/regular.txt"
ln -sf "regular.txt" "$TARGET_DIR/filter/types/symlink.lnk" 2>/dev/null || true
if mkfifo "$TARGET_DIR/filter/types/pipe.fifo" 2>/dev/null; then
    echo "filter/types/pipe.fifo  ← FIFO 管道"
fi
echo "filter/types/          ← 含 regular + symlink + fifo"

# 维度 3: 名称筛选 — glob 通配匹配
mkdir -p "$TARGET_DIR/filter/names"
echo "debug info" > "$TARGET_DIR/filter/names/debug.log"
echo "app log" > "$TARGET_DIR/filter/names/app.log"
echo "data" > "$TARGET_DIR/filter/names/data.bin"
echo "cache" > "$TARGET_DIR/filter/names/cache.tmp"
echo "readme" > "$TARGET_DIR/filter/names/README.md"
echo "filter/names/          ← debug.log / app.log / data.bin / cache.tmp / README.md"

# 维度 4: 时间筛选 — 修改时间不同的文件
mkdir -p "$TARGET_DIR/filter/times"
touch -t 202401010000 "$TARGET_DIR/filter/times/old_file.txt" 2>/dev/null || true
touch -t 202406010000 "$TARGET_DIR/filter/times/recent_file.txt" 2>/dev/null || true
touch "$TARGET_DIR/filter/times/current_file.txt"
echo "old content" > "$TARGET_DIR/filter/times/old_file.txt"
echo "recent content" > "$TARGET_DIR/filter/times/recent_file.txt"
echo "current content" > "$TARGET_DIR/filter/times/current_file.txt"
echo "filter/times/          ← 2024-01 / 2024-06 / 今天 三种时间"

# 维度 5: 尺寸筛选 — 不同大小的文件
mkdir -p "$TARGET_DIR/filter/sizes"
echo "small" > "$TARGET_DIR/filter/sizes/tiny.txt"                    # ~6 B
dd if=/dev/zero bs=1024 count=1 2>/dev/null | tr '\0' 'A' > "$TARGET_DIR/filter/sizes/1k.txt" 2>/dev/null || echo "1KB file" > "$TARGET_DIR/filter/sizes/1k.txt"
dd if=/dev/zero bs=1024 count=100 2>/dev/null | tr '\0' 'B' > "$TARGET_DIR/filter/sizes/100k.txt" 2>/dev/null || echo "100KB file" > "$TARGET_DIR/filter/sizes/100k.txt"
echo "filter/sizes/          ← tiny(6B) / 1k(1KB) / 100k(100KB)"

# 维度 6: 属主筛选 — 当前用户文件
mkdir -p "$TARGET_DIR/filter/owners"
touch "$TARGET_DIR/filter/owners/current_user.txt"
echo "owned by $(whoami)" > "$TARGET_DIR/filter/owners/current_user.txt"
echo "filter/owners/         ← 属主: $(whoami)"

echo ""

# ═══════════════════════════════════════════════════════════════════
# [pack] 打包/解包 — Tar + Zip
#   测试: --pack tar / --pack zip
#   包含各种文件类型确保打包格式正确
# ═══════════════════════════════════════════════════════════════════
echo "━━━ [pack] 打包/解包 ━━━"

mkdir -p "$TARGET_DIR/pack"

# -- 不同内容、不同大小的文件
echo "Hello from pack test" > "$TARGET_DIR/pack/hello.txt"
touch "$TARGET_DIR/pack/empty.txt"
dd if=/dev/urandom bs=512 count=4 2>/dev/null \
    | base64 > "$TARGET_DIR/pack/medium.bin" 2>/dev/null || echo "medium content" > "$TARGET_DIR/pack/medium.bin"

# -- 嵌套目录
mkdir -p "$TARGET_DIR/pack/multi_level/deep/path"
echo "very deep" > "$TARGET_DIR/pack/multi_level/deep/path/data.txt"

# -- 空目录
mkdir -p "$TARGET_DIR/pack/empty_dir"

# -- 特殊字符文件名
echo "brackets content" > "$TARGET_DIR/pack/special_chars_[test].txt"
echo "plus content" > "$TARGET_DIR/pack/special_chars_+test+.txt"

# -- 符号链接（打包时需要正确处理）
echo "original" > "$TARGET_DIR/pack/original.txt"
ln -sf "original.txt" "$TARGET_DIR/pack/pack_symlink.txt" 2>/dev/null || true

echo "pack/hello.txt              ← 文本文件"
echo "pack/empty.txt              ← 空文件"
echo "pack/medium.bin             ← ~3 KB 二进制"
echo "pack/multi_level/.../       ← 4 层嵌套目录"
echo "pack/empty_dir/             ← 空目录"
echo "pack/special_chars_[...].txt ← 特殊字符文件名"
echo "pack/pack_symlink.txt       ← 符号链接"

echo ""

# ═══════════════════════════════════════════════════════════════════
# [naming] 特殊命名 — 空格 / 中文 / 隐藏文件
# ═══════════════════════════════════════════════════════════════════
echo "━━━ [naming] 特殊命名 ━━━"

mkdir -p "$TARGET_DIR/naming"

echo "hidden file content" > "$TARGET_DIR/naming/.hidden.txt"
echo "file name with spaces" > "$TARGET_DIR/naming/file with spaces.txt"
echo "中文文件内容" > "$TARGET_DIR/naming/中文文档.txt"
echo "special!@# char" > "$TARGET_DIR/naming/special!@#char.txt"
echo "UPPER_case.TXT" > "$TARGET_DIR/naming/UPPER_case.TXT"

echo "naming/.hidden.txt          ← 点开头隐藏文件"
echo "naming/file with spaces.txt ← 含空格文件名"
echo "naming/中文文档.txt          ← 中文文件名"
echo "naming/special!@#char.txt   ← 特殊符号"
echo "naming/UPPER_case.TXT       ← 大写文件名"

echo ""

# ═══════════════════════════════════════════════════════════════════
# 摘要
# ═══════════════════════════════════════════════════════════════════
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║  ✅ 测试数据已生成: $TARGET_DIR"
echo "║"
echo "║  模块        文件数    用途"
echo "║  ─────────────────────────────────────────────────"
echo "║  core/       $(find "$TARGET_DIR/core" -type f 2>/dev/null | wc -l) 个     核心备份/还原"
echo "║  special/    $(find "$TARGET_DIR/special" -type f -o -type l -o -type p 2>/dev/null | wc -l) 个     特殊文件"
echo "║  meta/       $(find "$TARGET_DIR/meta" -type f -o -type d 2>/dev/null | wc -l) 个     元数据"
echo "║  filter/     $(find "$TARGET_DIR/filter" -type f 2>/dev/null | wc -l) 个     6 维筛选"
echo "║  pack/       $(find "$TARGET_DIR/pack" -type f -o -type l 2>/dev/null | wc -l) 个     打包/解包"
echo "║  naming/     $(find "$TARGET_DIR/naming" -type f 2>/dev/null | wc -l) 个     特殊命名"
echo "║  ─────────────────────────────────────────────────"
echo "║  总计: $(find "$TARGET_DIR" -type f 2>/dev/null | wc -l) 个文件"
echo "║"
echo "║  使用示例:"
echo "║    ./build/backer-cli backup data/source data/backup"
echo "║    ./build/backer-cli restore data/backup data/restore"
echo "║    ./build/backer-cli backup data/source data/backup --pack tar"
echo "║    ./build/backer-cli backup data/source data/backup --exclude-name \"*.log\""
echo "╚══════════════════════════════════════════════════════════════╝"
