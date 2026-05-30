# BitTorrent Tracker Server

[![CI](https://github.com/chhy911/tracker-server/actions/workflows/ci.yml/badge.svg)](https://github.com/chhy911/tracker-server/actions/workflows/ci.yml)

高性能 BitTorrent Tracker 服务，C++17 + Boost.Asio + MySQL，内置 REST API 与 React 监控仪表板。

## 功能特性

- **异步网络**：Boost.Asio，可配置 worker 线程与最大连接数
- **BEP Announce**：标准 HTTP announce 协议处理
- **MySQL 持久化**：种子与 peer 信息、连接池
- **REST API**（8080）：统计、健康检查、peer 查询
- **Web 仪表板**（3000）：实时连接数、QPS、做种/下载统计
- **Docker 一键部署**：MySQL + Tracker 编排

## 项目结构

```
tracker-server/
├── src/
│   ├── main.cpp
│   ├── tracker/          # Tracker 核心与 BEP
│   ├── database/         # MySQL 与连接池
│   ├── api/              # REST + HTTP 服务
│   └── utils/            # 配置与日志
├── dashboard/            # React (Vite) 仪表板
├── sql/init.sql
├── config/tracker.conf
├── docker/
│   ├── Dockerfile
│   ├── docker-compose.yml
│   └── supervisor.conf
├── CMakeLists.txt
└── scripts/init.sh
```

## 系统要求

- Linux（推荐 Ubuntu 22.04+）或 Docker
- C++17、CMake 3.15+
- Boost、libcurl、MySQL client
- Node.js 18+（仅构建 dashboard）

## 快速开始

### Docker（推荐）

```bash
git clone https://github.com/chhy911/tracker-server.git
cd tracker-server/docker

# 可选：复制环境变量
cp ../.env.example ../.env

docker compose up -d --build
```

访问：

| 服务 | 地址 |
|------|------|
| 仪表板 | http://localhost:3000 |
| REST API | http://localhost:8080 |
| Tracker | `0.0.0.0:6969` |

数据库主机由环境变量 `TRACKER_DB_HOST` 注入（Compose 默认为 `mysql`）。

### 本地构建

```bash
# 依赖 (Ubuntu)
sudo apt update
sudo apt install -y build-essential cmake libboost-all-dev \
  libcurl4-openssl-dev libmysqlclient-dev mysql-server

# 数据库
mysql -u root -p < sql/init.sql

# 前端
cd dashboard && npm install && npm run build && cd ..

# 后端
mkdir -p build && cd build
cmake ..
make -j$(nproc)
cd ..

mkdir -p logs
./build/tracker-server config/tracker.conf
```

开发时前端热更新：

```bash
cd dashboard && npm run dev
```

`vite` 会将 `/api` 代理到 `http://localhost:8080`。

## 配置

`config/tracker.conf` 主要段落：

```ini
[server]
host=0.0.0.0
port=6969
worker_threads=8
max_connections=300

[database]
host=localhost
port=3306
user=tracker
password=tracker_password
database=tracker_db
pool_size=50

[api]
host=0.0.0.0
port=8080

[dashboard]
host=0.0.0.0
port=3000
static_path=dashboard/dist
```

环境变量覆盖数据库连接（Docker 部署时使用）：

- `TRACKER_DB_HOST`
- `TRACKER_DB_PORT`
- `TRACKER_DB_USER`
- `TRACKER_DB_PASSWORD`
- `TRACKER_DB_NAME`

## API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/api/health` | 健康检查 |
| GET | `/api/stats` | 全局做种/下载统计 |
| GET | `/api/metrics` | 活跃连接、总请求、QPS |
| GET | `/api/torrent/:info_hash` | 单个种子统计 |
| GET | `/api/peers/:info_hash?limit=50` | Peer 列表 |

Tracker announce（6969）：

```http
GET /announce?info_hash=...&peer_id=...&port=...&uploaded=...&downloaded=...&left=...
```

## 推送到 GitHub

```bash
git checkout -b feature/my-change
git add .
git commit -m "描述你的改动"
git push -u origin feature/my-change
```

在 GitHub 上创建 Pull Request；CI 会自动编译 C++ 与构建 dashboard。详见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

MIT

## 贡献

欢迎 Issue 与 Pull Request。
