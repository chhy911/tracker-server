#!/usr/bin/env python3
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

REMOTE = r"""
set -e

echo "=== Before fix ==="
curl -s http://127.0.0.1:8888/api/health | head -c 60; echo
curl -s http://127.0.0.1:3000/api/health | head -c 60; echo

echo "=== Nginx ==="
cat > /etc/nginx/sites-available/tracker <<'NGINX'
server {
    listen 8888;
    listen [::]:8888;
    server_name _;

    # API 必须优先匹配，避免落到静态页返回 HTML
    location ^~ /api/ {
        proxy_pass http://127.0.0.1:8081;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
NGINX
ln -sf /etc/nginx/sites-available/tracker /etc/nginx/sites-enabled/tracker
nginx -t && systemctl restart nginx

echo "=== Rebuild dashboard (same-origin /api) ==="
cd /opt/tracker-server/dashboard
export VITE_API_URL=
npm run build --silent

cd /opt/tracker-server/build
make -j$(nproc)
systemctl restart tracker-server
sleep 2

echo "=== After fix ==="
curl -s http://127.0.0.1:8888/api/health; echo
curl -s http://127.0.0.1:8888/api/stats; echo
curl -s http://127.0.0.1:3000/api/health | head -c 80; echo
echo DONE
"""


def main():
    if not PASSWORD:
        sys.exit("Set DEPLOY_PASSWORD")
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    sftp = c.open_sftp()
    sftp.put(os.path.join(REPO, "src", "api", "http_server.cpp"),
             "/opt/tracker-server/src/api/http_server.cpp")
    sftp.put(os.path.join(REPO, "dashboard", "src", "App.jsx"),
             "/opt/tracker-server/dashboard/src/App.jsx")
    sftp.close()
    _, stdout, _ = c.exec_command(REMOTE, timeout=400)
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
