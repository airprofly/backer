#!/usr/bin/env bash
# setup-testdata.sh — 特殊文件类型测试数据
# 覆盖 requirements.md: 链接/管道/设备

set -euo pipefail

BASE_DIR="${1:-data}"
SOURCE_DIR="${BASE_DIR}/source"
BACKUP_DIR="${BASE_DIR}/backup"
RESTORE_DIR="${BASE_DIR}/restore"

rm -rf "${BASE_DIR}"
mkdir -p "${SOURCE_DIR}" "${BACKUP_DIR}" "${RESTORE_DIR}"

echo "=== 创建测试数据 ==="

# 普通文件
echo "普通文件" > "${SOURCE_DIR}/doc.txt"

# 子目录
mkdir -p "${SOURCE_DIR}/sub"
echo "文件" > "${SOURCE_DIR}/sub/file.txt"

# 软链接
ln -sf "sub/file.txt" "${SOURCE_DIR}/symlink"

# 设置不同修改时间，供 --mtime-* 筛选演示
touch -t 202507011200.00 "${SOURCE_DIR}/sub/file.txt"  # 设为旧文件（2025-07-01 12:00:00）
touch -t 202507011200.00 "${SOURCE_DIR}/sub"           # 目录时间一并设置

# 管道 (FIFO) 和 设备文件
mkfifo "${SOURCE_DIR}/fifo" 2>/dev/null || true
mknod "${SOURCE_DIR}/device_file" c 1 3 2>/dev/null || true
mknod "${SOURCE_DIR}/block_device" b 8 0 2>/dev/null || true

echo "完成! 文件: $(find "${SOURCE_DIR}" -type f | wc -l)"
echo "使用: ./build/backer-cli backup ${SOURCE_DIR} ${BACKUP_DIR}"
