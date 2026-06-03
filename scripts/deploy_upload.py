#!/usr/bin/env python3
"""Upload sources and deploy tracker-server natively on remote Ubuntu."""
import os
import sys
import tarfile
import tempfile
import time
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
USER = os.environ.get("DEPLOY_USER", "root")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
REMOTE_DIR = "/opt/tracker-server"

SKIP_DIRS = {
    "build",
    "node_modules",
    ".git",
    "logs",
    "cmake-build-debug",
    "cmake-build-release",
    "scripts/archive",
}
SKIP_PREFIXES = ("dashboard/node_modules", "dashboard/dist")


def should_skip(path: str) -> bool:
    parts = path.replace("\\", "/").split("/")
    for part in parts:
        if part in SKIP_DIRS:
            return True
    rel = path.replace("\\", "/")
    for prefix in SKIP_PREFIXES:
        if rel == prefix or rel.startswith(prefix + "/"):
            return True
    return False


def make_archive() -> str:
    tmp = tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False)
    tmp.close()
    count = 0
    print("Creating archive...")
    with tarfile.open(tmp.name, "w:gz") as tar:
        for root, dirs, files in os.walk(REPO_ROOT):
            dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
            for name in files:
                full = os.path.join(root, name)
                rel = os.path.relpath(full, REPO_ROOT).replace("\\", "/")
                if should_skip(rel):
                    continue
                tar.add(full, arcname=rel)
                count += 1
    size_kb = os.path.getsize(tmp.name) / 1024
    print(f"Archive: {count} files, {size_kb:.0f} KB")
    return tmp.name


REMOTE_SCRIPT = f"""set -e
export DEBIAN_FRONTEND=noninteractive

echo "=== Install packages ==="
apt-get update -qq
apt-get install -y -qq \
  build-essential cmake git curl ca-certificates nginx \
  libboost-all-dev libmysqlclient-dev \
  mysql-server mysql-client \
  nodejs npm

systemctl enable mysql
systemctl start mysql || true

echo "=== Extract project ==="
mkdir -p {REMOTE_DIR}
rm -rf {REMOTE_DIR}/*
tar -xzf /tmp/tracker-server.tar.gz -C {REMOTE_DIR}

echo "=== Initialize database ==="
mysql -u root < {REMOTE_DIR}/sql/init.sql 2>/dev/null || mysql < {REMOTE_DIR}/sql/init.sql

echo "=== Swap ==="
bash {REMOTE_DIR}/scripts/setup_swap.sh || true

echo "=== Build dashboard ==="
cd {REMOTE_DIR}/dashboard
npm install --silent
unset VITE_API_URL
npm run build

echo "=== Build tracker-server ==="
cd {REMOTE_DIR}
mkdir -p build logs
cd build
cmake ..
make -j$(nproc)

echo "=== Install systemd service ==="
cp -f {REMOTE_DIR}/config/tracker-server.service /etc/systemd/system/tracker-server.service
systemctl daemon-reload
systemctl enable tracker-server

echo "=== Nginx ==="
bash {REMOTE_DIR}/scripts/setup_nginx.sh

cat > /etc/cron.d/tracker-health <<'CRON'
*/2 * * * * root curl -sf -m 5 http://127.0.0.1:8888/api/health >/dev/null || (systemctl restart tracker-server; systemctl restart nginx)
CRON
chmod 644 /etc/cron.d/tracker-health

systemctl restart tracker-server
sleep 2
systemctl restart nginx
sleep 1

systemctl status tracker-server --no-pager || true
curl -sf http://127.0.0.1:8081/api/health && echo " API OK" || echo " API check failed"
curl -sf -o /dev/null -w "Web 8888 %{{http_code}}\\n" http://127.0.0.1:8888/ || true
bash {REMOTE_DIR}/scripts/smoke_test.sh || true

echo "DEPLOY_DONE"
"""


def _safe_print(text: str, stream=None) -> None:
    stream = stream or sys.stdout
    enc = getattr(stream, "encoding", None) or "utf-8"
    stream.buffer.write(text.encode(enc, errors="replace"))
    stream.buffer.flush()


def main():
    if not PASSWORD:
        print("Set DEPLOY_PASSWORD", file=sys.stderr)
        sys.exit(1)

    archive = make_archive()
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(
        HOST, username=USER, password=PASSWORD, timeout=30,
        allow_agent=False, look_for_keys=False,
    )

    print(f"Uploading to {USER}@{HOST}...")
    sftp = client.open_sftp()
    sftp.put(archive, "/tmp/tracker-server.tar.gz")
    sftp.close()
    os.unlink(archive)

    print("Installing on server (build may take several minutes)...")
    stdin, stdout, stderr = client.exec_command(REMOTE_SCRIPT, timeout=1800)
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
    print("Remote exit code:", code)
    client.close()
    sys.exit(code)


if __name__ == "__main__":
    main()
