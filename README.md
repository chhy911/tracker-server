# BitTorrent Tracker Server

[![CI](https://github.com/chhy911/tracker-server/actions/workflows/ci.yml/badge.svg)](https://github.com/chhy911/tracker-server/actions/workflows/ci.yml)

高性能 BitTorrent Tracker 服务，**C++17 + Boost.Asio + MySQL + React 仪表板**，支持 BEP 标准 announce / scrape，内置 REST API。

## 功能特性

- **异步网络**：Boost.Asio，可配置 worker 线程与最大连接数
- **BEP Announce & Scrape**：标准 HTTP announce 与 `/scrape` 端点（BEP-48 compact）
- **MySQL 持久化**：种子与 peer 信息、连接池、定时清理过期 peer
- **REST API**（默认 **8081**）：统计、健康检查（含 DB ping）、peer 查询、分页
- **Web 仪表板**（**8888**）：实时 Sparkline 图表、Peer 详情弹窗、种子分页列表
- **日志轮转**：按文件大小自动轮转，保留备份
- **Docker 编排**：MySQL + Tracker + Nginx(8888) 三服务

---

## 目录结构

```
tracker-server/
├── src/
│   ├── main.cpp                # 入口、定时清理线程
│   ├── tracker/                # BEP announce/scrape、连接管理
│   ├── database/               # MySQL 连接池、CRUD
│   ├── api/                    # REST API、静态 HTTP 服务
│   └── utils/                  # 配置解析、日志、SQL 工具
├── dashboard/                  # React 18 + Vite 仪表板
├── config/
│   ├── tracker.conf            # 主配置
│   └── tracker-server.service  # systemd 单元
├── docker/                     # Dockerfile、docker-compose、nginx.conf
├── scripts/                    # 部署与运维脚本
├── sql/init.sql                # 数据库初始化
├── DEPLOY.md                   # 生产部署详细指南
└── SECURITY.md                 # 安全说明
```

---

## 快速开始

### 方式一：Docker（推荐本地测试）

**前置：** Docker Desktop 或 Docker Engine（含 `docker compose`）

```bash
# 1. 克隆仓库
git clone https://github.com/chhy911/tracker-server.git
cd tracker-server

# 2. 可选：复制并修改环境变量
cp .env.example .env

# 3. 启动全部服务（MySQL + Tracker + Nginx）
cd docker
docker compose up -d --build
```

| 服务 | 地址 | 说明 |
|------|------|------|
| 仪表板 + API | http://localhost:8888 | Nginx 托管 |
| REST API | http://localhost:8081 | 容器直连/调试 |
| Tracker | `udp/tcp://localhost:6969` | BEP announce |

```bash
# 查看日志
docker compose logs -f tracker

# 停止
docker compose down
```

> **国内 ECS** 80/443 端口常被运营商/WAF 封堵，生产请用 Nginx 监听 **8888** 并在云控制台安全组放行该端口。

---

### 方式二：本地编译（Ubuntu 22.04+）

#### 1. 安装系统依赖

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake \
  libboost-all-dev \
  libmysqlclient-dev \
  mysql-server \
  nodejs npm
```

#### 2. 初始化数据库

```bash
# 启动 MySQL
sudo systemctl start mysql

# 创建数据库、用户与表
sudo mysql < sql/init.sql
```

#### 3. 构建前端

```bash
cd dashboard
npm install
npm run build   # 输出到 dashboard/dist/
cd ..
```

#### 4. 编译后端

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
cd ..
```

#### 5. 运行

```bash
mkdir -p logs
./build/tracker-server config/tracker.conf
```

**前端开发热更新**（Vite 代理 API 到 8081）：

```bash
cd dashboard && npm run dev
```

---

### 方式三：生产部署到 ECS（推荐）

#### 前置条件

| 条件 | 说明 |
|------|------|
| Linux ECS | Ubuntu 22.04 或更高版本 |
| SSH 访问 | root 或 sudo 权限 |
| 安全组 | 放行 **8888**（Web/API）、**6969**（Tracker） |
| Python 本机 | `pip install paramiko` |

#### 步骤

**1. 在本机设置部署密码**

```bash
# Linux/macOS
export DEPLOY_PASSWORD='你的服务器root密码'
export DEPLOY_HOST='你的ECS公网IP'    # 默认 59.110.14.226

# Windows PowerShell
$env:DEPLOY_PASSWORD='你的服务器root密码'
$env:DEPLOY_HOST='你的ECS公网IP'
```

**2. 执行全量部署脚本**

```bash
python scripts/deploy_upload.py
```

该脚本会自动完成：

1. 打包源码（排除 build/node_modules）上传至 `/opt/tracker-server`
2. 安装系统依赖（cmake、boost、mysql-client、nodejs、nginx）
3. 初始化 MySQL 数据库
4. 添加 2 GB Swap（小内存机器）
5. 构建前端（`npm run build`）
6. 编译 C++ 后端（`cmake + make`）
7. 配置 Nginx 8888（静态文件 + `/api` 反代）
8. 安装 systemd 服务（开机自启、失败自动重启）
9. 设置 cron 健康看门狗（每 2 分钟检查，失败则重启）
10. 运行冒烟测试验证服务是否正常

**部署完成标志：**

```
{"status":"ok","db":"ok"} API OK
Web 8888 200
smoke_test OK
DEPLOY_DONE
```

**3. 验证服务**

```bash
# 仪表板
curl http://你的IP:8888/

# API 健康检查
curl http://你的IP:8888/api/health
# 返回: {"status":"ok","db":"ok"}

# Tracker（无参数返回 400，属正常）
curl http://你的IP:6969/announce
```

#### 后续增量更新

代码修改后只需：

```bash
python scripts/deploy_upload.py
```

或仅在服务器上重新编译并重启：

```bash
python scripts/apply_production.py
```

#### 服务管理

```bash
# SSH 登录服务器后
systemctl status tracker-server     # 查看状态
systemctl restart tracker-server    # 重启
journalctl -u tracker-server -n 50  # 查看日志
tail -f /opt/tracker-server/logs/tracker.log
```

---

## 配置说明

`config/tracker.conf` 全部配置项：

```ini
[server]
host=0.0.0.0
port=6969
worker_threads=4          # 建议 = CPU 核数，小内存 VPS 用 2-4
max_connections=200

[database]
host=localhost
port=3306
user=tracker
password=tracker_password
database=tracker_db
pool_size=20

[api]
host=0.0.0.0
port=8081                 # 8080 若被占用请改为 8081

[dashboard]
enabled=false             # 生产由 Nginx 提供静态文件，此处保持 false
port=3000
static_path=dashboard/dist

[logging]
level=info
file=logs/tracker.log
max_size=100              # MB，超出后自动轮转
backup_count=5

[tracker]
announce_interval=1800    # 客户端 announce 间隔（秒）
min_announce_interval=600
num_want=50               # 每次 announce 最多返回的 peer 数
cleanup_interval=3600     # 后台清理过期 peer 的间隔（秒）
max_peer_age=3600         # peer 超过此时间不活跃则视为过期（秒）

[performance]
request_timeout=5000      # 毫秒
buffer_size=8192
```

**环境变量**（优先级高于配置文件，适合 Docker/CI）：

| 变量 | 说明 |
|------|------|
| `TRACKER_DB_HOST` | 数据库主机 |
| `TRACKER_DB_PORT` | 数据库端口 |
| `TRACKER_DB_USER` | 数据库用户 |
| `TRACKER_DB_PASSWORD` | 数据库密码 |
| `TRACKER_DB_NAME` | 数据库名 |

---

## REST API

Base URL：`http://你的IP:8888/api`（或 `http://你的IP:8081/api` 直连）

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/health` | 健康检查，含 DB 状态 |
| GET | `/api/stats` | 全局统计（种子数、peer 数、做种/下载中、QPS 等） |
| GET | `/api/metrics` | 连接数、总请求数、QPS |
| GET | `/api/torrents` | 种子列表，支持 `?page=1&limit=20` 分页 |
| GET | `/api/torrent/:info_hash` | 单种子详情（40 位 hex） |
| GET | `/api/peers/:info_hash?limit=50` | 该种子的 Peer 列表 |

**Tracker**：

```
GET http://你的IP:6969/announce?info_hash=...&peer_id=...&port=...&uploaded=...&downloaded=...&left=...
GET http://你的IP:6969/scrape?info_hash=...
```

---

## 客户端配置

在 qBittorrent / Transmission 等客户端的种子 **Tracker 列表** 中添加：

```
http://你的ECS公网IP:6969/announce
```

---

## 安全注意事项

- 生产环境**务必修改** `tracker_password`、`root_password` 等默认密码
- `DEPLOY_PASSWORD` 仅通过环境变量传入，切勿写入代码或提交到 Git
- REST API 暂无鉴权，建议不要把 8081 直接暴露到公网安全组（通过 Nginx 8888 访问即可）
- 详细安全说明见 [SECURITY.md](SECURITY.md)

---

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。CI 自动编译 C++ 与构建 dashboard。

## 许可证

MIT
