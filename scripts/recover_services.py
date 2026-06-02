#!/usr/bin/env python3
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

RECOVER = r"""
set -e
echo "=== Service status ==="
systemctl is-active tracker-server 2>&1 || true
systemctl is-active nginx 2>&1 || true
systemctl is-active mysql 2>&1 || true

echo "=== Ports ==="
ss -tlnp | grep -E ':8888|:3000|:8081|:6969|:80 ' || echo "no ports"

echo "=== Memory / disk ==="
free -h | head -2
df -h / /opt | tail -2

echo "=== Recent errors ==="
journalctl -u tracker-server -n 25 --no-pager 2>/dev/null || true
tail -15 /opt/tracker-server/logs/tracker.log 2>/dev/null || true

echo "=== Restart services ==="
systemctl start mysql 2>/dev/null || true
sleep 2
systemctl restart tracker-server
systemctl restart nginx
sleep 3

echo "=== After restart ==="
systemctl is-active tracker-server
systemctl is-active nginx
ss -tlnp | grep -E ':8888|:6969' || true

echo "=== Health checks ==="
curl -s -m 5 -o /dev/null -w "8888:%{http_code}\n" http://127.0.0.1:8888/ || echo "8888 fail"
curl -s -m 5 http://127.0.0.1:8888/api/health || echo "api fail"
curl -s -m 5 -o /dev/null -w "6969:%{http_code}\n" "http://127.0.0.1:6969/announce?info_hash=%01%02%03%04%05%06%07%08%09%10%11%12%13%14%15%16%17%18%19%20&peer_id=%021%022%023%024%025%026%027%028%029%030%031%032%033%034%035%036&port=6881&uploaded=0&downloaded=0&left=1" || echo "6969 fail"

echo "=== Enable on boot ==="
systemctl enable tracker-server nginx mysql 2>/dev/null || true
echo RECOVER_DONE
"""


def main():
    if not PASSWORD:
        print("Set DEPLOY_PASSWORD", file=sys.stderr)
        sys.exit(1)
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    try:
        c.connect(HOST, username="root", password=PASSWORD, timeout=20, allow_agent=False, look_for_keys=False)
    except Exception as e:
        print("SSH failed:", e, file=sys.stderr)
        sys.exit(1)
    _, stdout, stderr = c.exec_command(RECOVER, timeout=90)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    sys.stdout.buffer.write(out.encode("utf-8", errors="replace"))
    if err:
        sys.stderr.buffer.write(err.encode("utf-8", errors="replace"))
    code = stdout.channel.recv_exit_status()
    c.close()
    sys.exit(code)


if __name__ == "__main__":
    main()
