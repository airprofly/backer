# 子系统六：文件过滤子系统构件图描述

## 对应源文件

`subsystem-06-filter.svg`

## 图概述

本图展示了文件过滤子系统的内部结构，包含 Filter 抽象接口、NoopFilter 空过滤器、以及 CriteriaFilter 条件过滤器及其支持的六大过滤维度。

## 构件说明

### 接口层

| 构件 | 类型 | 说明 |
|------|------|------|
| Filter | 抽象类（接口） | 过滤器策略接口，定义 `apply(files: vector<FileEntry>&) → vector<FileEntry>` 纯虚方法。接收文件列表，返回过滤后的子集 |
| NoopFilter | final 实现类 | 空过滤器实现。apply() 方法原样返回输入文件列表，不做任何过滤 |

### 过滤条件结构体

| 构件 | 类型 | 说明 |
|------|------|------|
| FilterCriteria | 结构体 | 单条过滤条件的数据载体。字段：pathGlob（路径模式列表）、fileType（文件类型列表）、nameGlob（名称模式列表）、timeRange（TimeRange 时间范围）、sizeRange（SizeRange 大小范围）、ownerId（optional\<uint32_t\> 属主 ID）、exclude（bool，true 表示排除条件） |
| TimeRange | 结构体 | 时间过滤范围。字段：beforeSec+Nsec（修改时间不得晚于）、afterSec+Nsec（修改时间不得早于）、hasBefore 和 hasAfter（bool，指示边界是否有效） |
| SizeRange | 结构体 | 大小过滤范围。字段：minSize / maxSize（uint64_t 边界值）、hasMin 和 hasMax（bool，指示边界是否有效） |

### CriteriaFilter（条件过滤器）

| 构件 | 类型 | 说明 |
|------|------|------|
| CriteriaFilter | 实现类 | 条件过滤器实现，支持包含+排除两阶段过滤逻辑。内部维护多个 FilterCriteria 对象 |
| matches() | 私有方法 | 检查单个文件条目是否满足一个 FilterCriteria。返回 bool。按字段逐项匹配：路径 glob、文件类型、名称 glob、时间范围、大小范围、属主 ID。单条 criteria 内各字段为 AND 关系 |
| apply() | 公开方法 | 执行完整过滤。两阶段逻辑：① 确定哪些维度有 include 条件 → 对每个条目逐维度检查（AND）→ 同维度内多个条件 OR → 无条件的维度跳过；② 逐个检查 exclude 条件，任一匹配即剔除 |
| globToRegex() | 私有静态方法 | 将 glob 通配符模式转换为 std::regex 正则表达式。pathMode=true 时 \* 不匹配路径分隔符 "/" |
| matchGlob() | 私有静态方法 | 通配符匹配函数。POSIX 平台使用 fnmatch(FNM_PATHNAME) 高效匹配；Windows 平台使用 std::regex_match（glob→regex 转换后匹配）|

## 过滤逻辑详解

### 第一阶段：包含过滤（AND + OR 组合）

```
对每个文件条目：
  ┌─ 路径维度有 include 条件？
  │   是 → 任一 include-path glob 匹配？ 否 → 排除
  │   否 → 跳过此维度
  ├─ 类型维度有 include 条件？
  │   是 → 任一 include-type 匹配？ 否 → 排除
  │   否 → 跳过此维度
  ├─ 名称维度（同上）
  ├─ 时间维度（同上）
  ├─ 大小维度（同上）
  ├─ 属主维度（同上）
  └─ 全部活跃维度通过 → 保留
```

### 第二阶段：排除过滤

```
对第一阶段保留的每个条目：
  逐一检查所有 exclude 条件
  任一匹配 → 剔除
  全部不匹配 → 最终保留
```

## 六大过滤维度

| 维度 | 数据字段 | CLI 参数 | GUI 控件 |
|------|---------|---------|---------|
| 路径过滤 | pathGlob | --include-path / --exclude-path | FilterDialog 路径标签页（+/- 按钮列表）|
| 类型过滤 | fileType | --include-type / --exclude-type | FilterDialog 类型标签页（7 种复选框）|
| 名称过滤 | nameGlob | --include-name / --exclude-name | 无 GUI 控件（CLI only）|
| 时间过滤 | timeRange | --mtime-before / --mtime-after | FilterDialog 时间标签页（QDateTimeEdit）|
| 大小过滤 | sizeRange | --size-min / --size-max | FilterDialog 大小标签页（QSpinBox + 单位 ComboBox）|
| 属主过滤 | ownerId | --owner | FilterDialog 属主标签页（QLineEdit）|

## 平台差异

| 平台 | glob 匹配实现 | 备注 |
|------|-------------|------|
| POSIX | fnmatch(str, pattern, FNM_PATHNAME) | 系统调用，高效，语义正确 |
| Windows | std::regex_match（glob→regex 转换） | fnmatch 不可用时的回退方案 |

## 设计模式

- **策略模式**：Filter 接口 + CriteriaFilter / NoopFilter 实现
- **解释器模式**：FilterCriteria 结构体本质上是过滤条件 AST，apply() 方法解释执行
