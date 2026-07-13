#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════
# setup-testdata.sh — 生成全功能验证测试数据
#
# 📦 覆盖范围:
#   core/      核心备份/还原 — 常规文件、嵌套目录、大文件、空文件
#   fs/       特殊文件 — 软链接、硬链接、FIFO、Socket、设备节点
#   meta/     元数据 — 权限(ugo)、setuid/setgid/sticky、时间戳、属主
#   filter/   6 维筛选 — 路径/类型/名称/时间/大小/属主
#   pack/     打包(Tar+Zip) — 长路径、空格、特殊字符、空目录
#   compress/ 压缩 — 高压缩比(全零)+低压缩比(随机)+混合数据
#   crypto/   加密 — 敏感数据 + 大文件加密测试
#   naming/   特殊命名 — Unicode、emoji、空格、隐藏文件、超长名
#   stress/   压力测试 — 海量小文件、超大目录、极深嵌套
#
# 使用方式:
#   bash scripts/setup-testdata.sh              # 生成所有数据
#   bash scripts/setup-testdata.sh --clean      # 清理数据
#   bash scripts/setup-testdata.sh --dry-run    # 只预览不做
#
# 生成位置: data/source/  (被 .gitignore 忽略)
# ═══════════════════════════════════════════════════════════════════════

set -euo pipefail

error_handler() {
    echo "❌ Error at line ${1}: '${2}' failed with exit code ${3}"
}
trap 'error_handler ${LINENO} "$BASH_COMMAND" $?' ERR

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TARGET_DIR="$BASE_DIR/data/source"

# ── Clean mode ────────────────────────────────────────────────────────
if [[ "${1:-}" == "--clean" ]]; then
    echo "🧹 Cleaning test data..."
    rm -rf "$TARGET_DIR"
    echo "✅ Done — removed $TARGET_DIR"
    exit 0
fi

# ── Dry-run mode ──────────────────────────────────────────────────────
if [[ "${1:-}" == "--dry-run" ]]; then
    echo "📋 Dry run — would generate:"
    echo "  Target: $TARGET_DIR"
    echo "  Estimated items: 250+ files, 30+ dirs, 15+ symlinks, 2+ FIFOs"
    echo "  Total size: ~15 MB"
    exit 0
fi

rm -rf "$TARGET_DIR"
echo ""
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║            Backer — 全功能验证测试数据生成                       ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# ═══════════════════════════════════════════════════════════════════════
# helper — 安全创建文件、符号链接等
# ═══════════════════════════════════════════════════════════════════════
safe_symlink() {
    local target="$1" linkpath="$2"
    ln -sf "$target" "$linkpath" 2>/dev/null || echo "    ⚠ symlink skip: $linkpath"
}
safe_fifo() {
    local path="$1"
    mkfifo "$path" 2>/dev/null || echo "    ⚠ FIFO skip: $path"
}
safe_chmod() {
    local mode="$1" path="$2"
    chmod "$mode" "$path" 2>/dev/null || echo "    ⚠ chmod skip: $path ($mode)"
}
safe_mkdir() {
    mkdir -p "$1"
}
count_files() {
    find "$TARGET_DIR/$1" -type f 2>/dev/null | wc -l
}
count_all() {
    find "$1" ! -name . -prune -type f 2>/dev/null | wc -l
}

# ═══════════════════════════════════════════════════════════════════════
# [core] 核心备份/还原
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [core] 核心备份/还原 ━━━"
safe_mkdir "$TARGET_DIR/core"

# 1) 文本文件（含中文、英文混合）
cat > "$TARGET_DIR/core/hello.txt" << 'EOF'
Hello, Backer! 你好，备份还原工具！
This is a test file for backup and restore functionality.
Line 3: C++17 cross-platform backup tool.
Line 4: 支持中文、English、混合内容。
Line 5: Special characters: !@#$%^&*()_+-=[]{}|;':",./<>?
EOF

# 2) UTF-8 多语言文本
cat > "$TARGET_DIR/core/unicode.txt" << 'EOF'
English: Hello World
中文: 你好世界
日本語: こんにちは世界
한국어: 안녕하세요 세계
Русский: Привет мир
العربية: مرحبا بالعالم
Deutsch: Hallo Welt
Français: Bonjour le monde
EOF

# 3) 空文件
touch "$TARGET_DIR/core/empty.txt"

# 4) 单字节文件
printf '\x7f' > "$TARGET_DIR/core/single_byte.bin"

# 5) 小随机文件 (~1 KB)
dd if=/dev/urandom bs=1024 count=1 2>/dev/null \
    | base64 > "$TARGET_DIR/core/random_1k.txt" 2>/dev/null

