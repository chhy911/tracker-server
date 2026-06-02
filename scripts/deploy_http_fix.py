#!/usr/bin/env python3
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

REMOTE = """
set -e
cd /opt/tracker-server/build
make -j$(nproc)
systemctl restart tracker-server
systemctl restart nginx
sleep 2
echo "=== Content-Type check ==="
curl -sI http://127.0.0.1:3000/ | grep -i content-type
curl -sI http://127.0.0.1:8888/ | grep -i content-type
curl -s http://127.0.0.1:8081/api/health
echo ""
curl -s http://127.0.0.1:8081/ | head -c 120
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
    sftp.put(os.path.join(REPO, "src", "api", "http_server.cpp"),
             "/opt/tracker-server/src/api/http_server.cpp")
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
