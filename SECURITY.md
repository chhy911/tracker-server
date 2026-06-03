# 安全说明

## 默认凭据

仓库内 `tracker_password`、`root_password` 仅用于**本地开发 / Docker 演示**。上线后必须：

1. 修改 MySQL `tracker` 用户密码，并同步 `config/tracker.conf` 或环境变量 `TRACKER_DB_PASSWORD`。
2. 删除或收紧 `sql/init.sql` 中的 `tracker@'%'`，仅允许 `localhost`。
3. 勿将 `DEPLOY_PASSWORD` 写入代码或提交到 Git。

## REST API

- 无认证，CORS 为 `*`。
- 公网仅通过 Nginx **8888** 同域访问即可；避免将 **8081** 直接映射到公网安全组。

## SQL

- `info_hash` / `peer_id` 须为 40 位十六进制；非法输入在 API 与 DB 层拒绝。
- 仍建议生产环境使用参数化查询（后续改进）。

## SSH

- 使用密钥登录，禁用 root 密码登录（可选）。
- 部署脚本通过环境变量 `DEPLOY_PASSWORD` 传入，用完即 unset。