# 6) 中等大小文件 (~100 KB)
dd if=/dev/urandom bs=1024 count=100 2>/dev/null \
    | base64 > "$TARGET_DIR/core/random_100k.bin" 2>/dev/null
echo -n "."

# 7) 大文件 (~1 MB)
dd if=/dev/urandom bs=1024 count=1024 2>/dev/null \
    | base64 > "$TARGET_DIR/core/random_1m.bin" 2>/dev/null
echo -n "."

# 8) 超大文件 (~10 MB) — 用于压力测试
dd if=/dev/urandom bs=1024 count=10240 2>/dev/null \
    | base64 > "$TARGET_DIR/core/random_10m.bin" 2>/dev/null
echo -n "."

# 9) 完全可预测的文件（方便校验）
printf 'A%.0s' $(seq 1 1000) > "$TARGET_DIR/core/all_A.txt"
printf '0123456789%.0s' $(seq 1 100) > "$TARGET_DIR/core/pattern.txt"

# 10) 多级嵌套目录（5 层）
safe_mkdir "$TARGET_DIR/core/nested/level1/level2/level3/level4"
echo "level 4 deep" > "$TARGET_DIR/core/nested/level1/level2/level3/level4/leaf.txt"
echo "level 3" > "$TARGET_DIR/core/nested/level1/level2/level3/data.txt"
echo "level 2" > "$TARGET_DIR/core/nested/level1/level2/data.txt"
echo "level 1" > "$TARGET_DIR/core/nested/level1/data.txt"

# 11) 极深嵌套（12 层 — 测试路径深度极限）
safe_mkdir "$TARGET_DIR/core/deep"
dir="$TARGET_DIR/core/deep"
for i in $(seq 1 12); do
    dir="$dir/d$i"
    safe_mkdir "$dir"
done
echo "bottom of 12-level nesting" > "$dir/bottom.txt"

# 12) 批量化同层文件
for i in $(seq 1 10); do
    echo "批量文件 #${i} — 用于测试大量同层文件" > "$TARGET_DIR/core/batch_${i}.txt"
done

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [fs] 特殊文件系统对象
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [fs] 特殊文件系统对象 ━━━"
safe_mkdir "$TARGET_DIR/fs"

# -- 符号链接 --
safe_mkdir "$TARGET_DIR/fs/symlinks"

# 普通符号链接
echo "target file content" > "$TARGET_DIR/fs/symlinks/target.txt"
safe_symlink "target.txt" "$TARGET_DIR/fs/symlinks/good_link.txt"

# 断裂符号链接
safe_symlink "nonexistent_file_does_not_exist" "$TARGET_DIR/fs/symlinks/broken_link"

# 指向目录的链接
safe_mkdir "$TARGET_DIR/fs/symlinks/target_dir"
echo "inside target dir" > "$TARGET_DIR/fs/symlinks/target_dir/file.txt"
safe_symlink "target_dir" "$TARGET_DIR/fs/symlinks/dir_link"

# 符号链接链 (A → B → C)
echo "chain target" > "$TARGET_DIR/fs/symlinks/chain_c.txt"
safe_symlink "chain_c.txt" "$TARGET_DIR/fs/symlinks/chain_b.txt"
safe_symlink "chain_b.txt" "$TARGET_DIR/fs/symlinks/chain_a.txt"

# 指向树外路径的符号链接
safe_symlink "/tmp/backer_outside_target" "$TARGET_DIR/fs/symlinks/outside_tree_link"

# 符号链接 → 符号链接 → 断裂（二级断裂）
safe_symlink "broken_link" "$TARGET_DIR/fs/symlinks/double_broken"

# 自引用循环符号链接（⚠ 注意：遍历时可能死循环）
safe_symlink "self_loop" "$TARGET_DIR/fs/symlinks/self_loop"
echo -n "."

# 符号链接到上级目录
safe_symlink ".." "$TARGET_DIR/fs/symlinks/parent_link"

# -- 硬链接 --
safe_mkdir "$TARGET_DIR/fs/hardlinks"
echo "hardlink source" > "$TARGET_DIR/fs/hardlinks/original.txt"
ln "$TARGET_DIR/fs/hardlinks/original.txt" "$TARGET_DIR/fs/hardlinks/hardlink_1.txt" 2>/dev/null || echo "    ⚠ hardlink skip"
ln "$TARGET_DIR/fs/hardlinks/original.txt" "$TARGET_DIR/fs/hardlinks/hardlink_2.txt" 2>/dev/null || true
echo -n "."

