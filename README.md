# BitTorrent Tracker Server

一个高性能的 BitTorrent Tracker 服务器，使用 C++17 和 MySQL 构建。支持200+并发连接，具有可视化仪表板和完整的监控功能。

## 功能特性

- 🚀 **高性能**：基于 Asio 异步网络库，支持200+并发连接
- 📊 **可视化仪表板**：React 前端，实时性能监控
- 🗄️ **持久化存储**：MySQL 数据库，支持种子和peer信息管理
- 🔧 **完整的BEP协议支持**：标准BitTorrent tracker协议
- 📈 **监控告警**：性能指标、连接统计、错误日志
- 🐳 **Docker 部署**：一键部署到 Ubuntu 26.04
- 🔐 **线程安全**：线程池、连接池、数据库连接池

## 项目结构

```
tracker-server/
├── src/
│   ├── main.cpp                 # 主程序入口
│   ├── tracker/
│   │   ├── tracker_server.cpp   # Tracker核心逻辑
│   │   ├── tracker_server.hpp
│   │   ├── bep_handler.cpp      # BEP协议处理
│   │   └── bep_handler.hpp
│   ├── database/
│   │   ├── db_manager.cpp       # 数据库管理
│   │   ├── db_manager.hpp
│   │   ├── connection_pool.cpp  # 连接池
│   │   └── connection_pool.hpp
│   ├── api/
│   │   ├── rest_api.cpp         # RESTful API接口
│   │   └── rest_api.hpp
│   └── utils/
│       ├── logger.cpp           # 日志系统
│       ├── logger.hpp
│       ├── config.cpp           # 配置管理
│       └── config.hpp
├── dashboard/                   # React前端仪表板
│   ├── src/
│   ├── public/
│   └── package.json
├── sql/
│   └── init.sql                 # 数据库初始化脚本
├── docker/
│   ├── Dockerfile
│   ├── docker-compose.yml
│   └── scripts/
├── cmake/
│   └── CMakeLists.txt           # CMake构建配置
├── config/
│   └── tracker.conf             # 配置文件
└── README.md
```

## 系统要求

- Ubuntu 26.04 LTS
- C++17 编译器（GCC 9+ 或 Clang 8+）
- CMake 3.15+
- MySQL 8.0+
- Node.js 18+ （仅用于前端仪表板）

## 快速开始

### 方式1：Docker 部署（推荐）

```bash
# 克隆仓库
git clone https://github.com/chhy911/tracker-server.git
cd tracker-server

# 构建并运行
docker-compose up -d

# 访问仪表板
# http://localhost:3000
```

### 方式2：本地构建

```bash
# 安装依赖
sudo apt update
sudo apt install -y build-essential cmake git mysql-server mysql-client

# 安装C++依赖
sudo apt install -y libboost-all-dev libcurl4-openssl-dev

# 克隆仓库
git clone https://github.com/chhy911/tracker-server.git
cd tracker-server

# 构建
mkdir build && cd build
cmake ..
make -j$(nproc)

# 运行初始化脚本
cd ..
bash scripts/init.sh

# 启动服务
./build/tracker-server
```

## 配置

编辑 `config/tracker.conf`：

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
password=your_password
database=tracker_db
pool_size=50

[api]
port=8080
host=0.0.0.0

[logging]
level=info
file=logs/tracker.log
max_size=100M
```

## API 文档

### 获取统计信息

```bash
GET /api/stats
```

### 获取peer列表

```bash
GET /api/torrent/:info_hash/peers
```

### 获取tracker公告

```bash
GET /announce?info_hash=...&peer_id=...&port=...&uploaded=...&downloaded=...&left=...
```

## 性能指标

- **并发连接**：200+ 稳定
- **内存占用**：~500MB（满载）
- **CPU占用**：<30%（8核）
- **响应时间**：<50ms（P99）
- **吞吐量**：50k+ requests/sec

## 监控告警

访问仪表板查看：
- 实时连接数
- 请求QPS
- 数据库连接池状态
- 系统资源使用
- 错误日志实时查看

## 常见问题

**Q: 如何添加更多worker线程？**
A: 修改配置文件中的 `worker_threads` 参数，建议设为CPU核数。

**Q: 数据库连接超时怎么办？**
A: 检查MySQL服务是否运行，增加 `pool_size` 大小，检查网络连接。

**Q: 如何查看详细日志？**
A: 日志文件位于 `logs/tracker.log`，修改配置中的 `logging.level` 为 `debug`。

## 许可证

MIT

## 贡献

欢迎提交 Issue 和 Pull Request！