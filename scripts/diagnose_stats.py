#!/usr/bin/env python3
import os
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

DIAG = r"""
echo "=== API stats ==="
curl -s http://127.0.0.1:8081/api/stats
echo ""
curl -s http://127.0.0.1:8081/api/metrics
echo ""

echo "=== MySQL counts ==="
mysql -u tracker -ptracker_password tracker_db -e "
SELECT COUNT(*) AS torrents FROM torrents;
SELECT COUNT(*) AS peers FROM peers;
SELECT * FROM torrents LIMIT 5;
SELECT peer_id, info_hash, ip, port, event, last_seen FROM peers ORDER BY last_seen DESC LIMIT 10;
SELECT * FROM statistics;
"

echo "=== tracker log errors (last 50) ==="
grep -i error /opt/tracker-server/logs/tracker.log 2>/dev/null | tail -20

echo "=== recent log ==="
tail -15 /opt/tracker-server/logs/tracker.log

echo "=== announce count today ==="
grep -c announce /opt/tracker-server/logs/tracker.log 2>/dev/null || echo 0
"""


def main():
    if not PASSWORD:
        print("Set DEPLOY_PASSWORD")
        return
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
    _, o, e = c.exec_command(DIAG, timeout=60)
    print(o.read().decode())
    if e.read():
        print(e.read().decode())
    c.close()


if __name__ == "__main__":
    main()
