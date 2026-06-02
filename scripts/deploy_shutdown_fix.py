#!/usr/bin/env python3
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

REMOTE = r"""
set -e
cp /opt/tracker-server/config/tracker-server.service /etc/systemd/system/tracker-server.service 2>/dev/null || true

cd /opt/tracker-server/build
make -j$(nproc)

cat > /etc/systemd/system/tracker-server.service <<'UNIT'
[Unit]
Description=BitTorrent Tracker Server
After=network.target mysql.service
Wants=mysql.service

[Service]
Type=simple
WorkingDirectory=/opt/tracker-server
ExecStart=/opt/tracker-server/build/tracker-server /opt/tracker-server/config/tracker.conf
Restart=on-failure
RestartSec=5
TimeoutStopSec=15
KillMode=mixed

[Install]
WantedBy=multi-user.target
UNIT

systemctl daemon-reload
systemctl enable tracker-server nginx mysql
systemctl restart tracker-server
systemctl restart nginx
sleep 3

systemctl is-active tracker-server nginx
curl -s -m 5 -o /dev/null -w "8888:%{http_code}\n" http://127.0.0.1:8888/
curl -s -m 5 http://127.0.0.1:8888/api/health
echo ""
echo DONE
"""


def main():
    if not PASSWORD:
        sys.exit("Set DEPLOY_PASSWORD")
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=20, allow_agent=False, look_for_keys=False)
    sftp = c.open_sftp()
    for f in ["src/tracker/tracker_server.cpp", "src/tracker/tracker_server.hpp", "src/main.cpp"]:
        sftp.put(os.path.join(REPO, f), f"/opt/tracker-server/{f}")
    sftp.close()
    _, stdout, _ = c.exec_command(REMOTE, timeout=180)
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
