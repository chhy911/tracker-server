#!/usr/bin/env python3
import os
import paramiko

HOST = os.environ.get("DEPLOY_HOST", "59.110.14.226")
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

DIAG = r"""
echo "=== service ==="
systemctl is-active tracker-server
ss -tlnp | grep 6969 || echo "6969 not listening"

echo "=== local announce test ==="
# minimal announce query (dummy 20-byte url-encoded hashes)
curl -sv "http://127.0.0.1:6969/announce?info_hash=%0123456789abcdef0123&peer_id=%0123456789abcdef0124&port=6881&uploaded=0&downloaded=0&left=100&compact=1&event=started" 2>&1 | tail -25

echo "=== external iface ==="
curl -s -m 5 -o /dev/null -w "ext6969:%{http_code}\n" "http://59.110.14.226:6969/announce?info_hash=%01%02%03%04%05%06%07%08%09%10%11%12%13%14%15%16%17%18%19%20&peer_id=%021%022%023%024%025%026%027%028%029%030%031%032%033%034%035%036&port=6881&uploaded=0&downloaded=0&left=1" || echo "ext curl failed"

echo "=== logs ==="
tail -20 /opt/tracker-server/logs/tracker.log 2>/dev/null

echo "=== iptables ufw ==="
ufw status 2>/dev/null | head -3
iptables -L INPUT -n 2>/dev/null | head -5
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
    print(e.read().decode())
    c.close()


if __name__ == "__main__":
    main()
