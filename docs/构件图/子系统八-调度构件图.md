# 子系统八：调度子系统构件图描述

## 对应源文件

`subsystem-08-scheduler.svg`

## 图概述

本图展示了调度子系统的内部结构，包含 BackupScheduler 定时调度器、ccronexpr Cron 表达式解析引擎、RetentionPolicy 快照保留策略以及 ScheduleStore 计划持久化存储四个子模块。

## 构件说明

### BackupScheduler（备份调度器）

| 构件 | 类型 | 说明 |
|------|------|------|
| BackupScheduler | 类（线程安全） | 跨平台定时备份调度器。使用 mutex + condition_variable 替代 Linux 独有的 timerfd/epoll，实现统一的跨平台调度机制。构造函数接收 BackupExecutor 回调（`function<bool(ScheduleJob const&)>`） |
| ScheduleJob | 结构体 | 调度任务的数据模型。字段：id（string）、name（string）、cronExpression（string）、source（path）、destination（path）、backupOptions（ScheduleBackupOptions：含 packFormat/compressAlgo/compressLevel/encryptAlgo/password/retainCount/retainDays/preserveMetadata/handleSpecial）、enabled（bool）、createdAt（time_point） |
| JobInfo | 结构体 | 查询结果的数据模型。在 ScheduleJob 基础上增加 nextFireTime（time_point）字段 |
| run() | 成员方法 | 阻塞式主循环：① 检查 stopRequested_ → ② 遍历已启用任务，触发到期任务（调用 executor_ 回调）→ ③ 计算所有任务中最近的 nextFireTime → ④ cv_.wait_until(nextFire) 等待，支持新任务加入时通过 cv_.notify_all() 唤醒 → ⑤ 回到步骤 ① |
| addJob() | 成员方法 | 添加调度任务。流程：生成唯一 ID → 计算首次触发时间（calcNextFire）→ 加入 jobs_ 和 nextFireTimes_ 映射 → cv_.notify_all() 唤醒可能正在等待的 run() 循环 |
| removeJob() | 成员方法 | 按 ID 移除任务，从两个映射表中删除，通知条件变量 |
| enableJob() | 成员方法 | 按 ID 设置任务的 enabled 状态。启用时重新计算 nextFireTime；禁用时从时间表中移除 |
| listJobs() | 成员方法 | 返回所有任务的 JobInfo 列表（包含实时计算的 nextFireTime） |
| findJob() | 成员方法 | 按 ID 查找 ScheduleJob 的内部指针 |
| calcNextFire() | 静态方法 | 计算 cron 表达式的下次触发时间。流程：① 规范化 cron 表达式（5 字段自动补 "0" 前缀变为 6 字段含秒）→ ② cron_parse_expr 解析 → ③ cron_next 计算 → ④ 返回 time_point |
| makeSnapshotPath() | 静态方法 | 根据基准路径 + 当前时间生成快照目录名（格式：YYYYMMDD_HHMMSS） |

### ccronexpr（Cron 表达式引擎）

| 构件 | 类型 | 说明 |
|------|------|------|
| ccronexpr | 第三方 C 语言库 | Cron 表达式解析与时间计算库。编译选项 CRON_USE_LOCAL_TIME=1 使用本地时区。支持标准 5 字段 cron（分/时/日/月/周）和扩展 6 字段 cron（秒/分/时/日/月/周）。支持步进（*/5）、范围（1-5）、列表（1,3,5）语法 |
| cron_parse_expr() | C API | 解析 cron 表达式字符串为 cron_expr 内部对象 |
| cron_next() | C API | 给定 cron_expr 和参考时间，计算下一个匹配的触发时间 |
| cron_expr | 内部结构 | cron 表达式的解析后内部表示 |

### RetentionPolicy（保留策略）