# -- 命名管道 (FIFO) --
safe_mkdir "$TARGET_DIR/fs/pipes"
safe_fifo "$TARGET_DIR/fs/pipes/test_pipe.fifo"
safe_fifo "$TARGET_DIR/fs/pipes/empty_pipe.fifo"
echo -n "."

# -- Socket 文件 --
safe_mkdir "$TARGET_DIR/fs/sockets"
# 真实 socket 文件需要进程绑定，这里创建一个标记文件
echo "socket placeholder — not a real socket" > "$TARGET_DIR/fs/sockets/placeholder.txt"
# 尝试创建真正的 socket 文件（用 python）
python3 -c "
import socket, os
try:
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.bind('$TARGET_DIR/fs/sockets/unix.sock')
    os.chmod('$TARGET_DIR/fs/sockets/unix.sock', 0o777)
    s.close()
except Exception as e:
    print(f'    ⚠ socket skip: {e}')
" 2>/dev/null || echo "    ⚠ socket skip: not supported"
echo -n "."

# -- 设备节点 --
safe_mkdir "$TARGET_DIR/fs/devices"
# 需要 root 权限，尝试但不报错
mknod "$TARGET_DIR/fs/devices/null_dev" c 1 3 2>/dev/null || echo "    ⚠ device node skip (need root)"
echo -n "."

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [meta] 元数据 — 权限、属主、时间戳
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [meta] 元数据 ━━━"
safe_mkdir "$TARGET_DIR/meta"

# -- 各种权限组合 --
echo '#!/bin/bash' > "$TARGET_DIR/meta/executable.sh"
echo 'echo "I am executable"' >> "$TARGET_DIR/meta/executable.sh"
safe_chmod 755 "$TARGET_DIR/meta/executable.sh"

echo "secret key data" > "$TARGET_DIR/meta/private.key"
safe_chmod 600 "$TARGET_DIR/meta/private.key"

echo "world only readable" > "$TARGET_DIR/meta/readonly.txt"
safe_chmod 444 "$TARGET_DIR/meta/readonly.txt"

echo "full open" > "$TARGET_DIR/meta/public.txt"
safe_chmod 777 "$TARGET_DIR/meta/public.txt"

touch "$TARGET_DIR/meta/all_deny.bin"
safe_chmod 000 "$TARGET_DIR/meta/all_deny.bin"

echo "group writable" > "$TARGET_DIR/meta/group_write.txt"
safe_chmod 664 "$TARGET_DIR/meta/group_write.txt"

# -- setuid / setgid --
cp "$TARGET_DIR/meta/executable.sh" "$TARGET_DIR/meta/setuid_test"
safe_chmod 4755 "$TARGET_DIR/meta/setuid_test"

cp "$TARGET_DIR/meta/executable.sh" "$TARGET_DIR/meta/setgid_test"
safe_chmod 2755 "$TARGET_DIR/meta/setgid_test"

cp "$TARGET_DIR/meta/executable.sh" "$TARGET_DIR/meta/setuid_setgid_test"
safe_chmod 6755 "$TARGET_DIR/meta/setuid_setgid_test"

# -- sticky bit 目录 --
safe_mkdir "$TARGET_DIR/meta/sticky_dir"
safe_chmod 1777 "$TARGET_DIR/meta/sticky_dir"
echo "file in sticky dir" > "$TARGET_DIR/meta/sticky_dir/note.txt"

# -- 不同时间戳 --
# 过去
touch -t 202001010000 "$TARGET_DIR/meta/epoch_2020.txt" 2>/dev/null
echo "2020 data" > "$TARGET_DIR/meta/epoch_2020.txt"

# 半年前
touch -t 202601010000 "$TARGET_DIR/meta/six_months_ago.txt" 2>/dev/null
echo "six months ago" > "$TARGET_DIR/meta/six_months_ago.txt"

# 一周前
touch -t "$(date -d '7 days ago' '+%Y%m%d%H%M.%S' 2>/dev/null)" "$TARGET_DIR/meta/one_week_ago.txt" 2>/dev/null
echo "one week ago" > "$TARGET_DIR/meta/one_week_ago.txt"

# 现在
touch "$TARGET_DIR/meta/just_now.txt"
echo "just now" > "$TARGET_DIR/meta/just_now.txt"

# 未来（用于测试 "不备份未来文件" 等场景）
touch -t 202801010000 "$TARGET_DIR/meta/future_file.txt" 2>/dev/null
echo "future dated" > "$TARGET_DIR/meta/future_file.txt"

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [filter] 6 维筛选测试数据
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [filter] 6 维筛选 ━━━"
safe_mkdir "$TARGET_DIR/filter"

