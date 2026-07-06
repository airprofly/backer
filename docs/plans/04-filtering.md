# 自定义备份筛选 — 实施计划

## 功能概述

允许用户通过多种条件筛选需要备份的文件，支持按路径、文件类型、文件名、修改时间、文件尺寸、属主等维度进行包含/排除。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（自定义备份 / 各 3 分）。

## 技术方案

### 筛选条件结构

```cpp
struct FilterCriteria {
    std::optional<std::regex>   pathPattern;     // 路径匹配 (正则)
    std::optional<std::string>  fileType;        // 文件类型 (file/dir/symlink/fifo/...)
    std::optional<std::string>  namePattern;     // 文件名通配 (glob)
    std::optional<TimeRange>    timeRange;       // 修改时间范围
    std::optional<SizeRange>    sizeRange;       // 文件尺寸范围
    std::optional<std::string>  userName;        // 用户名 (通过 UID 查询)
    bool                        exclude = false; // true=排除匹配项, false=仅包含匹配项
    CombineMode                 combine = CombineMode::kAnd; // 多条件组合方式 And/Or
};

enum class CombineMode {
    kAnd,  // 所有条件都满足
    kOr,   // 任一条件满足
};
```

### 筛选器管道

```
原始文件列表
    │
    ▼
┌──────────────────────────┐
│  Include 筛选器（链式）   │  ← 保留匹配项
│  ├─ 路径匹配              │
│  ├─ 类型匹配              │
│  ├─ 时间匹配              │
│  └─ 尺寸匹配              │
└──────────┬───────────────┘
           ▼
┌──────────────────────────┐
│  Exclude 筛选器（链式）   │  ← 移除匹配项
│  ├─ 路径排除              │
│  ├─ 名称排除              │
│  └─ 类型排除              │
└──────────┬───────────────┘
           ▼
      最终文件列表
```

### 筛选维度详表

| 维度 | CLI 参数（与 architecture-design.md §11 一致） | 实现方式 | 分值 |
|------|--------------------------------------------------|----------|------|
| 路径 | `--include-path <glob>` / `--exclude-path <glob>` | `fnmatch()` / `std::regex` | 3 |
| 类型 | `--include-type <type>` / `--exclude-type <type>` | FileType 枚举匹配 | 3 |
| 名字 | `--include-name <glob>` / `--exclude-name <glob>` | `fnmatch()` / glob | 3 |
| 时间 | `--mtime-before <time>` / `--mtime-after <time>` | `timespec` 比较 | 3 |
| 尺寸 | `--size-min <bytes>` / `--size-max <bytes>` | 数值比较 | 3 |
| 用户 | `--owner <name>` | `getpwnam_r()` 查询 | 3 |

### 默认行为

- 无筛选条件时：备份所有文件（`NoopFilter`）
- 多个 include 条件同时指定时：默认 AND 组合
- exclude 优先级高于 include：先 include 筛选，再 exclude 移除

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. FilterCriteria 结构体 | 定义筛选条件数据结构 | `filter.h` |
| 2. Filter 接口 + NoopFilter | 基类 + 默认空筛选器 | `filter.h` |
| 3. 路径/名字筛选 | glob 匹配实现 | `criteria_filter.cpp` |
| 4. 文件类型筛选 | FileType 枚举匹配 | `criteria_filter.cpp` |
| 5. 时间范围筛选 | timespec 比较实现 | `criteria_filter.cpp` |
| 6. 文件尺寸筛选 | 数值范围比较 | `criteria_filter.cpp` |
| 7. 用户筛选 | `getpwnam_r()` UID 查询 + 匹配 | `criteria_filter.cpp` |
| 8. 组合逻辑 | AND/OR 组合多条件 | `criteria_filter.cpp` |
| 9. CLI 参数集成 | 6 组 CLI 参数映射到 FilterCriteria | `commands.cpp` |
| 10. 单元测试 | 每种筛选条件的独立测试 + 组合测试 | 测试文件 |

## 关键接口

```cpp
class Filter {
public:
    virtual ~Filter() = default;
    virtual std::vector<FileEntry> apply(
        std::vector<FileEntry> const& files) = 0;
};

class CriteriaFilter : public Filter {
public:
    explicit CriteriaFilter(std::vector<FilterCriteria> criteria);
    std::vector<FileEntry> apply(std::vector<FileEntry> const& files) override;
};

// CLI 参数解析辅助
FilterCriteria parsePathFilter(std::string const& pattern, bool exclude);
FilterCriteria parseTypeFilter(std::string const& typeName, bool exclude);
FilterCriteria parseTimeFilter(std::string const& before, std::string const& after);
FilterCriteria parseSizeFilter(uint64_t min, uint64_t max);
FilterCriteria parseUserFilter(std::string const& userName);
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 路径筛选 | `--include-path "*.cpp"` | 仅 `.cpp` 文件通过 |
| 类型筛选 | `--exclude-type symlink` | 符号链接被排除 |
| 时间筛选 | `--mtime-after "2026-01-01"` | 旧文件被排除 |
| 尺寸筛选 | `--size-min 1024 --size-max 1048576` | 范围外文件被排除 |
| 用户筛选 | `--owner alice` | 非 alice 文件被排除 |
| AND 组合 | 路径 + 类型 + 时间同时筛选 | 三条件交集 |
| 空结果 | 筛选条件过于严格导致无文件 | 警告日志，不崩溃 |

## 关键文件

```
src/filters/
├── filter.h                # Filter 接口
├── criteria_filter.h
└── criteria_filter.cpp
src/cli/
└── commands.cpp            # 新增筛选参数解析
tests/
└── filter/
    └── criteria_filter_test.cpp
```

## 预计工作量

- 代码行数：~700 行（含测试）
- 开发周期：4-6 天
- 依赖：`<regex>`, `<fnmatch.h>`, `<pwd.h>`
