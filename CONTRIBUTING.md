# 贡献指南

感谢参与 [tracker-server](https://github.com/chhy911/tracker-server) 的开发！

## 开发环境

1. 克隆仓库并安装 C++ 依赖（Ubuntu / Debian）：

   ```bash
   sudo apt install build-essential cmake libboost-all-dev \
     libcurl4-openssl-dev libmysqlclient-dev mysql-server
   ```

2. 初始化数据库：`mysql -u root -p < sql/init.sql`

3. 构建前端：

   ```bash
   cd dashboard && npm install && npm run build && cd ..
   ```

4. 编译服务端：

   ```bash
   mkdir build && cd build && cmake .. && make -j$(nproc)
   ```

5. 编辑 `config/tracker.conf` 后启动：

   ```bash
   ./build/tracker-server ../config/tracker.conf
   ```

## 分支与提交

- 从 `main` 拉取功能分支：`git checkout -b feature/your-feature`
- 提交信息用英文或中文均可，需说明「做了什么」和「为什么」
- 推送后通过 GitHub 创建 Pull Request

## Pull Request 检查清单

- [ ] 本地 `cmake` 编译通过
- [ ] `dashboard` 能 `npm run build`
- [ ] 若改 API，已更新 README 中的接口说明
- [ ] 未提交密钥、`.env` 或本地 `config/tracker.conf.local`

## 报告问题

在 Issue 中请包含：操作系统、复现步骤、期望行为、相关日志（`logs/tracker.log`）。
