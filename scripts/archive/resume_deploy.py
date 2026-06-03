import os
import sys
import time
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

RESUME_SCRIPT = f"""set -e
export DEBIAN_FRONTEND=noninteractive
REMOTE={"/opt/tracker-server"}

echo "=== Fix port conflict: API 8081 (8080 used by qBittorrent) ==="
sed -i 's/^port=8080/port=8081/' $REMOTE/config/tracker.conf
grep -q '^port=8081' $REMOTE/config/tracker.conf || echo -e '\\n[api]\\nport=8081\\nhost=0.0.0.0' >> $REMOTE/config/tracker.conf

echo "=== Rebuild dashboard with API URL ==="
cd $REMOTE/dashboard
export VITE_API_URL=http://{HOST}:8081
npm run build --silent

echo "=== Install Boost dev components ==="
apt-get install -y -qq libboost-all-dev

echo "=== Build C++ tracker ==="
cd $REMOTE
mkdir -p build logs
cd build
rm -rf CMakeCache.txt CMakeFiles
cmake ..
make -j$(nproc)

echo "=== systemd service ==="
cat > /etc/systemd/system/tracker-server.service <<UNIT
[Unit]
Description=BitTorrent Tracker Server
After=network.target mysql.service
Wants=mysql.service

[Service]
Type=simple
WorkingDirectory=$REMOTE
ExecStart=$REMOTE/build/tracker-server $REMOTE/config/tracker.conf
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
UNIT

systemctl daemon-reload
systemctl enable tracker-server
systemctl restart tracker-server
sleep 4
systemctl status tracker-server --no-pager || true

echo "=== Health checks ==="
curl -sf http://127.0.0.1:8081/api/health || echo api_fail
curl -sf -o /dev/null -w "dashboard:%{{http_code}}\\n" http://127.0.0.1:3000/ || true
ss -tlnp | grep -E '6969|8081|3000' || true
echo RESUME_DONE
"""


def _safe_print(text, stream=None):
    stream = stream or sys.stdout
    enc = getattr(stream, "encoding", None) or "utf-8"
    stream.buffer.write(text.encode(enc, errors="replace"))
    stream.buffer.flush()


def main():
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    print("Resuming build on server...")
    _, stdout, stderr = client.exec_command(RESUME_SCRIPT, timeout=1200)
    ch = stdout.channel
    while not ch.exit_status_ready():
        if ch.recv_ready():
            _safe_print(ch.recv(4096).decode("utf-8", errors="replace"))
        if ch.recv_stderr_ready():
            _safe_print(ch.recv_stderr(4096).decode("utf-8", errors="replace"), sys.stderr)
        time.sleep(0.5)
    while ch.recv_ready():
        _safe_print(ch.recv(4096).decode("utf-8", errors="replace"))
    while ch.recv_stderr_ready():
        _safe_print(ch.recv_stderr(4096).decode("utf-8", errors="replace"), sys.stderr)
    code = ch.recv_exit_status()
    print("exit:", code)
    client.close()
    sys.exit(code)


if __name__ == "__main__":
    main()
