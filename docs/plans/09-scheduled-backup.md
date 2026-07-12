# 定时备份 — 实施计划

## 功能概述

允许用户设置周期性定时备份任务，支持 cron 表达式配置备份频率，并提供数据淘汰机制以管理历史版本。

## 对应需求

详见 [docs/requirements.md](../requirements.md)（定时备份 / 10 分）。

## 技术方案

### 架构设计

```
┌──────────────────────────────────────────────────┐
│                 BackupScheduler                    │
│                                                   │
│  schedule(cronExpr, task) → taskId                │
│  cancel(taskId)                                   │
│  listJobs() → vector<JobInfo>                     │
│                                                   │
│  内部循环:                                         │
│  timerfd + epoll_wait + ccronexpr                 │
└──────────────────────────────────────────────────┘
```

### 核心流程

```
1. 用户添加定时任务 (cron 表达式 + 备份参数)
2. 计算下次触发时间 (ccronexpr)
3. 设置 timerfd 定时器
4. epoll_wait 等待 → 超时触发
5. 调用 BackupEngine::backup()
6. 计算再下次触发时间 → 设置 timerfd
7. 循环直至任务取消
```

### Cron 表达式支持的格式

| 格式 | 示例 | 说明 |
|------|------|------|
| 标准 5 字段 | `0 2 * * *` | 每天凌晨 2:00 |
| 带秒 6 字段 | `0 30 9 * * 1-5` | 工作日 9:30 |
| 常用简写 | `@daily`, `@hourly`, `@weekly` | 预定义时间 |
| 步进 | `*/15 * * * *` | 每 15 分钟 |
| 范围 | `0 9-18 * * *` | 每小时的 0 分，9-18 点 |

### 数据淘汰策略

| 策略 | 说明 | 配置示例 |
|------|------|---------|
| 按数量 | 保留最近 N 个快照 | `--retain-count 30` |
| 按时长 | 保留 N 天的快照 | `--retain-days 7` |
| 混合 | 按时间分层保留（如：每小时保留最近 24h，每天保留最近 30 天，每月保留最近 12 个月） | 分级保留 |

### 淘汰算法实现

```
淘汰流程：
1. 扫描备份目录，获取所有快照及其时间戳
2. 按时间排序
3. 根据保留策略标记可删除的快照
4. 删除过期的快照文件 + 对应索引
5. 记录淘汰日志
```

### 持久化

定时任务配置存储为 JSON 文件 `~/.config/backer/schedule.json`：

```json
{
  "jobs": [
    {
      "id": "job_001",
      "name": "每日备份",
      "cron": "0 2 * * *",
      "source": "/home/user/data",
      "destination": "/backup/daily",
      "options": {
        "compress": "zstd",
        "retainCount": 30
      },
      "enabled": true,
      "createdAt": "2026-07-01T10:00:00Z"
    }
  ]
}
```

## 实施步骤

| 步骤 | 内容 | 产出 |
|------|------|------|
| 1. ccronexpr 集成 | 添加 ccronexpr 单头文件库 | `third_party/ccronexpr/` |
| 2. Job 数据结构 | 定时任务描述结构体 | `backup_scheduler.h` |
| 3. 时间计算 | cron 表达式 → 下次触发时间 | `backup_scheduler.cpp` |
| 4. timerfd 事件循环 | timerfd + epoll 实现定时触发 | `backup_scheduler.cpp` |
| 5. 备份任务执行 | 触发 → 调用 BackupEngine | `backup_scheduler.cpp` |
| 6. 任务配置持久化 | JSON 序列化/反序列化 | `schedule_store.h/cpp` |
| 7. 数据淘汰 | 按策略删除过期快照 | `retention_policy.h/cpp` |
| 8. CLI 集成 | `backer schedule` 子命令 | `commands.cpp` |
| 9. 守护进程模式 | `backer daemon` 后台运行 | `main.cpp` |
| 10. 单元测试 | cron 解析 + 淘汰策略 + 时间计算 | 测试文件 |

## 关键接口

```cpp
struct ScheduleJob {
    std::string id;
    std::string name;
    std::string cronExpression;
    std::filesystem::path source;
    std::filesystem::path destination;
    BackupOptions options;
    bool enabled;
    std::chrono::system_clock::time_point createdAt;
};

class BackupScheduler {
public:
    BackupScheduler();

    std::expected<std::string, ErrorCode> addJob(ScheduleJob const& job);
    std::expected<void, ErrorCode> removeJob(std::string_view jobId);
    std::expected<void, ErrorCode> enableJob(std::string_view jobId, bool enabled);
    std::vector<ScheduleJob> listJobs() const;

    // 启动调度循环（阻塞，应在独立线程中运行）
    void run();

    // 停止调度循环
    void stop();

private:
    void onTimerFired(std::string const& jobId);
    std::chrono::system_clock::time_point calcNextFire(
        std::string const& cronExpr,
        std::chrono::system_clock::time_point from);

    std::map<std::string, ScheduleJob> jobs_;
    std::atomic<bool> running_{false};
    int timerFd_{-1};
    int epollFd_{-1};
};

// 数据淘汰
class RetentionPolicy {
public:
    struct Config {
        std::optional<int> maxSnapshots;    // 最大快照数量
        std::optional<int> retentionDays;    // 保留天数
        bool enabled = false;
    };

    std::vector<std::filesystem::path> selectForRemoval(
        std::vector<std::filesystem::path> const& snapshots,
        Config const& config) const;
};
```

## 测试策略

| 场景 | 方法 | 验证点 |
|------|------|--------|
| cron 解析 | `0 2 * * *` → 下次凌晨 2 点 | 时间计算正确 |
| 定时触发 | 设置每隔 1 分钟 | 60s 后正确触发 |
| 多任务 | 添加 3 个不同时间任务 | 各自按计划触发 |
| 任务取消 | 执行中取消 | 不再触发 |
| 淘汰策略 | 保留最近 N 个 | 旧快照被删除 |
| 持久化 | 保存配置 → 重启 → 重新加载 | 任务配置恢复 |
| 混合保留 | 小时/天/月分级 | 符合预期 |

## 关键文件

```
src/scheduler/
├── backup_scheduler.h
├── backup_scheduler.cpp
├── schedule_store.h
├── schedule_store.cpp
├── retention_policy.h
├── retention_policy.cpp
└── ccronexpr.h                # ccronexpr 单头文件
src/cli/
└── commands.cpp                # schedule 子命令
tests/scheduler/
├── backup_scheduler_test.cpp
├── retention_policy_test.cpp
└── ccronexpr_test.cpp
```

## 预计工作量

- 代码行数：~1000 行（含测试）
- 开发周期：5-7 天
- 依赖：ccronexpr (单头文件, FetchContent)

> **状态**：✅ 已完成 (2026-07-12)
