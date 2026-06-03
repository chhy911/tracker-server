#!/usr/bin/env python3
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

FILES = [
    ("src/tracker/bep_handler.cpp", "src/tracker/bep_handler.cpp"),
    ("src/tracker/bep_handler.hpp", "src/tracker/bep_handler.hpp"),
    ("src/tracker/tracker_server.cpp", "src/tracker/tracker_server.cpp"),
    ("src/database/db_manager.cpp", "src/database/db_manager.cpp"),
    ("src/database/db_manager.hpp", "src/database/db_manager.hpp"),
    ("src/api/rest_api.cpp", "src/api/rest_api.cpp"),
]

REMOTE = r"""
set -e
cd /opt/tracker-server/build
make -j$(nproc)
systemctl restart tracker-server
sleep 2
echo "=== announce test ==="
curl -s -o /tmp/ann.out -w "code:%{http_code}\n" \
  "http://127.0.0.1:6969/announce?info_hash=%0123456789abcdef0123&peer_id=%0123456789abcdef0124&port=6881&uploaded=0&downloaded=0&left=100&compact=1&event=started"
head -c 80 /tmp/ann.out | xxd | head -5
tail -5 /opt/tracker-server/logs/tracker.log
"""


def main():
    if not PASSWORD:
        sys.exit("Set DEPLOY_PASSWORD")
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    sftp = c.open_sftp()
    for local, remote in FILES:
        sftp.put(os.path.join(REPO, local), f"/opt/tracker-server/{remote}")
    sftp.close()
    _, stdout, _ = c.exec_command(REMOTE, timeout=300)
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
