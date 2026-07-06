# Git Commit 信息生成指南

当用户请求生成 Git commit 信息时,请严格按照以下规范生成:

## 提交信息格式规范

遵循 **约定式提交(Conventional Commits)** 规范:

```
<type>(<scope>): <subject>

<body>

<footer>
```

## Type 类型(必选)

- `feat` - 新功能(feature)
- `fix` - 修复 bug
- `docs` - 文档变更(documentation)
- `style` - 代码格式调整(不影响代码运行的变动)
- `refactor` - 重构(既不是新增功能,也不是修复 bug) 
- `perf` - 性能优化
- `test` - 测试相关
- `chore` - 构建过程或辅助工具的变动
- `ci` - CI 配置文件和脚本的变动
- `revert` - 回退之前的 commit

## Scope 范围(可选)

指定 commit 影响的范围,例如:
- `common` - common 模块
- `pojo` - pojo 模块
- `server` - server 模块
- `auth` - 认证相关
- `database` - 数据库相关
- `api` - API 接口相关

## Subject 主题(必选)

- **使用英文描述**
- 简洁准确,不超过 50 个字符
- 不要以句号结尾
- 使用祈使句,如"add"而不是"added"或"has added"

## Body 正文(可选)

**重要**:Body 应该精炼简洁,避免冗长

- 简要说明变更的核心内容(1-3 点即可)
- 重点说明 **为什么** 要做这个改动(而非重复"做了什么")
- 只列出关键变更点,避免面面俱到的罗列
- 每行不超过 72 个字符
- 使用列表形式列出多项变更

## Footer 脚注(可选)

- 关联的 Issue:`Closes #123` 或 `Fixes #456`
- BREAKING CHANGE:不兼容的变动说明
- 引用相关 commit:`Refs commit abc123`

## 示例模板

### 1. New Feature
```
feat: add user authentication

- implement login/registration, JWT auth, password encryption, and permission verification

Closes #123
```

### 2. Bug Fix
```
fix(server): fix order status update issue

Problem: concurrent order status updates cause data inconsistency
Cause: missing optimistic locking mechanism
Solution: add version field and optimistic lock validation

Fixes #456
```

### 3. Refactor
```
refactor(common): refactor date utility class

- unify date format handling, optimize performance, and add unit tests
```

### 4. Initial Commit
```
feat: initialize project structure

- create parent POM config (Spring Boot 3.1.2, Java 17)
- add common, pojo, server modules and configure Maven multi-module dependency management
```

### 5. Documentation Update
```
docs: update API documentation

- add new API endpoint descriptions
- update request/response examples
- supplement error code documentation
```

### 6. Performance Optimization
```
perf(server): optimize database query performance

- add indexes, optimize N+1 queries, enable caching, improve performance by ~60%
```

## 生成提交信息时的检查清单

1. ✅ Type 类型是否正确
2. ✅ Subject 是否简洁清晰(≤50 字符)
3. ✅ **是否使用英文描述**
4. ✅ Body 是否详细说明了变更内容
5. ✅ 是否包含了必要的关联 Issue
6. ✅ 是否有 BREAKING CHANGE 说明(如有破坏性变更)
7. ✅ 格式是否符合约定式提交规范

## 特殊情况处理

### Minor Changes
For simple modifications (e.g., fixing typos), you can have only the subject:
```
fix: correct username typo
```

### Multi-module Changes
If changes involve multiple modules, use `all` as scope or omit scope:
```
feat: add global exception handler
```

### Revert Commit
```
revert: revert user authentication feature

This reverts commit abc123def456
```

## 注意事项

1. **精炼优先**: Body 正文控制在 1-3 点即可,避免冗长罗列
2. **英文优先**: **使用英文描述 commit 信息**,便于国际化团队协作和自动化工具处理
3. **言简意赅**: 标题要简洁,正文要精炼
4. **说明原因**: 重点说明 **为什么** 要做这个改动(而非重复"做了什么")
5. **一次一事**: 一个 commit 只做一件事
6. **可搜索性**: 使用规范的 type 和 scope,便于后续搜索和过滤
7. **自动化友好**: 规范的提交信息便于自动生成 CHANGELOG

## 经验规则

**请使用英文**：所有 commit 信息（包括 subject、body、footer）必须使用英文撰写，便于国际化团队协作和自动化工具处理。

## 使用建议

- 在生成提交信息前,先使用 `git diff --staged` 或 `git diff --cached` 查看已暂存的变更内容
- **只对比已暂存(staged)的文件与上一次提交的差异**
- **未暂存(unstaged)的文件和未跟踪(untracked)的文件不包含在 commit 信息中**
- 根据变更的规模和影响程度选择合适的格式
- 对于重大变更,务必在 footer 中说明 BREAKING CHANGE
- 保持提交信息的一致性,便于团队协作

## 变更分析步骤

生成 commit 信息时,请按以下步骤分析:

1. **查看已暂存的变更**: 执行 `git diff --staged` 或 `git diff --cached`
2. **查看已暂存的文件列表**: 执行 `git diff --staged --name-only`
3. **对比上一次提交**: 分析暂存区内容相对于上一次提交的差异
4. **忽略未暂存的变更**: 不要将 `git diff`(未暂存)或 `git status` 中显示的未跟踪文件纳入分析
5. **生成 commit 信息**: 基于已暂存的变更生成符合规范的提交信息

## 输出要求

**重要**:
1. **禁止自动提交**: 严禁执行 `git commit` 命令自动提交，只能将 commit 信息写入文件由用户手动提交
2. **写入文件**: 生成的 commit 信息必须输出到当前工作区的 `.git/COMMIT_EDITMSG` 文件中。使用 Write 工具将生成的提交信息写入该文件路径,例如:

```bash
# 文件路径应为: <当前工作区根目录>/.git/COMMIT_EDITMSG
# 例如: /home/airprofly/codeFolder/takeout/.git/COMMIT_EDITMSG
```

写入时只包含提交信息内容,不包含任何额外的说明文字或标记。
