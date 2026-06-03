# BitTorrent Tracker Server

[![CI](https://github.com/chhy911/tracker-server/actions/workflows/ci.yml/badge.svg)](https://github.com/chhy911/tracker-server/actions/workflows/ci.yml)

高性能 BitTorrent Tracker 服务，C++17 + Boost.Asio + MySQL，内置 REST API 与 React 监控仪表板。

## 功能特性

- **异步网络**：Boost.Asio，可配置 worker 线程与最大连接数
- **BEP Announce**：标准 HTTP announce 协议处理
- **MySQL 持久化**：种子与 peer 信息、连接池
- **REST API**（默认 **8081**）：统计、健康检查、peer 查询
- **Web 仪表板**：生产由 **Nginx 8888** 托管静态文件；开发可用 Vite
- **Docker 编排**：MySQL + Tracker + Nginx(8888)

## 项目结构

```
tracker-server/
├── src/
├── dashboard/
├── config/tracker.conf
├── docker/
├── scripts/          # 部署与运维（见 DEPLOY.md）
├── DEPLOY.md
└── SECURITY.md
```

## 系统要求

- Linux（推荐 Ubuntu 22.04+）或 Docker
- C++17、CMake 3.15+
- Boost、MySQL client（**无需 libcurl**）
- Node.js 18+（构建 dashboard）

## 快速开始

### Docker

```bash
cd docker
cp ../.env.example ../.env   # 可选
docker compose up -d --build
```

| 服务 | 地址 |
|------|------|
| 仪表板 + API（推荐） | http://localhost:8888 |
| REST API（容器内/调试） | http://localhost:8081 |
| Tracker | `0.0.0.0:6969` |

> 国内 ECS：**80/443 常被封**，生产用 Nginx **8888**（见 [DEPLOY.md](DEPLOY.md)）。

### 本地构建

```bash
sudo apt install -y build-essential cmake libboost-all-dev \
  libmysqlclient-dev mysql-server

mysql -u root -p < sql/init.sql

cd dashboard && npm install && npm run build && cd ..

mkdir -p build && cd build && cmake .. && make -j$(nproc) && cd ..

mkdir -p logs
./build/tracker-server config/tracker.conf
```

开发前端（Vite 代理 API 到 8081）：

```bash
cd dashboard && npm run dev
```

`vite.config.js` 默认代理到 `http://localhost:8081`。

### 生产部署（ECS）

```bash
export DEPLOY_PASSWORD='...'
python scripts/deploy_upload.py
```

详见 [DEPLOY.md](DEPLOY.md)、[SECURITY.md](SECURITY.md)。

## 配置

`config/tracker.conf` 要点：

```ini
[server]
worker_threads=4
max_connections=200

[database]
pool_size=20

[api]
port=8081

[dashboard]
enabled=false
```

环境变量可覆盖数据库：`TRACKER_DB_HOST`、`TRACKER_DB_PORT`、`TRACKER_DB_USER`、`TRACKER_DB_PASSWORD`、`TRACKER_DB_NAME`。

## API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/health` | 健康检查 |
| GET | `/api/stats` | 全局统计 |
| GET | `/api/metrics` | 连接数、QPS |
| GET | `/api/torrents` | 种子列表 |
| GET | `/api/torrent/:info_hash` | 单种统计（40 位 hex） |
| GET | `/api/peers/:info_hash` | Peer 列表 |

Tracker：`GET http://host:6969/announce?...`

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。CI 编译 C++ 与 dashboard。

## 许可证

MIT
