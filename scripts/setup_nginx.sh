#!/bin/bash
# Nginx 8888：静态 dashboard + /api 反代到 8081（与 setup_nginx_alt_port.py 一致）
set -e
export DEBIAN_FRONTEND=noninteractive

ROOT="${TRACKER_ROOT:-/opt/tracker-server}"
WEB_PORT="${WEB_PORT:-8888}"
API_PORT="${API_PORT:-8081}"

apt-get update -qq
apt-get install -y -qq nginx

cat > /etc/nginx/sites-available/tracker <<NGINX
server {
    listen ${WEB_PORT};
    listen [::]:${WEB_PORT};
    server_name _;

    root ${ROOT}/dashboard/dist;
    index index.html;

    location /api/ {
        proxy_pass http://127.0.0.1:${API_PORT};
        proxy_http_version 1.1;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_connect_timeout 5s;
        proxy_read_timeout 15s;
    }

    location /assets/ {
        try_files \$uri =404;
        expires 7d;
    }

    location / {
        try_files \$uri /index.html;
    }
}
NGINX

rm -f /etc/nginx/sites-enabled/default
ln -sf /etc/nginx/sites-available/tracker /etc/nginx/sites-enabled/tracker
nginx -t
systemctl enable nginx
systemctl restart nginx
echo "nginx OK on port ${WEB_PORT}"
