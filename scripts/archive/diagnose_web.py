#!/usr/bin/env python3
import os
import sys
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
USER = os.environ.get("DEPLOY_USER", "root")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

DIAG = r"""
echo "=== systemd ==="
systemctl is-active tracker-server 2>&1
systemctl status tracker-server --no-pager 2>&1 | head -20

echo "=== listen ports ==="
ss -tlnp | grep -E '6969|8080|8081|3000' || echo "no tracker ports"

echo "=== local curl ==="
curl -s -m 3 -o /dev/null -w "8081:%{http_code}\n" http://127.0.0.1:8081/api/health || echo "8081 fail"
curl -s -m 3 -o /dev/null -w "3000:%{http_code}\n" http://127.0.0.1:3000/ || echo "3000 fail"
curl -s -m 3 http://127.0.0.1:3000/ | head -3

echo "=== dashboard files ==="
ls -la /opt/tracker-server/dashboard/dist/ 2>&1 | head -8

echo "=== config ==="
grep -E '^\[|^(host|port|static)' /opt/tracker-server/config/tracker.conf

echo "=== firewall ==="
ufw status 2>/dev/null || echo "ufw not active"
iptables -L INPUT -n 2>/dev/null | head -15

echo "=== recent logs ==="
tail -15 /opt/tracker-server/logs/tracker.log 2>/dev/null
journalctl -u tracker-server -n 10 --no-pager 2>/dev/null
"""


def main():
    if not PASSWORD:
        print("Set DEPLOY_PASSWORD", file=sys.stderr)
        sys.exit(1)

    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    _, stdout, stderr = c.exec_command(DIAG, timeout=60)
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    sys.stdout.buffer.write(out.encode("utf-8", errors="replace"))
    if err:
        sys.stderr.buffer.write(err.encode("utf-8", errors="replace"))
    c.close()


if __name__ == "__main__":
    main()