| 构件 | 类型 | 说明 |
|------|------|------|
| RetentionPolicy | 类 | 快照保留策略管理。基于快照目录命名规则（YYYYMMDD_HHMMSS 格式）进行时间排序和清理决策 |
| RetentionConfig | 结构体 | 保留策略配置。字段：maxSnapshots（int，最大快照数量，0=不限制）、retentionDays（int，快照保留天数，0=不限制）。enabled() 方法返回是否有任一策略启用 |
| scanSnapshots() | 静态方法 | 扫描指定目录下所有匹配 YYYYMMDD_HHMMSS 模式的子目录，按路径名排序（即按时间排序）后返回路径列表 |
| parseTimestamp() | 静态方法 | 从快照目录名解析时间戳。使用 std::from_chars 逐字段解析年月日时分秒，通过 mktime 转换为 time_point |
| selectForRemoval() | 静态方法 | 根据 RetentionConfig 选择待删除快照路径列表。逻辑：① 按时间排序快照列表 → ② 若 maxSnapshots>0 且快照数超限，保留最新 N 个，其余标记删除 → ③ 若 retentionDays>0，计算截止时间，早于截止的全部标记删除 → ④ 返回待删除的路径列表 |

### ScheduleStore（计划持久化）

| 构件 | 类型 | 说明 |
|------|------|------|
| ScheduleStore | 类 | 调度任务的 JSON 持久化存储。默认路径 ~/.config/backer/schedule.json。使用自定义轻量 JSON 解析器/写入器（无外部 JSON 库依赖） |
| save() | 成员方法 | 将 vector\<ScheduleJob\> 序列化为 JSON 并写入文件。流程：serializeJobs → 创建父目录（create_directories）→ ofstream 写入 JSON 字符串 |
| load() | 成员方法 | 从 JSON 文件加载反序列化为 vector\<ScheduleJob\>。流程：检查文件存在 → ifstream 读取 → deserializeJobs 解析。文件不存在时返回空列表（不报错） |
| serializeJobs() | 内部方法 | 将 C++ 对象数组序列化为 JSON 字符串。构建 "jobs" 数组，每个 job 包含 id/name/cron/source/dest/enabled/backupOptions 等字段 |
| deserializeJobs() | 内部方法 | 从 JSON 字符串反序列化。使用轻量解析器：extractObject 提取对象 → extractStr/extractInt/extractBool 提取字段值 → 逐字段填充 ScheduleJob |
| extractStr() | 内部辅助 | 从 JSON 对象中提取字符串字段值 |
| extractInt() | 内部辅助 | 从 JSON 对象中提取整数字段值 |
| extractBool() | 内部辅助 | 从 JSON 对象中提取布尔字段值 |

## 调度主循环逻辑

```
run() 循环：
  while (!stopRequested_):
    now = system_clock::now()
    
    for each enabled job:
      if job.nextFireTime <= now:
        executor_(job)           // 同步执行备份
        job.nextFireTime = calcNextFire(job.cron, now)
    
    nextWake = min(all job.nextFireTimes)
    cv_.wait_until(lock, nextWake)  // 阻塞直到最近触发时间
                                     // 或被 addJob/removeJob/enableJob 唤醒
```

## 保留策略清理逻辑

```
selectForRemoval(snapshots, config):
  toRemove = []
  sorted = sort_by_time(snapshots)   // 从旧到新排序
  
  // 数量限制：保留最新 N 个
  if config.maxSnapshots > 0:
    if sorted.size() > config.maxSnapshots:
      toRemove += sorted[0 .. size-maxSnapshots)
  
  // 天数限制：删除超过期限的
  if config.retentionDays > 0:
    cutoff = now - days(config.retentionDays)
    for each snapshot:
      if snapshot.time < cutoff:
        toRemove += snapshot
  
  return unique(toRemove)
```

## 依赖关系

| 依赖 | 说明 |
|------|------|
| ccronexpr | C 语言 cron 库，通过 calcNextFire() 静态方法调用 |

## 设计模式

- **观察者模式变体**：BackupScheduler 通过 executor_ 回调（std::function）解耦调度触发与备份执行，不直接依赖 BackupEngine
- **条件变量同步**：使用 condition_variable::wait_until 实现精确到秒的定时等待，新任务加入时 notify_all 唤醒
- **策略模式**：RetentionConfig 作为策略配置，RetentionPolicy 解释执行配置语义
