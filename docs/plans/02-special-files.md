# 特殊文件支持 — 实施计划

## 功能概述

支持 Linux 特殊文件类型的备份与还原，包括符号链接、命名管道 (FIFO)、块设备、字符设备和 Socket。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（特殊文件支持 / 10 分）。

## 技术方案

### 文件类型检测

使用 `lstat()` 系统调用获取文件类型信息，通过 `st_mode` 判断：

```cpp
FileType detectFileType(struct stat const& st) {
    if (S_ISREG(st.st_mode))  return FileType::kRegular;
    if (S_ISDIR(st.st_mode))  return FileType::kDirectory;
    if (S_ISLNK(st.st_mode))  return FileType::kSymlink;
    if (S_ISFIFO(st.st_mode)) return FileType::kFifo;
    if (S_ISBLK(st.st_mode))  return FileType::kBlockDevice;
    if (S_ISCHR(st.st_mode))  return FileType::kCharDevice;
    if (S_ISSOCK(st.st_mode)) return FileType::kSocket;
    return FileType::kUnknown;
}
```

### 处理策略

| 文件类型 | 备份方式 | 还原方式 |
|----------|----------|----------|
| 符号链接 | `readlink()` 读目标路径 | `symlink()` 重建 |
| 命名管道 (FIFO) | 存元数据，不读内容 | `mkfifo()` + 设置权限 |
| 块设备 | `stat.st_rdev` 取设备号 | `mknod()` + 设备号 |
| 字符设备 | `stat.st_rdev` 取设备号 | `mknod()` + 设备号 |
| Socket | 存元数据 + 标记位 | `mknod()` + S_IFSOCK（仅占位） |

### 权限检查

- 特殊文件操作需要 `CAP_MKNOD` 权限（默认 root）
- 非 root 运行时：检测到特殊文件则跳过，记录警告日志，不中断备份
- 交互提示：`--skip-special` 选项支持跳过/强制备份

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. FileType 枚举 | 定义文件类型枚举 + 检测函数 | `special_file.h` |
| 2. 符号链接处理 | `readlink()` + `symlink()` 实现 | `fs_abstraction.cpp` |
| 3. FIFO 处理 | `mkfifo()` + 权限设置 | `special_file.cpp` |
| 4. 设备文件处理 | `mknod()` + major/minor 号解析 | `special_file.cpp` |
| 5. Socket 处理 | 元数据记录 + 占位重建 | `special_file.cpp` |
| 6. 集成到 FSAbstraction | 修改 `walk()` 和 `write()` 方法 | `fs_abstraction.cpp` |
| 7. CLI 选项 | `--handle-special` / `--skip-special` | `commands.cpp` |
| 8. 权限降级处理 | 非 root 运行时的降级策略 | 错误处理 |
| 9. 单元测试 | 创建各类特殊文件 → 备份 → 还原 → 验证 | 测试文件 |

## 关键接口

```cpp
enum class FileType : uint8_t {
    kRegular,
    kDirectory,
    kSymlink,
    kFifo,
    kBlockDevice,
    kCharDevice,
    kSocket,
    kUnknown
};

// 特殊文件处理
class SpecialFileHandler {
    FileType detect(std::filesystem::path const& path);
    std::expected<std::string, ErrorCode> readSymlink(std::filesystem::path const& path);
    std::expected<void, ErrorCode> createSymlink(std::filesystem::path const& link, std::string const& target);
    std::expected<void, ErrorCode> createFifo(std::filesystem::path const& path, mode_t mode);
    std::expected<void, ErrorCode> createDevice(std::filesystem::path const& path,
                                                FileType type, dev_t deviceNum, mode_t mode);
};
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 符号链接 | 备份含链接的目录 → 还原 → 目标正确 | `readlink()` 结果一致 |
| FIFO 管道 | 备份含 FIFO 的目录 → 还原 | `S_ISFIFO` 属性存在 |
| 块/字符设备 | `/dev/null`, `/dev/zero` 备份还原 | major/minor 号一致 |
| 非 root 运行 | 跳过特殊文件 | 日志记录+不崩溃 |
| 悬空符号链接 | 指向不存在的目标 | 还原后仍为悬空链接 |

## 关键文件

```
src/fs/
├── special_file.h         # FileType 枚举 + SpecialFileHandler
├── special_file.cpp
├── fs_abstraction.h       # 修改 walk/write 支持特殊文件
└── fs_abstraction.cpp
tests/fs/
└── special_file_test.cpp
```

## 预计工作量

- 代码行数：~500 行（含测试）
- 开发周期：3-5 天
- 依赖：`<sys/stat.h>`, `<unistd.h>`, `<sys/sysmacros.h>`