# ── 维度 1: 路径筛选 ──
for dir_name in documents images videos music archives code config backup_temp; do
    safe_mkdir "$TARGET_DIR/filter/by_path/$dir_name"
    echo "this is a file in $dir_name" > "$TARGET_DIR/filter/by_path/$dir_name/content.txt"
done
# 隐藏路径
safe_mkdir "$TARGET_DIR/filter/by_path/.secret"
echo "hidden path content" > "$TARGET_DIR/filter/by_path/.secret/credentials.txt"
# 中间隐藏目录
safe_mkdir "$TARGET_DIR/filter/by_path/normal/.hidden_inner/visible_inside"
echo "inside hidden dir" > "$TARGET_DIR/filter/by_path/normal/.hidden_inner/visible_inside/data.txt"
echo -n "."

# ── 维度 2: 文件类型筛选 ──
safe_mkdir "$TARGET_DIR/filter/by_type"
echo "regular text" > "$TARGET_DIR/filter/by_type/regular.txt"
safe_symlink "regular.txt" "$TARGET_DIR/filter/by_type/symlink.lnk"
safe_fifo "$TARGET_DIR/filter/by_type/pipe.fifo"
# 目录类型
safe_mkdir "$TARGET_DIR/filter/by_type/a_directory"
echo -n "."

# ── 维度 3: 名称筛选（glob 匹配）──
safe_mkdir "$TARGET_DIR/filter/by_name"
echo "debug info" > "$TARGET_DIR/filter/by_name/debug.log"
echo "app log" > "$TARGET_DIR/filter/by_name/app.log"
echo "system.log" > "$TARGET_DIR/filter/by_name/system.log"
echo "binary data" > "$TARGET_DIR/filter/by_name/data.bin"
echo "temp cache" > "$TARGET_DIR/filter/by_name/cache.tmp"
echo "backup file" > "$TARGET_DIR/filter/by_name/main.bak"
echo "readme" > "$TARGET_DIR/filter/by_name/README.md"
echo "config" > "$TARGET_DIR/filter/by_name/config.json"
echo "image" > "$TARGET_DIR/filter/by_name/photo.jpg"
echo "source" > "$TARGET_DIR/filter/by_name/main.cpp"
echo "header" > "$TARGET_DIR/filter/by_name/utils.h"
echo "python" > "$TARGET_DIR/filter/by_name/script.py"
echo "markup" > "$TARGET_DIR/filter/by_name/index.html"
echo -n "."

# ── 维度 4: 时间筛选 ──
safe_mkdir "$TARGET_DIR/filter/by_time"
# 创建然后改时间戳
for days_ago in 365 180 90 30 14 7 3 1 0; do
    fname="$TARGET_DIR/filter/by_time/${days_ago}d_ago.txt"
    echo "file modified ${days_ago} days ago" > "$fname"
    touch -t "$(date -d "${days_ago} days ago" '+%Y%m%d%H%M.%S' 2>/dev/null)" "$fname" 2>/dev/null || true
done
echo -n "."

# ── 维度 5: 大小筛选 ──
safe_mkdir "$TARGET_DIR/filter/by_size"
touch "$TARGET_DIR/filter/by_size/zero_byte.bin"                    # 0 B
printf 'a' > "$TARGET_DIR/filter/by_size/one_byte.bin"              # 1 B
printf 'Hello\n' > "$TARGET_DIR/filter/by_size/tiny.txt"              # ~6 B
dd if=/dev/zero bs=1024 count=1 2>/dev/null | tr '\0' 'A' > "$TARGET_DIR/filter/by_size/1k.txt"
dd if=/dev/zero bs=1024 count=10 2>/dev/null | tr '\0' 'B' > "$TARGET_DIR/filter/by_size/10k.txt"
dd if=/dev/zero bs=1024 count=100 2>/dev/null | tr '\0' 'C' > "$TARGET_DIR/filter/by_size/100k.txt"
dd if=/dev/zero bs=1024 count=1024 2>/dev/null | tr '\0' 'D' > "$TARGET_DIR/filter/by_size/1m.txt"
echo -n "."

# ── 维度 6: 属主筛选 ──
safe_mkdir "$TARGET_DIR/filter/by_owner"
touch "$TARGET_DIR/filter/by_owner/current_user.txt"
echo "owned by $(whoami)" > "$TARGET_DIR/filter/by_owner/current_user.txt"
echo -n "."

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [pack] 打包功能测试
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [pack] 打包测试 ━━━"
safe_mkdir "$TARGET_DIR/pack"

# 基础文件
echo "hello pack" > "$TARGET_DIR/pack/hello.txt"
touch "$TARGET_DIR/pack/empty.txt"

