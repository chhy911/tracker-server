#!/usr/bin/env python3
"""增量生产部署：编译、Nginx 静态、Swap、systemd、健康检查。"""
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
USER = os.environ.get("DEPLOY_USER", "root")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REMOTE = "/opt/tracker-server"

REMOTE_SCRIPT = f"""set -e
export DEBIAN_FRONTEND=noninteractive
cd {REMOTE}

echo "=== Swap ==="
bash scripts/setup_swap.sh || true

echo "=== Build dashboard ==="
cd dashboard
npm install --silent
unset VITE_API_URL
npm run build
cd ..

echo "=== Build tracker-server ==="
mkdir -p build logs
cd build
cmake ..
make -j$(nproc)
cd ..

echo "=== systemd ==="
cp -f config/tracker-server.service /etc/systemd/system/tracker-server.service
systemctl daemon-reload

echo "=== Nginx 8888 static ==="
bash scripts/setup_nginx.sh

echo "=== Cron health watchdog ==="
cat > /etc/cron.d/tracker-health <<'CRON'
*/2 * * * * root curl -sf -m 5 http://127.0.0.1:8888/api/health >/dev/null || (systemctl restart tracker-server; systemctl restart nginx)
CRON
chmod 644 /etc/cron.d/tracker-health

systemctl restart tracker-server
sleep 2
systemctl restart nginx
sleep 1

echo "=== Verify ==="
ss -tlnp | grep -E '6969|8081|8888' || true
for i in 1 2 3; do
  curl -s -m 5 -o /dev/null -w "8888_$i:%{{http_code}} %{{time_total}}s\\n" http://127.0.0.1:8888/ || echo fail
done
curl -s -m 5 http://127.0.0.1:8888/api/health
bash scripts/smoke_test.sh
echo APPLY_PRODUCTION_DONE
"""


def main():
    if not PASSWORD:
        sys.exit("Set DEPLOY_PASSWORD")
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PASSWORD, timeout=30,
              allow_agent=False, look_for_keys=False)
    stdin, stdout, stderr = c.exec_command(REMOTE_SCRIPT, timeout=1200)
    def safe_print(text: str, stream=None) -> None:
        stream = stream or sys.stdout
        enc = getattr(stream, "encoding", None) or "utf-8"
        stream.buffer.write(text.encode(enc, errors="replace"))
        stream.buffer.flush()

    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    code = stdout.channel.recv_exit_status()
    safe_print(out)
    if err:
        safe_print(err, sys.stderr)
    c.close()
    sys.exit(code)


if __name__ == "__main__":
    main()
