#!/usr/bin/env python3
"""Serve dashboard via nginx static files; proxy only /api to tracker."""
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

FIX = r"""
set -e
STATIC=/opt/tracker-server/dashboard/dist

# 2GB swap (small VPS)
if ! swapon --show | grep -q /swapfile; then
  if [ ! -f /swapfile ]; then
    fallocate -l 2G /swapfile 2>/dev/null || dd if=/dev/zero of=/swapfile bs=1M count=2048
    chmod 600 /swapfile
    mkswap /swapfile
  fi
  swapon /swapfile 2>/dev/null || true
  grep -q /swapfile /etc/fstab || echo '/swapfile none swap sw 0 0' >> /etc/fstab
fi

# Free stuck connections by restarting tracker
systemctl restart tracker-server
sleep 2

cat > /etc/nginx/sites-available/tracker <<'NGINX'
server {
    listen 8888;
    listen [::]:8888;
    server_name _;

    root /opt/tracker-server/dashboard/dist;
    index index.html;

    client_max_body_size 1m;
    send_timeout 30s;

    location /api/ {
        proxy_pass http://127.0.0.1:8081;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_connect_timeout 5s;
        proxy_send_timeout 15s;
        proxy_read_timeout 15s;
    }

    location /assets/ {
        try_files $uri =404;
        expires 7d;
        add_header Cache-Control "public, immutable";
    }

    location / {
        try_files $uri /index.html;
    }
}
NGINX

ln -sf /etc/nginx/sites-available/tracker /etc/nginx/sites-enabled/tracker
nginx -t
systemctl restart nginx
sleep 2

echo "=== backlog check ==="
ss -tlnp | grep -E '8888|8081|3000' || true

echo "=== health ==="
for i in 1 2 3 4 5; do
  curl -s -m 5 -o /dev/null -w "8888_$i:%{http_code} %{time_total}s\n" http://127.0.0.1:8888/ || echo "8888_$i:fail"
done
curl -s -m 5 http://127.0.0.1:8888/api/health
echo ""

# watchdog cron every 2 min
cat > /etc/cron.d/tracker-watch <<'CRON'
*/2 * * * * root curl -sf -m 5 http://127.0.0.1:8888/api/health >/dev/null || (systemctl restart tracker-server; systemctl restart nginx)
CRON
chmod 644 /etc/cron.d/tracker-watch

cat > /etc/systemd/system/tracker-server.service <<'UNIT'
[Unit]
Description=BitTorrent Tracker Server
After=network.target mysql.service
Wants=mysql.service

[Service]
Type=simple
WorkingDirectory=/opt/tracker-server
ExecStart=/opt/tracker-server/build/tracker-server /opt/tracker-server/config/tracker.conf
Restart=always
RestartSec=5
TimeoutStopSec=15
KillMode=mixed
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
UNIT

systemctl daemon-reload
systemctl enable tracker-server nginx mysql
echo STABLE_DONE
"""


def main():
    if not PASSWORD:
        sys.exit("Set DEPLOY_PASSWORD")
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=20, allow_agent=False, look_for_keys=False)
    _, stdout, _ = c.exec_command(FIX, timeout=120)
    ch = stdout.channel
    while not ch.exit_status_ready():
        if ch.recv_ready():
            sys.stdout.buffer.write(ch.recv(4096))
            sys.stdout.buffer.flush()
        import time
        time.sleep(0.2)
    while ch.recv_ready():
        sys.stdout.buffer.write(ch.recv(4096))
    print("exit:", ch.recv_exit_status())
    c.close()


if __name__ == "__main__":
    main()