# 长路径（Tar ustar 限制 255 字符）
safe_mkdir "$TARGET_DIR/pack/long_path"
lp="$TARGET_DIR/pack/long_path"
for seg in $(seq 1 8); do
    lp="$lp/segment_${seg}_with_extra_text"
    safe_mkdir "$lp"
done
echo "deep path content for tar" > "$TARGET_DIR/pack/long_path_final.txt"

# 含空格路径
safe_mkdir "$TARGET_DIR/pack/my documents"
echo "space in path" > "$TARGET_DIR/pack/my documents/notes.txt"

# 含特殊字符的路径
safe_mkdir "$TARGET_DIR/pack/dir_1_test"
echo "brackets in path" > "$TARGET_DIR/pack/dir_1_test/file.txt"

# 空目录
safe_mkdir "$TARGET_DIR/pack/empty_dir"
safe_mkdir "$TARGET_DIR/pack/nested_empty/level1/level2"

# 符号链接
echo "original pack" > "$TARGET_DIR/pack/original.txt"
safe_symlink "original.txt" "$TARGET_DIR/pack/pack_symlink.txt"

# 大量小文件（测试打包性能）
safe_mkdir "$TARGET_DIR/pack/many_files"
for i in $(seq 1 50); do
    echo "small file number $i with some padding content to make it realistic" \
        > "$TARGET_DIR/pack/many_files/file_$(printf '%03d' $i).txt"
done
echo -n "."

# 文件名长度极限
lname=$(printf 'A%.0s' $(seq 1 240))
echo "long filename test" > "$TARGET_DIR/pack/$lname.txt"

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [compress] 压缩测试
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [compress] 压缩测试 ━━━"
safe_mkdir "$TARGET_DIR/compress"

# 1) 高压缩比 — 全零（gzip/zstd/lzma 应极大缩小）
dd if=/dev/zero bs=1024 count=1024 2>/dev/null > "$TARGET_DIR/compress/all_zeros.bin"
echo -n "."

# 2) 高压缩比 — 重复模式
printf 'ABCDEFGHIJ%.0s' $(seq 1 10000) > "$TARGET_DIR/compress/repeating_pattern.txt"
echo -n "."

# 3) 中等压缩比 — 英文文本+重复
cat > "$TARGET_DIR/compress/english_text.txt" << 'HEREDOC'
This is a sample English text file for compression testing.
It contains repeated words and common patterns that compress well.
The quick brown fox jumps over the lazy dog. The quick brown fox jumps.
Repeated content is good for compression algorithms to work on.
This pattern appears multiple times in this file for better compression.
Lorem ipsum dolor sit amet, consectetur adipiscing elit.
Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.
Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.
HEREDOC
for i in $(seq 1 50); do
    cat "$TARGET_DIR/compress/english_text.txt" >> "$TARGET_DIR/compress/english_large.txt" 2>/dev/null
done
echo -n "."

# 4) 低压缩比 — 随机数据
dd if=/dev/urandom bs=1024 count=512 2>/dev/null > "$TARGET_DIR/compress/random_data.bin"
echo -n "."

# 5) 混合数据（部分可压缩 + 部分不可压缩）
dd if=/dev/zero bs=512 count=256 2>/dev/null > "$TARGET_DIR/compress/mixed.bin"
dd if=/dev/urandom bs=512 count=256 2>/dev/null >> "$TARGET_DIR/compress/mixed.bin"
echo -n "."

# 6) 极小文件（压缩前后对比用）
echo "a" > "$TARGET_DIR/compress/tiny.txt"

# 7) 已经被压缩过的文件（模拟再次压缩）
mkdir -p /tmp/backer_test_compress
gzip -c "$TARGET_DIR/compress/random_data.bin" > "$TARGET_DIR/compress/already_gzip.gz" 2>/dev/null || true
zstd -c "$TARGET_DIR/compress/random_data.bin" > "$TARGET_DIR/compress/already_zst.zst" 2>/dev/null || true
xz -c "$TARGET_DIR/compress/random_data.bin" > "$TARGET_DIR/compress/already_xz.xz" 2>/dev/null || true
echo -n "."

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [crypto] 加密测试数据
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [crypto] 加密测试 ━━━"
safe_mkdir "$TARGET_DIR/crypto"

# 敏感数据风格的内容
cat > "$TARGET_DIR/crypto/credit_card.txt" << 'EOF'
Credit Card Test Data — for encryption testing only:
Card Type: Visa
Card Number: 4111-1111-1111-1111
CVV: 123
Expiry: 12/28
EOF

cat > "$TARGET_DIR/crypto/passwords.txt" << 'EOF'
Password Store — Encrypt me!
Email: user@example.com / Pass: S3cur3P@ss!
Bank: admin / P@ssw0rd123!
SSH Key: -----BEGIN OPENSSH PRIVATE KEY-----
This is test data for encryption module
EOF

