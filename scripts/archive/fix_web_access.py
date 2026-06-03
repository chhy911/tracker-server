#!/usr/bin/env python3
"""Rebuild dashboard (same-origin API) and enable nginx on port 80."""
import os
import sys
import time
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

REMOTE = r"""
set -e
export DEBIAN_FRONTEND=noninteractive

echo "=== Rebuild dashboard (same-origin /api) ==="
cd /opt/tracker-server/dashboard
# Upload fresh App.jsx happens via sftp before this
export VITE_API_URL=
npm run build --silent

echo "=== Install nginx :80 ==="
apt-get update -qq
apt-get install -y -qq nginx

cat > /etc/nginx/sites-available/tracker <<'NGINX'
server {
    listen 80 default_server;
    listen [::]:80 default_server;
    server_name _;

    location /api/ {
        proxy_pass http://127.0.0.1:8081/api/;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    location / {
        proxy_pass http://127.0.0.1:3000/;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
NGINX

rm -f /etc/nginx/sites-enabled/default
ln -sf /etc/nginx/sites-available/tracker /etc/nginx/sites-enabled/tracker
nginx -t
systemctl enable nginx
systemctl restart nginx

systemctl restart tracker-server
sleep 2

echo "=== Checks ==="
curl -s -o /dev/null -w "local80:%{http_code}\n" http://127.0.0.1/
curl -s http://127.0.0.1/api/health
echo ""
ss -tlnp | grep -E ':80 |:3000|:8081' || true
echo FIX_DONE
"""


def main():
    if not PASSWORD:
        print("Set DEPLOY_PASSWORD", file=sys.stderr)
        sys.exit(1)

    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)

    sftp = c.open_sftp()
    sftp.put(
        os.path.join(REPO, "dashboard", "src", "App.jsx"),
        "/opt/tracker-server/dashboard/src/App.jsx",
    )
    sftp.close()

    _, stdout, stderr = c.exec_command(REMOTE, timeout=300)
    ch = stdout.channel
    while not ch.exit_status_ready():
        if ch.recv_ready():
            sys.stdout.buffer.write(ch.recv(4096))
            sys.stdout.buffer.flush()
        time.sleep(0.3)
    while ch.recv_ready():
        sys.stdout.buffer.write(ch.recv(4096))
    code = ch.recv_exit_status()
    print("exit:", code)
    c.close()
    sys.exit(code)


if __name__ == "__main__":
    main()
