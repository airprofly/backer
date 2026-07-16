---
标题：Backer — 数据备份软件 项目说明
要点：
- 软件综合实验 课程项目
- 电子科技大学 计算机科学与工程学院
- 作者：古文斌 2023080901012、叶炜 2023080901024、谢松谕 2023080902017
- 日期：2026年7月
---

---
标题：目录 / Outline
要点：
- 项目背景与需求
- 系统架构设计
- 功能实现与评分
- UML 建模
- 软件开发工具链
- 人员分工与进度把控
- 总结与展望
---

---
标题：项目背景
要点：
- 电子科技大学计算机专业最后一门实践必修课，综合运用本科所学知识
- 围绕一个模拟实战软件项目，以 3 人团队完成从需求分析→系统设计→编码实现→集成测试的完整软件生命周期
- 项目目标：开发一款**数据备份软件**，支持目录树的备份与还原，并可选实现多项扩展功能
- 重点考察：正确性、易用性、健壮性
---

---
标题：需求概述
要点：
- **基本要求**：目录树文件数据的备份与还原
- **扩展要求**：
  - 特殊文件支持（管道/链接/设备文件）
  - 元数据支持（属主/时间/权限）
  - 自定义筛选（路径/类型/名称/时间/尺寸/属主 6 维度）
  - Tar / Zip 两种打包格式
  - gzip / zstd / lzma 三种压缩算法
  - AES-256-GCM / SM4-CBC 两种加密算法
  - Qt 6 图形界面
  - cron 定时备份与自动淘汰
---

---
标题：总体架构设计
要点：
- 自底向上五层分层架构：(1)存储层（本地文件系统）→ (2)基础设施层（FSAbstraction/元数据管理/特殊文件支持）→ (3)管道层（Filter→Packer→Compressor→Encryptor）→ (4)业务层（BackupEngine/RestoreEngine/Scheduler）→ (5)表现层（CLI + GUI），各层间箭头标注数据流向。右侧标注关键技术：CLI11、spdlog、Qt6、OpenSSL、miniz、ccronexpr
- 采用**五层分层架构** + **管道模式**设计
  - 表现层：CLI (CLI11) + GUI (Qt 6)
  - 业务层：BackupEngine / RestoreEngine / BackupScheduler
  - 管道层：Filter → Packer → Compressor → Encryptor 可插拔组合
  - 基础设施层：FSAbstraction / Metadata / 特殊文件处理
  - 存储层：LocalStorage
- 各层通过抽象接口通信，上层依赖下层，下层对上层无感知
- ![五层分层架构](diagrams/dist/backer-layers.svg)
---

---
标题：核心模块 — 备份/还原引擎
要点：
- **BackupEngine**: 备份流程编排器
  - `walk(source)` → 遍历目录树获取 FileEntry 列表
  - `filter(entries)` → 按 6 维度筛选
  - `pack(entries)` → Tar/Zip 打包
  - `compress(buffer)` → gzip/zstd/lzma 压缩
  - `encrypt(buffer)` → AES/SM4 加密
  - `storage.write()` → 持久化到本地
- **RestoreEngine**: 还原流程编排器（逆操作）
  - 支持目录镜像模式（逐个文件复制）和归档模式（打包文件）
- 备份流程完整步骤：用户输入 backup 命令 → CLI 解析参数 → BackupEngine 调起 → FSAbstraction 遍历目录 → Filter 筛选 → Packer 打包 → Compressor 压缩 → Encryptor 加密 → Storage 写入磁盘，各步骤可选（虚线框标注）
---

---
标题：核心模块 — 管道策略接口
要点：
- 采用**策略模式**设计管道阶段，新增算法无需修改现有代码
- **Filter**：`apply(entries)` → 返回筛选后子集
- **Packer**：`pack/unpack(files, fs, stream)` → TarPacker / ZipPacker
- **Compressor**：`compress/decompress(input, output)` → Gzip / Zstd / Lzma
- **Encryptor**：`encrypt/decrypt(input, output, password)` → AES-256-GCM / SM4-CBC
- 使用工厂模式 (`CompressorFactory` / `EncryptorFactory`) 实现算法注册与按名创建
- 所有接口使用 `std::expected<T, ErrorCode>` 返回，错误传播清晰
---

