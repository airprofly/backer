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

- 每个测试文件按类/功能组织 `TEST_F` 或 `TEST`
- 覆盖边界场景（空值、异常输入）和正常路径
- 测试间隔离，临时目录在 `SetUp`/`TearDown` 中创建清理
- 新增测试文件时，确认 `CMakeLists.txt` 中有对应的测试目标

### 3. 构建并运行测试

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

- 不通过则回头修代码或测试，直到**全部通过**

### 4. 更新使用文档

更新 `docs/usage.md`：

- 新增功能：添加对应的功能章节（命令形式、选项、示例、格式细节）
- 修改功能：更新已有章节中的命令、选项、示例
- 确保文档中的命令示例与实际 CLI 一致

### 5. 功能测试

执行 `.claude/commands/test-features.md` 中的测试流程：

- 生成测试数据：`bash scripts/setup-testdata.sh`
- 按 `test-features.md` 要求测试受影响的功能
- 每组测试前执行 `rm -rf data/backup data/restore` 清理
- 验证 CLI 输出、目录内容、日志行符合预期

### 6. 更新项目文档

执行 `.claude/commands/update-info.md` 更新项目文档：

- 更新 `CLAUDE.md`：构建命令、测试、代码规范、架构、目录结构等核心信息
- 更新 `README.md`：构建运行说明、CLI 使用示例、Docker 指南等面向人类的内容
- 以项目实际源码为准，先探索再更新
- 目录结构不枚举所有文件，只保留关键目录和文件

## 检查清单

- [ ] 代码实现完成，符合项目规范和架构
- [ ] `Dockerfile` 已同步更新（如有需要）
- [ ] 测试用例覆盖正常和边界路径
- [ ] 单元测试全部通过（`ctest --test-dir build --output-on-failure`）
- [ ] `docs/usage.md` 已同步更新
- [ ] 端到端功能测试通过（`test-features.md` 流程）
- [ ] `CLAUDE.md` 和 `README.md` 已同步更新
