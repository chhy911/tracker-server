#!/bin/bash
set -e
export DEBIAN_FRONTEND=noninteractive

apt-get update -qq
apt-get install -y -qq nginx

cat > /etc/nginx/sites-available/tracker <<'NGINX'
# 国内 ECS 80/443 常被封堵，请使用 8888 等端口（见 setup_nginx_alt_port.py）
server {
    listen 8888;
    listen [::]:8888;
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

echo "nginx OK on port 80"