---
title: UML 建模 — 用例图
要点：
- 主要用例包括：备份数据（含筛选文件子用例）、还原数据、管理定时任务（含添加/删除/启用/禁用子用例）、查看备份索引、配置参数（含选择打包格式/选择压缩算法/选择加密算法子用例）
- 覆盖全部需求功能点
- `docs/diagrams/src/use-case.puml`
- ![UML 用例图](diagrams/dist/backer-use-case.svg)
---

---
标题：UML 建模 — 类图
要点：
- 五大分层核心类：表现层（MainWindow/BackupTab/CLICommands）、业务层（BackupEngine/RestoreEngine/BackupScheduler）、管道层（Filter←CriteriaFilter, Packer←TarPacker/ZipPacker, Compressor←GzipCompressor/ZstdCompressor/LzmaCompressor, Encryptor←OpenSslEncryptor）、基础设施层（FSAbstraction←LocalFsAbstraction）、存储层（Storage←LocalStorage）
- 管道层全部采用接口（纯虚类）设计，支持可插拔策略
- 表现层与业务层解耦，CLI 和 GUI 共用同一套业务接口
- ![UML 类图](diagrams/dist/backer-class.svg)
---

---
标题：UML 建模 — 顺序图 / 构件图
要点：
- 一次完整备份请求的交互流程：用户→CLI→BackupEngine→FSAbstraction(walk)→Filter(apply)→Packer(pack)→Compressor(compress)→Encryptor(encrypt, PBKDF2+AES)→Storage(writeFile)→返回结果
- 可执行构件（backer-cli/backer-gui/backer_test）与各编译单元（commands.cpp, backup_engine.cpp, tar_packer.cpp 等）及外部依赖（CLI11/spdlog/Qt6/OpenSSL/miniz/ccronexpr/Google Test）之间的链接和依赖关系
- 顺序图清晰展示备份请求在 5 层架构中的完整传递路径
- 构件图反映编译期依赖关系，指导模块化开发和增量编译
- ![UML 顺序图](diagrams/dist/backer-sequence-backup.svg)
- ![UML 构件图](diagrams/dist/backer-component.svg)
---

---
标题：功能实现概览
要点：
| 类别 | 功能 | 状态 |
|:-----|:-----|:----:|
| 基本要求 | 目录镜像备份 | ✅ |
| 扩展要求 | 特殊文件支持（管道/链接/设备） | ✅ |
| 扩展要求 | 元数据保留（属主/时间/权限） | ✅ |
| 扩展要求 | 6 维度筛选器（路径/类型/名称/时间/尺寸/属主） | ✅ |
| 扩展要求 | Tar / Zip 打包 | ✅ |
| 扩展要求 | gzip / zstd / lzma 压缩 | ✅ |
| 扩展要求 | AES-256-GCM / SM4-CBC 加密 | ✅ |
| 扩展要求 | Qt 6 图形界面 | ✅ |
| 扩展要求 | cron 定时备份与自动淘汰 | ✅ |
- 打包+压缩+加密可串联使用：`backup src dest --pack tar --compress zstd --encrypt aes256 --password pwd`
---

---
标题：图形界面展示
要点：
- 主窗口包含4个标签页：(1)备份标签页：源路径/目标路径选择、打包/压缩/加密选项、开始按钮；(2)还原标签页：备份源/还原目标选择、解密/解压/解包选项；(3)定时任务标签页：任务列表表格（cron表达式/下次执行时间）、添加/删除/启用按钮；(4)设置标签页。窗口底部为状态栏和进度条
- 基于 Qt 6 Widget 开发，4 个标签页：
  - **备份**：源/目标路径选择 + 筛选/打包/压缩/加密参数配置
  - **还原**：备份源选择 + 解密/解压/解包选项
  - **定时任务**：任务列表 + 添加/删除/启用/禁用管理
  - **设置**：通用配置
- 备份操作在 `QThread` 后台线程执行，不阻塞 UI
---

---
标题：软件开发工具链
要点：
- **版本控制**：Git + GitHub（PR 工作流、Issue 追踪、CI/CD）
- **集成开发**：VSCode + CMake + GCC/Clang/MSVC 三平台
- **项目管理**：GitHub Issues（Bug 追踪和任务分配）+ GitHub Projects
  - UML 建模：PlantUML（用例图、类图、顺序图、构件图）
