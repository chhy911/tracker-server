#!/usr/bin/env python3
"""One-off remote deployment helper. Do not commit credentials."""
import os
import sys
import time
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
USER = os.environ.get("DEPLOY_USER", "root")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

DEPLOY_SCRIPT = """set -e
export DEBIAN_FRONTEND=noninteractive

echo "=== [1/5] Install prerequisites ==="
apt-get update -qq
apt-get install -y -qq git curl ca-certificates

if ! command -v docker >/dev/null 2>&1; then
  echo "=== [2/5] Install Docker ==="
  curl -fsSL https://get.docker.com | sh
  systemctl enable docker
  systemctl start docker
else
  echo "Docker already installed:"
  docker --version
fi

if ! docker compose version >/dev/null 2>&1; then
  apt-get install -y -qq docker-compose-plugin
fi

echo "=== [3/5] Clone or update repository ==="
mkdir -p /opt
if [ -d /opt/tracker-server/.git ]; then
  cd /opt/tracker-server && git fetch origin && git reset --hard origin/main
else
  rm -rf /opt/tracker-server
  git clone https://github.com/chhy911/tracker-server.git /opt/tracker-server
fi

echo "=== [4/5] Build and start containers ==="
cd /opt/tracker-server/docker
docker compose down 2>/dev/null || true
docker compose up -d --build

echo "=== [5/5] Status ==="
sleep 10
docker compose ps
docker compose logs --tail=40 tracker 2>/dev/null || true
echo "DEPLOY_DONE"
"""


def main():
    if not PASSWORD:
        print("Set DEPLOY_PASSWORD environment variable", file=sys.stderr)
        sys.exit(1)

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(
        HOST,
        username=USER,
        password=PASSWORD,
        timeout=30,
        allow_agent=False,
        look_for_keys=False,
    )

    print(f"Deploying to {USER}@{HOST} ...")
    stdin, stdout, stderr = client.exec_command(DEPLOY_SCRIPT, timeout=900)
    channel = stdout.channel

    while not channel.exit_status_ready():
        if channel.recv_ready():
            print(channel.recv(4096).decode("utf-8", errors="replace"), end="")
        if channel.recv_stderr_ready():
            print(
                channel.recv_stderr(4096).decode("utf-8", errors="replace"),
                end="",
                file=sys.stderr,
            )
        time.sleep(0.5)

    while channel.recv_ready():
        print(channel.recv(4096).decode("utf-8", errors="replace"), end="")
    while channel.recv_stderr_ready():
        print(
            channel.recv_stderr(4096).decode("utf-8", errors="replace"),
            end="",
            file=sys.stderr,
        )

    code = channel.recv_exit_status()
    print("Remote exit code:", code)
    client.close()
    sys.exit(code)


if __name__ == "__main__":
    main()