# 大文件加密测试
dd if=/dev/urandom bs=1024 count=512 2>/dev/null | base64 > "$TARGET_DIR/crypto/large_secret.bin"
echo -n "."

# 二进制敏感数据
dd if=/dev/urandom bs=64 count=1 2>/dev/null > "$TARGET_DIR/crypto/key_material.bin"
echo -n "."

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [naming] 特殊命名文件
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [naming] 特殊命名 ━━━"
safe_mkdir "$TARGET_DIR/naming"

# 隐藏文件
echo "hidden content" > "$TARGET_DIR/naming/.hidden.txt"
echo "another hidden" > "$TARGET_DIR/naming/.env"
echo "dotfile config" > "$TARGET_DIR/naming/.gitignore"

# 空格
echo "space in name" > "$TARGET_DIR/naming/file with spaces.txt"
echo "leading space" > "$TARGET_DIR/naming/"'   leading_space.txt'

# 中文
echo "中文文件内容" > "$TARGET_DIR/naming/中文文档.txt"
echo "路径测试" > "$TARGET_DIR/naming/测试文件.md"

# 日文 + 韩文
echo "日本語ファイル" > "$TARGET_DIR/naming/日本語の資料.txt"
echo "한국어 파일" > "$TARGET_DIR/naming/한국어_문서.txt"

# 特殊符号
echo "special chars" > "$TARGET_DIR/naming/special!@#\$%^&char.txt"
echo "plus plus" > "$TARGET_DIR/naming/file+name+with+plus.txt"
echo "brackets" > "$TARGET_DIR/naming/file[name](test).txt"
echo "comma, semicolon; file" > "$TARGET_DIR/naming/comma,semicolon;file.txt"
echo "tilde and quotes" > "$TARGET_DIR/naming/file~with\'quotes\".txt"

# emoji
echo "emoji test" > "$TARGET_DIR/naming/🎉release_notes.txt"
echo "emoji dir file" > "$TARGET_DIR/naming/📁project_file.txt"

# 大小写敏感测试
echo "uppercase" > "$TARGET_DIR/naming/UPPER_CASE.TXT"
echo "lowercase" > "$TARGET_DIR/naming/upper_case.txt"

# 点号开头 + 扩展名
echo "config json" > "$TARGET_DIR/naming/.config.json"
echo "local config" > "$TARGET_DIR/naming/config.local.json"
echo "backup copy" > "$TARGET_DIR/naming/config.json.bak"

# 数字开头
echo "number prefix" > "$TARGET_DIR/naming/2024-01-15_report.log"
echo "version" > "$TARGET_DIR/naming/v1.2.3_release.txt"

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [stress] 压力测试
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [stress] 压力测试 ━━━"

# 海量小文件（200 个极小文件）
safe_mkdir "$TARGET_DIR/stress/many_files"
for i in $(seq 1 200); do
    echo "$i" > "$TARGET_DIR/stress/many_files/f_$(printf '%04d' $i).txt"
done
echo -n "."

# 单目录下大量文件
safe_mkdir "$TARGET_DIR/stress/flat_bomb"
for i in $(seq 1 100); do
    printf '%80s' "$i" > "$TARGET_DIR/stress/flat_bomb/flat_$(printf '%05d' $i).bin"
done
echo -n "."

# 混合深度 + 广度
safe_mkdir "$TARGET_DIR/stress/bush"
for branch in $(seq 1 5); do
    bdir="$TARGET_DIR/stress/bush/branch_$branch"
    safe_mkdir "$bdir"
    for leaf in $(seq 1 10); do
        echo "branch ${branch} leaf ${leaf}" > "$bdir/leaf_${leaf}.txt"
    done
done
echo -n "."

# 同名文件在不同目录
for d in dup_a dup_b dup_c; do
    safe_mkdir "$TARGET_DIR/stress/$d"
    echo "same name different dir" > "$TARGET_DIR/stress/$d/duplicate_name.txt"
done
echo -n "."

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# [integration] 综合场景 — 模拟真实项目目录
# ═══════════════════════════════════════════════════════════════════════
echo "━━━ [integration] 综合场景 ━━━"
safe_mkdir "$TARGET_DIR/integration"

# 模拟一个 C++ 项目结构
safe_mkdir "$TARGET_DIR/integration/project/src"
safe_mkdir "$TARGET_DIR/integration/project/include"
safe_mkdir "$TARGET_DIR/integration/project/tests"
safe_mkdir "$TARGET_DIR/integration/project/docs"
safe_mkdir "$TARGET_DIR/integration/project/build"
safe_mkdir "$TARGET_DIR/integration/project/.git"