- **单元测试**：Google Test（14 个测试文件覆盖全部功能模块）
- **内存检测**：Valgrind（`--leak-check=full`）
- **性能分析**：perf（内核级采样）+ gprof（`-pg` 编译插桩）
- **CI/CD**：GitHub Actions（三平台构建测试 + Docker 验证）
---

---
标题：自动化测试体系
要点：
- **单元测试**：Google Test，每个模块独立测试文件
  - core/: BackupEngine + RestoreEngine 集成测试
  - compress/: gzip/zstd/lzma 参数化测试
  - crypto/: AES/SM4 加解密验证
  - filter/: 6维度筛选逻辑全覆盖
  - fs/: 元数据 + 特殊文件读写
  - pack/: Tar/Zip 打包解包
  - scheduler/: 定时任务 + cron 表达式 + 淘汰策略
- **CI 流水线**（GitHub Actions）：
  - Linux GCC + Clang 双编译器矩阵构建
  - macOS AppleClang 构建
  - Windows MSVC 构建
  - Docker multi-stage 构建验证
- **本地检测**：Valgrind 内存泄漏检测 + perf/gprof 性能分析
---

---
标题：人员分工
要点：
- **古文斌** — 技术选型和架构设计
  - 核心备份/还原引擎实现
  - 压缩(gzip/zstd/lzma)、加密(AES/SM4)功能实现与测试
- **叶炜** — 测试和文档
  - 软件测试和测试报告编写
  - 测试用例设计与执行（Google Test + 端到端脚本）
  - PPT 制作与演示材料
- **谢松谕** — 编码实现和性能优化
  - 文件类型支持、元数据管理、6维度筛选器
  - Tar/Zip 打包解包、Qt 6 图形界面实现与测试
  - UML 图绘制、项目文档编写、演示视频制作
---

---
标题：项目进度与版本管理
要点：
- **开发流程**：基于 GitHub Flow 分支策略
  - main → release/分支 → feature/分支
  - 每个功能独立分支开发，PR 合入前需 CI 通过 + Code Review
- **Git 提交规范**：`<type>(<scope>): <description>` 格式
  - 如 `feat(core):` / `fix(cli):` / `docs(usage):` / `perf(ci):`
- **版本发布**：git tag + GitHub Release 自动发布
  - 当前版本：`release/airprofly_01-v1.0.0`
- **问题追踪**：GitHub Issues（bug report 模板 + 任务分配）
---

---
标题：项目亮点与难点
要点：
- **亮点**：
  - 可插拔管道架构：新增压缩/加密算法仅需 3 个文件（接口实现+工厂注册）
  - 跨平台三平台（Linux/macOS/Windows）CI 持续验证
  - 依赖零系统化：`git clone` → `cmake --build` 即可产出可执行软件
  - Full pipeline 串联：打包+压缩+加密可同时启用
- **难点**：
  - 特殊文件处理：设备号(major/minor)获取与重建需平台特定系统调用
  - 元数据还原顺序：属主→权限→时间戳需按内核预期顺序操作
  - 定时任务的时间计算：跨平台 cron 表达式解析 (ccronexpr)
  - Qt 6 跨平台构建：不同平台 Qt 安装路径和依赖差异大
---

---
标题：总结与展望
要点：
- **成果**：完成 158 分功能的数据备份软件，通过 CI 三平台构建验证和 Valgrind 内存检测
- **已实现**：核心备份还原、特殊文件、元数据、6 维度筛选、Tar/Zip 打包、3 种压缩、2 种加密、GUI 界面、定时备份
- **技术收获**：
  - 经历完整软件生命周期：需求→设计→编码→测试→部署
  - 掌握 CMake/FetchContent + 零系统依赖的跨平台构建技术
  - 运用 RAII、策略模式、工厂模式、管道模式等 C++ 设计模式
  - 实践 Git Flow、CI/CD、自动化测试等现代工程实践
- **可扩展方向**：
  - 实时备份（inotify 目录监控）
  - 网络备份（gRPC 客户端/服务端架构）
  - 增量备份（基于文件哈希比较）
---

---
标题：致谢 / Q&A
要点：
- 感谢课程指导老师的悉心指导
- 感谢团队成员的通力协作
- 项目地址：`https://github.com/airprofly/backer`
- 欢迎提问！
---

---
风格指引：清洁、简约、舒缓
要点：
- 整体风格：简约风格
- 色彩基调：以柔和低饱和度的主色点缀
- 传达感受：阅读轻松、视觉舒缓、令人感到舒适和专注
---
