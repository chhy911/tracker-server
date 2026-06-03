import os
import sys
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

SCRIPT = r"""
set -e
REMOTE=/opt/tracker-server

mkdir -p $REMOTE/logs
chmod 755 $REMOTE/logs

# Ensure database schema
mysql -u root < $REMOTE/sql/init.sql 2>/dev/null || mysql < $REMOTE/sql/init.sql

# API on 8081 (8080 used by qBittorrent)
sed -i '/^\[api\]/,/^\[/ s/^port=.*/port=8081/' $REMOTE/config/tracker.conf

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
sleep 3
systemctl status tracker-server --no-pager || true
journalctl -u tracker-server -n 15 --no-pager || true

curl -sf http://127.0.0.1:8081/api/health && echo
curl -sf -o /dev/null -w "dashboard:%{http_code}\n" http://127.0.0.1:3000/
ss -tlnp | grep -E '6969|8081|3000' || true
echo FINISH_DONE
"""


def main():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)

    # upload db_manager fix
    sftp = c.open_sftp()
    sftp.put(
        os.path.join(os.path.dirname(__file__), "..", "src", "database", "db_manager.cpp"),
        "/opt/tracker-server/src/database/db_manager.cpp",
    )
    sftp.close()

    full = "cd /opt/tracker-server/build && make -j$(nproc) && " + SCRIPT
    _, stdout, stderr = c.exec_command(full, timeout=300)
    ch = stdout.channel
    while not ch.exit_status_ready():
        if ch.recv_ready():
            sys.stdout.buffer.write(ch.recv(4096))
            sys.stdout.buffer.flush()
        if ch.recv_stderr_ready():
            sys.stderr.buffer.write(ch.recv_stderr(4096))
            sys.stderr.buffer.flush()
        import time
        time.sleep(0.3)
    while ch.recv_ready():
        sys.stdout.buffer.write(ch.recv(4096))
    print("exit:", ch.recv_exit_status())
    c.close()


if __name__ == "__main__":
    main()