# 源文件
cat > "$TARGET_DIR/integration/project/src/main.cpp" << 'EOF'
#include <iostream>
#include "backup.h"
int main() { BackupEngine engine; engine.run(); return 0; }
EOF

cat > "$TARGET_DIR/integration/project/include/backup.h" << 'EOF'
#pragma once
class BackupEngine {
public:
    void run();
};
EOF

cat > "$TARGET_DIR/integration/project/tests/test_backup.cpp" << 'EOF'
#include <gtest/gtest.h>
TEST(BackupTest, Basic) { EXPECT_EQ(1, 1); }
EOF

# 编译产物（模拟）
dd if=/dev/urandom bs=1024 count=512 2>/dev/null > "$TARGET_DIR/integration/project/build/backer.o"
echo "executable" > "$TARGET_DIR/integration/project/build/backer-cli"

# 文档
echo "# Project Documentation" > "$TARGET_DIR/integration/project/docs/README.md"
echo "Configuration guide" > "$TARGET_DIR/integration/project/docs/setup.md"

# 图片
printf '\x89PNG\r\n\x1a\n' > "$TARGET_DIR/integration/project/docs/logo.png"
dd if=/dev/urandom bs=64 count=1 2>/dev/null >> "$TARGET_DIR/integration/project/docs/logo.png" || true

# 各种配置文件
printf '#!/usr/bin/env python3\nprint("hello")\n' > "$TARGET_DIR/integration/project/script.py"
printf '#!/bin/bash\necho "build"\n' > "$TARGET_DIR/integration/project/build.sh"
safe_chmod 755 "$TARGET_DIR/integration/project/build.sh"

# Makefile
echo "all: g++ main.cpp -o app" > "$TARGET_DIR/integration/project/Makefile"

# 二进制数据
dd if=/dev/urandom bs=1024 count=2 2>/dev/null > "$TARGET_DIR/integration/project/favicon.ico"

# .gitignore
echo "build/*" > "$TARGET_DIR/integration/project/.gitignore"

# 符号链接
safe_symlink "src/main.cpp" "$TARGET_DIR/integration/project/current_source.cpp"

echo " ✅"

