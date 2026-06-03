import os
import sys
import tarfile
import tempfile
import time
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

BUILD_SCRIPT = """set -e
cd /opt/tracker-server/build
rm -rf CMakeCache.txt CMakeFiles
cmake ..
make -j$(nproc)
systemctl restart tracker-server
sleep 3
systemctl status tracker-server --no-pager || true
curl -sf http://127.0.0.1:8081/api/health && echo
curl -sf -o /dev/null -w "dashboard:%{http_code}\\n" http://127.0.0.1:3000/
ss -tlnp | grep -E '6969|8081|3000' || true
"""


def main():
    tmp = tempfile.NamedTemporaryFile(suffix=".tar.gz", delete=False)
    tmp.close()
    with tarfile.open(tmp.name, "w:gz") as tar:
        for root, dirs, files in os.walk(os.path.join(REPO, "src")):
            for f in files:
                full = os.path.join(root, f)
                rel = os.path.relpath(full, REPO).replace("\\", "/")
                tar.add(full, arcname=rel)
        tar.add(os.path.join(REPO, "CMakeLists.txt"), arcname="CMakeLists.txt")

    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    sftp = client.open_sftp()
    sftp.put(tmp.name, "/tmp/src-update.tar.gz")
    sftp.close()
    os.unlink(tmp.name)

    _, stdout, stderr = client.exec_command(
        "tar -xzf /tmp/src-update.tar.gz -C /opt/tracker-server && " + BUILD_SCRIPT,
        timeout=600,
    )
    ch = stdout.channel
    while not ch.exit_status_ready():
        if ch.recv_ready():
            sys.stdout.buffer.write(ch.recv(4096))
            sys.stdout.buffer.flush()
        if ch.recv_stderr_ready():
            sys.stderr.buffer.write(ch.recv_stderr(4096))
            sys.stderr.buffer.flush()
        time.sleep(0.3)
    while ch.recv_ready():
        sys.stdout.buffer.write(ch.recv(4096))
    code = ch.recv_exit_status()
    print("exit:", code)
    client.close()
    sys.exit(code)


if __name__ == "__main__":
    main()
