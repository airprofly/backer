# Git 分支创建规范

当用户请求创建 Git 分支时，请严格按照以下规范执行。

## 执行流程

### Step 1: 收集信息

引导用户提供以下信息：

| 信息 | 说明 | 获取方式 |
|------|------|----------|
| **需求内容** | 本次开发的内容描述（新功能、bugfix、重构等） | 主动询问 |
| **用户名** | 开发者的用户名标识 | `git config user.name` 获取，转小写 |
| **关联编号** | issue 编号或需求编号（可选） | 主动询问 |

### Step 2: 确定分支类型

根据需求内容选择分支类型：

| 类型 | 适用场景 |
|------|----------|
| `feature` | 新功能开发 |
| `bugfix` | 缺陷修复 |
| `hotfix` | 紧急生产修复 |
| `chore` | 构建/工具/依赖等杂项 |
| `release` | 发布准备 |
| `refactor` | 代码重构 |

### Step 3: 确定序号

- **有关联编号**（如 issue `#42`）：直接用编号作为序号，`42`
- **无关联编号**：查询本地+远程已有分支，取当前用户最大序号自增（两位数 `01`-`99`）

  查询命令：`git branch -a | grep "<type>/<username>_" | sort`

### Step 4: 生成分支名

**命名格式**：
```
<type>/<username>_<sequence>-<description>
```

| 部分 | 约束 | 示例 |
|------|------|------|
| `<type>` | 见 Step 2 类型表 | `feature` |
| `<username>` | 小写字母，来自 git config | `airprofly` |
| `<sequence>` | 数字序号（关联编号 或 两位数自增） | `01` / `42` |
| `<description>` | 2-6 个英文单词，kebab-case，概括需求核心 | `core-backup-restore` |

### Step 5: 检查冲突

```bash
git branch -a | grep "<候选分支名>"
```

有同名分支（本地或远程已存在）时，提示用户并建议调整序号或描述。

### Step 6: 创建分支

```bash
git checkout -b <branch-name>
```

或输出分支名供用户复制备用。

## 完整示例

**用户输入**：
> 需求：给备份引擎添加压缩功能支持
> 用户名：airprofly
> 关联 issue：#23

**分支输出**：`feature/airprofly_23-compression-support`

**用户输入**：
> 需求：修复空路径备份时的段错误
> 用户名：airprofly

**分支输出**（假设已有 `feature/airprofly_01-*`、`feature/airprofly_02-*`）：

```bash
# git branch -a | grep "feature/airprofly_"
feature/airprofly_01-core-backup-restore
feature/airprofly_02-metadata-preserve
```
→ `bugfix/airprofly_03-fix-npe-on-empty-path`

## 约束检查清单

- [ ] 分支名不含中文
- [ ] description 为 kebab-case（全小写，连字符分隔）
- [ ] description 不超过 50 个字符
- [ ] 用户名与实际 git config 一致
- [ ] 序号不与其他分支冲突
