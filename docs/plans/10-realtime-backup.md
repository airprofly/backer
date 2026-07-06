# 实时备份 — 实施计划

## 功能概述

自动感知用户文件变化（创建、修改、删除、重命名），自动触发增量或全量备份，实现文件的实时保护。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（实时备份 / 15 分）。

## 技术方案

### 架构设计

```
应用层: inotify (IN_CREATE|IN_MODIFY|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO)
    │
    ▼
┌────────────────────────────────────────────┐
│              FileWatcher                    │
│                                             │
│  事件队列 ← inotify FD (epoll_wait)         │
│      │                                      │
│      ▼                                      │
│  ┌──────────┐     ┌──────────────┐          │
│  │  Event    │  →  │  Debouncer   │          │
│  │  Queue    │     │  (合并去重)   │          │
│  └──────────┘     └──────┬───────┘          │
│                          │                   │
│                          ▼                   │
│  ┌──────────────────────────────┐            │
│  │       BackupWorker           │            │
│  │   (后台线程: 增量备份/全量)    │            │
│  └──────────────────────────────┘            │
└────────────────────────────────────────────┘
```

### inotify 事件监听

```cpp
// 注册的事件类型
constexpr uint32_t kWatchEvents =
    IN_CREATE | IN_MODIFY | IN_DELETE |
    IN_MOVED_FROM | IN_MOVED_TO |
    IN_ATTRIB | IN_CLOSE_WRITE;
```

| 事件 | 含义 | 触发动作 |
|------|------|---------|
| IN_CLOSE_WRITE | 文件写入完毕关闭 | 增量备份该文件 |
| IN_CREATE | 创建文件/目录 | 备份新文件 |
| IN_DELETE | 删除文件 | 记录删除日志 |
| IN_MOVED_FROM/TO | 文件重命名/移动 | 更新索引 |
| IN_ATTRIB | 属性变化 | 更新元数据 |

### 目录监控策略

```
watchedDir/
├── dir1/                  ← 递归添加 watch (IN_CREATE 时自动 add watch)
├── dir2/
│   ├── file1.txt          ← 文件变更事件
│   └── subdir/            ← 新增子目录 → 自动 add watch
└── file2.jpg
```

- 对监控的根目录添加 `IN_CREATE` 事件
- 新创建的子目录自动添加 `inotify_add_watch()`
- 删除的目录自动移除 `inotify_rm_watch()`
- 使用 `IN_ONLYDIR` 选项确保只监控目录

### 去抖机制 (Debounce)

高频修改（如 IDE 自动保存）场景下，合并批量事件为一次备份：

```
时间线:
file1.txt MODIFY ─┐
file1.txt MODIFY ─┤
file2.jpg CREATE ─┤    ┌───────────┐    ┌───────────┐
file2.jpg MODIFY ─┼──→ │ 去抖窗口  │──→ │ 1 次触发   │
                   │    │ (1 秒)    │    │ 增量备份    │
                   │    └───────────┘    └───────────┘
                   └──── 时间窗口 ────→
```

### 事件队列实现

```cpp
class Debouncer {
public:
    explicit Debouncer(std::chrono::milliseconds window = 1000ms);

    // 添加事件，返回是否需要触发备份
    bool addEvent(FileEvent event);

    // 获取去重后的事件列表
    std::vector<FileEvent> flushEvents();

    // 重置去抖窗口
    void reset();

private:
    std::chrono::milliseconds window_;
    std::unordered_set<std::string> changedFiles_;  // 去重
    std::chrono::steady_clock::time_point lastFlush_;
};
```

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. inotify 初始化 | `inotify_init1(IN_CLOEXEC)` + epoll | `file_watcher.h/cpp` |
| 2. 目录递归监控 | `inotify_add_watch` 递归添加目录 | `file_watcher.cpp` |
| 3. 事件读取 | `read()` inotify 事件 → 解析结构体 | `file_watcher.cpp` |
| 4. 事件队列 | 线程安全的事件队列 | `event_queue.h/cpp` |
| 5. 去抖模块 | 时间窗口合并去重 | `debouncer.h/cpp` |
| 6. 自动触发备份 | 去抖后调用增量备份 | `file_watcher.cpp` |
| 7. 删除/重命名处理 | 对应事件的正确处理 | `file_watcher.cpp` |
| 8. 最大 inotify watch 限制 | `/proc/sys/fs/inotify/max_user_watches` | 配置 + 警告 |
| 9. CLI 集成 | `backer watch <source> <destination>` | `commands.cpp` |
| 10. 单元测试 | 模拟文件变更事件 | 测试文件 |

## 关键接口

```cpp
struct FileEvent {
    std::filesystem::path path;     // 变更文件路径
    EventType type;                 // Created / Modified / Deleted / Renamed
    std::chrono::system_clock::time_point timestamp;
};

enum class EventType {
    kCreated,
    kModified,
    kDeleted,
    kRenamed,
    kAttrChanged
};

class FileWatcher {
public:
    explicit FileWatcher(std::chrono::milliseconds debounceMs = 1000);

    // 开始监控（阻塞，应在独立线程中运行）
    std::expected<void, ErrorCode> watch(std::filesystem::path const& dir);

    // 停止监控
    void stop();

    // 设置回调（去抖后触发）
    void setOnChange(std::function<void(std::vector<FileEvent> const&)> callback);

    // 设置去抖窗口
    void setDebounce(std::chrono::milliseconds ms);

private:
    void addWatchRecursive(std::filesystem::path const& dir);
    void handleEvents();
    void processBatch(std::vector<FileEvent> events);

    int inotifyFd_;
    int epollFd_;
    std::atomic<bool> running_{false};
    std::unique_ptr<Debouncer> debouncer_;
    std::unordered_map<int, std::filesystem::path> watchMap_;  // wd → path
};

class EventQueue {
public:
    void push(FileEvent event);
    std::optional<FileEvent> tryPop();
    size_t size() const;

private:
    std::queue<FileEvent> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
};
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| 文件创建 | 在监控目录新建文件 | 触发备份事件 |
| 文件修改 | 修改已有文件 | 检测到 MODIFY 事件 |
| 文件删除 | 删除文件 | 触发 DELETE 事件 |
| 目录创建 | 新建子目录 | 自动添加 watch |
| 高频修改 | 1s 内修改 10 次 | 去抖合并为 1 次 |
| 大目录 | 10k+ 文件目录 | 无 watch 溢出 |
| 重命名 | 文件/目录重命名 | MOVED_FROM + MOVED_TO |
| 停止监控 | 调用 stop() | 线程安全退出 |

## 边界情况处理

- **inotify 队列溢出**：读取 `/proc/sys/fs/inotify/max_user_watches`，超过限制时发出警告
- **事件丢失**：监听 `IN_Q_OVERFLOW` 事件，触发全量备份
- **文件被锁**：尝试跳过，等下次 `IN_CLOSE_WRITE` 事件
- **监控路径不存在**：路径被删除后自动停止对应 watch
- **符号链接**：`inotify_add_watch` 会自动跟随符号链接到目标

## 关键文件

```
src/watch/
├── file_watcher.h
├── file_watcher.cpp
├── event_queue.h
├── event_queue.cpp
├── debouncer.h
└── debouncer.cpp
src/cli/
└── commands.cpp                # watch 子命令
tests/watch/
├── file_watcher_test.cpp
├── event_queue_test.cpp
└── debouncer_test.cpp
```

## 预计工作量

- 代码行数：~1000 行（含测试）
- 开发周期：5-7 天
- 依赖：`<sys/inotify.h>`, `<sys/epoll.h>`, 标准库线程
