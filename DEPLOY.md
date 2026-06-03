# 生产部署指南

目标环境：国内 ECS（示例 `59.110.14.226`），**8888** 访问 Web + API，**6969** Tracker。

## 架构

| 端口 | 服务 | 说明 |
|------|------|------|
| 8888 | Nginx | 静态 `dashboard/dist`，`/api/` → 127.0.0.1:8081 |
| 8081 | tracker-server | REST API（`config/tracker.conf` `[api] port=8081`） |
| 6969 | tracker-server | BitTorrent announce |
| 3000 | （默认关闭） | `dashboard.enabled=false`，勿作为入口 |

## 一次性全量部署

```bash
export DEPLOY_PASSWORD='你的root密码'
export DEPLOY_HOST=59.110.14.226   # 可选
python scripts/deploy_upload.py
```

## 代码已上传后的增量更新

```bash
export DEPLOY_PASSWORD='...'
python scripts/apply_production.py
```

## 运维脚本（保留）

| 脚本 | 用途 |
|------|------|
| `deploy_upload.py` | 打包上传 + 编译 + Nginx + systemd |
| `apply_production.py` | 服务器上已有代码时重新编译并套用生产配置 |
| `setup_nginx.sh` / `setup_nginx_alt_port.py` | 配置 Nginx 8888 |
| `setup_swap.sh` | 添加 2GB swap（小内存机器） |
| `stabilize_web.py` | 紧急修复 8888 积压（静态化 + 重启） |
| `deep_diagnose.py` | 远程诊断 |
| `verify_deploy.py` | 快速验证 API |
| `recover_services.py` | 重启 tracker/nginx |
| `smoke_test.sh` | 本地/远程 API 冒烟 |

历史一次性修复脚本已移至 `scripts/archive/`。

## 安全组

放行：**8888**、**6969**（按需 **8081** 仅内网，勿对公网开放亦可）。

## 客户端

每个种子添加 Tracker：`http://你的IP:6969/announce`

## 生产加固

见 [SECURITY.md](SECURITY.md)：修改 MySQL 默认密码、限制 `tracker@'%'`、API 无鉴权勿暴露敏感网络。
