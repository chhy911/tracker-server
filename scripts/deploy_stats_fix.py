#!/usr/bin/env python3
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

FILES = [
    "src/database/db_manager.cpp",
    "src/database/db_manager.hpp",
    "src/tracker/bep_handler.cpp",
    "src/api/rest_api.cpp",
    "src/api/rest_api.hpp",
    "src/api/http_server.cpp",
    "dashboard/src/App.jsx",
    "dashboard/src/App.css",
]

REMOTE = r"""
set -e
cd /opt/tracker-server/dashboard
export VITE_API_URL=
npm run build --silent

cd /opt/tracker-server/build
make -j$(nproc)
systemctl restart tracker-server
systemctl restart nginx
sleep 2

mysql -u tracker -ptracker_password tracker_db -e "
UPDATE torrents t SET
  complete = (SELECT COUNT(*) FROM peers p WHERE p.info_hash=t.info_hash AND p.left_to_download=0 AND p.last_seen > DATE_SUB(NOW(), INTERVAL 2 HOUR)),
  incomplete = (SELECT COUNT(*) FROM peers p WHERE p.info_hash=t.info_hash AND p.left_to_download>0 AND p.last_seen > DATE_SUB(NOW(), INTERVAL 2 HOUR));
"

echo "=== API ==="
curl -s http://127.0.0.1:8888/api/stats
echo ""
curl -s http://127.0.0.1:8888/api/torrents | head -c 300
echo ""
echo DONE
"""


def main():
    if not PASSWORD:
        sys.exit("Set DEPLOY_PASSWORD")
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    sftp = c.open_sftp()
    for f in FILES:
        sftp.put(os.path.join(REPO, f), f"/opt/tracker-server/{f}")
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
