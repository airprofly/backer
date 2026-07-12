## 测试分层

| 层级 | 说明 | 是否需要 `setup-testdata.sh` |
|------|------|------------------------------|
| 单元测试 (`ctest`) | Mock 文件系统，测试函数/类逻辑 | **不需要** — 可直接运行 |
| 集成测试 (`ctest`) | Mock 文件系统，验证模块间协作 | **不需要** — 可直接运行 |
| 内存检测 (Valgrind) | 通过 `gtest` 覆盖路径检测泄漏 | **不需要** — 同上 |
| 性能分析 (gprof/perf/gperftools) | 对真实二进制进行采样 | **不需要** — 但需先编译 Debug |
| 手动 CLI / 端到端 (`test-backup-restore.sh`) | 使用真实文件读写完整流程 | **需要** — 必须先执行 `setup-testdata.sh` |

---

测试前置（仅手动 CLI / 端到端测试）：`bash scripts/setup-testdata.sh` 生成测试数据。

**注意**：`ctest --test-dir build --output-on-failure` 使用 Mock 文件系统，无需任何外部数据即可直接运行。

## 范围

只测试受代码修改影响的功能，未修改的功能不测试，除非特别指明。

## 测试副作用清理

所有测试执行后必须恢复原状，不得在工作目录中残留任何测试产物。中间文件一律放入指定目录：

- 运行日志 → `logs/`
- 分析产物（profile data、perf.data、gmon.out 等）→ `output/`

上述两目录已 `.gitignore` 忽略。工作目录及源码树中不得出现任何测试生成的文件。

## 手动 CLI / 端到端测试要求

1. **隔离**：执行前执行 `rm -rf data/backup data/restore` 清理上一组结果；执行完成后同样清理本次产物，恢复原状
2. **执行**：运行命令，同时捕获 stdout 和 stderr
3. **验证**：
   - 检查 CLI 输出中的成功/失败状态及统计信息（Files/Dirs/Size）
   - 对于筛选测试，关注日志中 `CriteriaFilter: N → M entries after filtering` 行
   - 检查目标目录的实际内容与预期一致（`find`、`ls -la`、`diff`、`readlink` 等）
   - 对异常路径（空目录、不存在的源、权限不足等）验证命令能优雅处理而非崩溃

## 详细运行命令

### 基础测试

```bash
# 运行全部单元测试
ctest --test-dir build
# 带详细输出（失败时打印）
ctest --test-dir build --output-on-failure

# 运行指定测试
./build/backer_test --gtest_filter="*BackupCore*"
```

### Valgrind 内存检测（必须通过，否则 CI 不合入）

> 输出目录约定：运行日志放 `logs/`，分析产物放 `output/`。

```bash
mkdir -p logs output
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 \
    ./build/backer_test 2>logs/valgrind.log
```

### gprof 性能分析（需 `-pg` 编译）

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_CXX_FLAGS="-pg"
cmake --build build
./build/backer-cli > /dev/null 2>&1
mv gmon.out output/gmon.out
gprof ./build/backer-cli output/gmon.out > output/gprof.txt
```

### perf 内核级 CPU 采样（无需重新编译）

```bash
perf record -g -o output/perf.data -- ./build/backer-cli
perf report --stdio -i output/perf.data > output/perf.txt
```

### gperftools CPU Profiler + Heap Profiler（链接 `-ltcmalloc_minimal`）

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DCMAKE_EXE_LINKER_FLAGS="-ltcmalloc_minimal"
cmake --build build
CPUPROFILE=output/profile.out ./build/backer-cli > /dev/null 2>&1
pprof --callgraph ./build/backer-cli output/profile.out > logs/gperftools.txt
```
