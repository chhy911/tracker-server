#!/bin/bash

set -e

echo "=== BitTorrent Tracker Server Initialization Script ==="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Check if running on Ubuntu
if [ ! -f /etc/os-release ]; then
    echo -e "${RED}Error: Cannot detect OS${NC}"
    exit 1
fi

source /etc/os-release
if [[ "$ID" != "ubuntu" ]]; then
    echo -e "${RED}Error: This script is designed for Ubuntu. Current OS: $ID${NC}"
    exit 1
fi

echo -e "${GREEN}Detected OS: $ID $VERSION_ID${NC}"

# 1. Update system
echo -e "${YELLOW}Step 1: Updating system packages...${NC}"
sudo apt-get update
sudo apt-get upgrade -y

# 2. Install dependencies
echo -e "${YELLOW}Step 2: Installing dependencies...${NC}"
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    curl \
    wget \
    libboost-all-dev \
    libcurl4-openssl-dev \
    libmysqlclient-dev \
    mysql-server \
    mysql-client \
    nodejs \
    npm

# 3. Create directories
echo -e "${YELLOW}Step 3: Creating directories...${NC}"
mkdir -p logs
mkdir -p config

# 4. Build tracker server
echo -e "${YELLOW}Step 4: Building tracker server...${NC}"
if [ ! -d build ]; then
    mkdir build
fi
cd build
cmake ..
make -j$(nproc)
cd ..

echo -e "${GREEN}Build completed${NC}"

# 5. Initialize MySQL database
echo -e "${YELLOW}Step 5: Initializing MySQL database...${NC}"
echo "Please enter MySQL root password when prompted:"
mysql -u root -p < sql/init.sql

echo -e "${GREEN}Database initialized${NC}"

# 6. Create tracker user (optional)
if ! id "tracker" &>/dev/null 2>&1; then
    echo -e "${YELLOW}Step 6: Creating tracker system user...${NC}"
    sudo useradd -r -s /bin/bash -d /opt/tracker-server tracker
    echo -e "${GREEN}Tracker user created${NC}"
else
    echo -e "${YELLOW}Step 6: Tracker user already exists${NC}"
fi

# 7. Set permissions
echo -e "${YELLOW}Step 7: Setting permissions...${NC}"
sudo chown -R tracker:tracker .
sudo chmod 755 build/tracker-server

# 8. Install dashboard dependencies
echo -e "${YELLOW}Step 8: Installing dashboard dependencies...${NC}"
cd dashboard || exit 1
npm install
npm run build
cd ..

echo -e "${GREEN}Dashboard setup completed${NC}"

# 9. Create systemd service file
echo -e "${YELLOW}Step 9: Creating systemd service file...${NC}"
TRACKER_DIR=$(pwd)
sudo tee /etc/systemd/system/tracker-server.service > /dev/null <<EOF
[Unit]
Description=BitTorrent Tracker Server
After=network.target mysql.service
Wants=mysql.service

[Service]
Type=simple
User=tracker
WorkingDirectory=${TRACKER_DIR}
ExecStart=${TRACKER_DIR}/build/tracker-server ${TRACKER_DIR}/config/tracker.conf
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload

echo -e "${GREEN}Service file created${NC}"

# Summary
echo ""
echo -e "${GREEN}=== Installation Complete ===${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Edit configuration file:"
echo "   sudo nano config/tracker.conf"
echo ""
echo "2. Update database credentials in config/tracker.conf"
echo ""
echo "3. Start the service:"
echo "   sudo systemctl start tracker-server"
echo ""
echo "4. Enable service to start on boot:"
echo "   sudo systemctl enable tracker-server"
echo ""
echo "5. Check service status:"
echo "   sudo systemctl status tracker-server"
echo ""
echo "6. View logs:"
echo "   tail -f logs/tracker.log"
echo ""
echo -e "${YELLOW}Tracker will listen on:${NC}"
echo "  - BitTorrent: 0.0.0.0:6969"
echo "  - API Server: 0.0.0.0:8080"
echo "  - Dashboard: 0.0.0.0:3000"
echo ""
