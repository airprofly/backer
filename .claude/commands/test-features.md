测试前置：先确保已构建（`cmake --build build -j$(nproc)`）且测试数据已生成（`bash scripts/setup-testdata.sh`）。

## 范围

只测试受代码修改影响的功能，未修改的功能不测试，除非特别指明。

## 要求

1. **隔离**：每组测试前执行 `rm -rf data/backup data/restore` 清理上一组结果
2. **执行**：运行命令，同时捕获 stdout 和 stderr
3. **验证**：
   - 检查 CLI 输出中的成功/失败状态及统计信息（Files/Dirs/Size）
   - 对于筛选测试，关注日志中 `CriteriaFilter: N → M entries after filtering` 行
   - 检查目标目录的实际内容与预期一致（`find`、`ls -la`、`diff`、`readlink` 等）
   - 对异常路径（空目录、不存在的源、权限不足等）验证命令能优雅处理而非崩溃