# ═══════════════════════════════════════════════════════════════════════
# 目录摘要
# ═══════════════════════════════════════════════════════════════════════
echo ""
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║  ✅ 测试数据已生成                                               ║"
echo "║                                                                 ║"
echo "║  data/source/                                                   ║"
echo "║  ├── core/        (~$(count_files core) 个文件) 核心备份/还原        ║"
echo "║  │   ├── hello.txt          中文+英文混合文本                      ║"
echo "║  │   ├── unicode.txt        多语言 Unicode 文本                    ║"
echo "║  │   ├── empty.txt          空文件 (0 字节)                       ║"
echo "║  │   ├── single_byte.bin    单字节文件                            ║"
echo "║  │   ├── random_1k.txt      ~1 KB 随机                           ║"
echo "║  │   ├── random_100k.bin    ~100 KB 随机                         ║"
echo "║  │   ├── random_1m.bin      ~1 MB 随机                           ║"
echo "║  │   ├── random_10m.bin     ~10 MB 随机                          ║"
echo "║  │   ├── all_A.txt          完全可预测 (方便校验)                  ║"
echo "║  │   ├── pattern.txt        循环模式                              ║"
echo "║  │   ├── nested/            5 层嵌套目录                          ║"
echo "║  │   ├── deep/              12 层深度嵌套                         ║"
echo "║  │   └── batch_1..10.txt    10 个同层文件                         ║"
fs_obj=$(find "$TARGET_DIR/fs" -type f -o -type l -o -type p 2>/dev/null | wc -l)
echo "║  ├── fs/          (~$fs_obj 个对象) 特殊文件 ║"
echo "║  │   ├── symlinks/  正常/断裂/链式/循环/跨树 符号链接              ║"
echo "║  │   ├── hardlinks/   硬链接 (共享 inode)                        ║"
echo "║  │   ├── pipes/      FIFO 命名管道                               ║"
echo "║  │   ├── sockets/    Unix Domain Socket                         ║"
echo "║  │   └── devices/    字符/块设备节点                             ║"
echo "║  ├── meta/        (~$(count_files meta) 个文件) 元数据              ║"
echo "║  │   ├── executable.sh   755 (可执行)                            ║"
echo "║  │   ├── private.key     600 (私有)                             ║"
echo "║  │   ├── readonly.txt    444 (只读)                             ║"
echo "║  │   ├── public.txt      777 (全开)                             ║"
echo "║  │   ├── all_deny.bin    000 (无权限)                           ║"
echo "║  │   ├── setuid_test     4755 (setuid)                         ║"
echo "║  │   ├── setgid_test     2755 (setgid)                         ║"
echo "║  │   ├── sticky_dir/     1777 (sticky 目录)                     ║"
echo "║  │   └── epoch_*.txt     2020~2028 不同时间戳                    ║"
echo "║  ├── filter/      (~$(count_files filter) 个文件) 6 维筛选          ║"
echo "║  │   ├── by_path/     9 路径分类 + 隐藏目录                       ║"
echo "║  │   ├── by_type/     regular + symlink + fifo                  ║"
echo "║  │   ├── by_name/     13 种扩展名 (.log/.tmp/.bak/...)           ║"
echo "║  │   ├── by_time/     9 个时间维度 (365d~now)                    ║"
echo "║  │   ├── by_size/     0B/1B/6B/1K/10K/100K/1M                  ║"
echo "║  │   └── by_owner/   当前用户文件                                ║"
echo "║  ├── pack/        (~$(count_files pack) 个文件) 打包测试             ║"
echo "║  │   ├── long_path/      8 段深路径                             ║"
echo "║  │   ├── \"my documents\"/  含空格路径                              ║"
echo "║  │   ├── dir_(1)_[test]/ 特殊字符路径                            ║"
echo "║  │   ├── empty_dir/     空目录                                  ║"
echo "║  │   ├── many_files/    50 个小文件                              ║"
echo "║  │   └── 240-char-name  超长文件名                              ║"
echo "║  ├── compress/    (~$(count_files compress) 个文件) 压缩测试         ║"
echo "║  │   ├── all_zeros.bin      全零 (极高压缩比)                     ║"
echo "║  │   ├── repeating_pattern  重复模式 (高压缩比)                   ║"
echo "║  │   ├── english_large.txt  英文文本 (中压缩比)                   ║"
echo "║  │   ├── random_data.bin    随机数据 (低压缩比)                   ║"
echo "║  │   ├── mixed.bin         混合 (部分可压缩)                      ║"
echo "║  │   └── already_*         已压缩文件                            ║"
echo "║  ├── crypto/      (~$(count_files crypto) 个文件) 加密测试          ║"
echo "║  │   ├── credit_card.txt   模拟敏感数据                          ║"
echo "║  │   ├── passwords.txt     密码风格内容                          ║"
echo "║  │   └── large_secret.bin  ~700 KB 加密大文件                     ║"
echo "║  ├── naming/      (~$(count_files naming) 个文件) 特殊命名          ║"
echo "║  │   ├── .hidden           隐藏文件                              ║"
echo "║  │   ├── \"file with spaces\" 含空格                              ║"
echo "║  │   ├── 中文文档           中文/日文/韩文                         ║"
echo "║  │   ├── special!@#        特殊符号                              ║"
echo "║  │   └── 🎉release         Emoji 文件名                          ║"
echo "║  ├── stress/      (~$(count_files stress) 个文件) 压力测试           ║"
echo "║  │   ├── many_files/      200 个极小文件                         ║"
echo "║  │   ├── flat_bomb/       100 个文件单目录                       ║"
echo "║  │   ├── bush/            5×10 混合结构                         ║"
echo "║  │   └── dup_*/           3 目录同名文件                         ║"
echo "║  └── integration/ (~$(count_files integration) 个文件) 模拟项目       ║"
echo "║                                                                 ║"
echo "║  总计: $(find "$TARGET_DIR" -type f 2>/dev/null | wc -l) 个常规文件                     ║"
echo "║        $(find "$TARGET_DIR" -type l 2>/dev/null | wc -l) 个符号链接                     ║"
echo "║        $(find "$TARGET_DIR" -type p 2>/dev/null | wc -l) 个 FIFO 管道                     ║"
echo "║        $(find "$TARGET_DIR" -type s 2>/dev/null | wc -l) 个 Socket                        ║"
echo "║        $(find "$TARGET_DIR" -type d \! -name . 2>/dev/null | wc -l) 个目录                       ║"
echo "║  总大小: $(du -sh "$TARGET_DIR" 2>/dev/null | cut -f1)                        ║"
echo "║                                                                 ║"
echo "║  使用示例:                                                     ║"
echo "║    ./build/backer-cli backup data/source data/backup            ║"
echo "║    ./build/backer-cli restore data/backup data/restore          ║"
echo "║    bash scripts/test-backup-restore.sh                         ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""

# 提醒
echo "💡 数据已生成: $TARGET_DIR"
echo "   清理: bash $0 --clean"
echo ""
