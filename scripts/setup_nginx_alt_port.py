#!/usr/bin/env python3
"""Nginx reverse proxy on non-blocked port (default 8888) for China VPS."""
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
USER = os.environ.get("DEPLOY_USER", "root")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
WEB_PORT = os.environ.get("WEB_PORT", "8888")

REMOTE = f"""set -e
export DEBIAN_FRONTEND=noninteractive

apt-get install -y -qq nginx 2>/dev/null || true

cat > /etc/nginx/sites-available/tracker <<'NGINX'
server {{
    listen {WEB_PORT};
    listen [::]:{WEB_PORT};
    server_name _;

    root /opt/tracker-server/dashboard/dist;
    index index.html;

    location /api/ {{
        proxy_pass http://127.0.0.1:8081;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_connect_timeout 5s;
        proxy_read_timeout 15s;
    }}

    location /assets/ {{
        try_files $uri =404;
        expires 7d;
    }}

    location / {{
        try_files $uri /index.html;
    }}
}}
NGINX

# Disable default :80 site (blocked in CN anyway)
rm -f /etc/nginx/sites-enabled/default
ln -sf /etc/nginx/sites-available/tracker /etc/nginx/sites-enabled/tracker

nginx -t
systemctl enable nginx
systemctl restart nginx
systemctl restart tracker-server 2>/dev/null || true
sleep 2

echo "=== listen ==="
ss -tlnp | grep -E ':{WEB_PORT} |:3000|:8081' || true
curl -s -o /dev/null -w "web:{WEB_PORT}=%{{http_code}}\\n" http://127.0.0.1:{WEB_PORT}/
curl -s http://127.0.0.1:{WEB_PORT}/api/health
echo ""
echo "ACCESS_URL=http://{HOST}:{WEB_PORT}"
echo "DONE"
"""


def main():
    if not PASSWORD:
        print("Set DEPLOY_PASSWORD", file=sys.stderr)
        sys.exit(1)

    script = REMOTE.replace("{HOST}", HOST).replace("{WEB_PORT}", WEB_PORT)

    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    _, stdout, _ = c.exec_command(script, timeout=120)
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
