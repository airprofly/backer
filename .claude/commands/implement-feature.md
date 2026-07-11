# 新功能实现流水线

当用户要求实现一个新功能或修改已有功能时，按以下流水线执行。

## 流水线步骤

### 1. 实现代码

根据需求编写实现代码，遵循项目已有的代码规范和架构设计（见 CLAUDE.md）。

- 新模块在 `src/` 下对应子目录中创建
- 接口定义在对应的 `.h` 文件中，实现放在 `.cpp`
- 遵循编码风格：`PascalCase` 类名、`camelCase` 函数/变量、`const` 右侧、`#pragma once`
- 需要新依赖时，在 `CMakeLists.txt` 中用 `FetchContent` 声明
- **新增依赖后检查 `Dockerfile` 是否需要同步更新**

### 2. 编写测试

在 `tests/` 下对应的测试文件或新文件中写 Google Test 测试用例。

#### 测试分层

| 层级 | 说明 | 工具/方式 |
|------|------|----------|
| 单元测试 | 对函数/类级别进行隔离测试 | Google Test (gtest) |
| 集成测试 | 验证模块间协作（备份→打包→存储、还原→解包→写入） | 端到端脚本 + gtest `TEST_P` / `TEST_F` |
| 内存测试 | 检测内存泄漏、越界访问 | Valgrind (`--leak-check=full --show-leak-kinds=all`) |
| 性能测试 | 识别热点函数、评估时间复杂度 | perf（内核级采样）/ gprof（`-pg` 编译） |

#### 测试设计方法

**所有逻辑代码必须有对应测试用例**，未覆盖不允许提交。

- **等价类划分**：输入空间较大时，选取代表性数据覆盖有效/无效等价类
  - 例：文件尺寸筛选 → 0字节（有效）、1~1024B（有效）、>10MB（有效）、-1（无效）
- **边界值分析**：在等价类边界上取值，捕捉常见错误
  - 例：文件大小 0、1、MAX；路径深度 0、1、1024；过滤器数量 0、1、MAX
- **白盒测试**：覆盖语句覆盖、分支覆盖、条件覆盖、路径覆盖
  - 例：对 `if/else`、`switch`、循环边界编写显式用例

#### 单元测试规范

- 每个测试文件按类/功能组织 `TEST_F` 或 `TEST`
- 使用等价类和边界值分析方法设计用例，确保覆盖正常和异常路径
- 覆盖边界场景（空值、空文件、大文件、特殊路径）和异常路径（权限不足、磁盘满、路径不存在）
- 测试间隔离，临时目录在 `SetUp`/`TearDown` 中创建清理
- 管道接口（Compressor、Encryptor、Packer）使用参数化测试 `TEST_P` 遍历不同算法策略
- 新增测试文件时，确认 `CMakeLists.txt` 中有对应的测试目标

#### 集成测试要求

- 验证完整流程：备份 → 筛选 → 元数据采集 → 打包 → 落盘 → 还原 → 解包 → 校验
- 使用真实测试数据目录（由 `scripts/setup-testdata.sh` 生成），而非内存 mock
- 执行 `bash scripts/test-backup-restore.sh` 进行端到端验证（交互式选择 Local/Docker 模式）

#### 构建并运行测试

```bash
# 配置构建
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 编译
cmake --build build -j$(nproc)

# 运行全部单元测试
ctest --test-dir build --output-on-failure

# 运行指定测试
./build/backer_test --gtest_filter="*BackupCore*"
```

- 不通过则回头修代码或测试，直到**全部通过**

#### 内存与性能测试

输出目录约定：
- `logs/` — 运行日志（Valgrind 等文本日志）
- `output/` — 分析产物（perf.data、perf.txt、gmon.out、gprof.txt 等二进制/报告）

```bash
mkdir -p logs output

# ── Valgrind 内存检测（必须通过，否则 CI 不合入）──
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
    ./build/backer_test 2>logs/valgrind.log

# ── gprof 性能分析（需 -pg 编译）──
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_CXX_FLAGS="-pg"
cmake --build build
./build/backer-cli > /dev/null 2>&1          # 执行产生 profile
mv gmon.out output/gmon.out
gprof ./build/backer-cli output/gmon.out > output/gprof.txt

# ── perf 内核级 CPU 采样（无需重新编译）──
perf record -g -o output/perf.data -- ./build/backer-cli   # 采样调用栈
perf report --stdio -i output/perf.data > output/perf.txt   # 文本报告
```

- 新增代码涉及动态内存分配时，必须运行 Valgrind 确认无泄漏（检查 `logs/valgrind.log` 无 error）
- 核心路径（大量 I/O、循环、哈希计算）必须跑 **perf + gprof** 两种工具确认无显著性能退化

### 3. 更新使用文档

更新 `docs/usage.md`：

- 新增功能：添加对应的功能章节（命令形式、选项、示例、格式细节）
- 修改功能：更新已有章节中的命令、选项、示例
- 确保文档中的命令示例与实际 CLI 一致

### 4. 功能测试

执行 `.claude/commands/test-features.md` 中的测试流程：

- 生成测试数据：`bash scripts/setup-testdata.sh`
- 按 `test-features.md` 要求测试受影响的功能
- 每组测试前执行 `rm -rf data/backup data/restore` 清理
- 验证 CLI 输出、目录内容、日志行符合预期

### 5. 更新项目文档

执行 `.claude/commands/update-info.md` 更新项目文档：

- 更新 `CLAUDE.md`：构建命令、测试、代码规范、架构、目录结构等核心信息
- 更新 `README.md`：构建运行说明、CLI 使用示例、Docker 指南等面向人类的内容
- 以项目实际源码为准，先探索再更新
- 目录结构不枚举所有文件，只保留关键目录和文件

## 检查清单

- [ ] 代码实现完成，符合项目规范和架构
- [ ] `Dockerfile` 已同步更新（如有需要）
- [ ] 测试用例覆盖正常和边界路径（等价类划分 + 边界值分析 + 白盒分支覆盖）
- [ ] 单元测试全部通过（`ctest --test-dir build --output-on-failure`）
- [ ] 集成测试通过（`bash scripts/test-backup-restore.sh`）
- [ ] Valgrind 内存检测无泄漏（`--error-exitcode=1`）
- [ ] `docs/usage.md` 已同步更新
- [ ] 端到端功能测试通过（`test-features.md` 流程）
- [ ] `CLAUDE.md` 和 `README.md` 已同步更新
