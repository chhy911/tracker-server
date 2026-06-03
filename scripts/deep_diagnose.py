#!/usr/bin/env python3
import os
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

CMD = r"""
echo "=== uptime / load ==="
uptime
echo "=== services ==="
systemctl is-active tracker-server nginx mysql 2>&1
systemctl show tracker-server -p ActiveState,SubState,NRestarts,Result,MainPID --value 2>/dev/null | paste - - - - -
echo "=== ports ==="
ss -tlnp | grep -E '8888|3000|8081|6969' || true
echo "=== OOM / kills last 7d ==="
journalctl -k --since "7 days ago" 2>/dev/null | grep -iE 'oom|killed|out of memory' | tail -10 || echo none
echo "=== tracker failures 24h ==="
journalctl -u tracker-server --since "24 hours ago" -p err --no-pager 2>/dev/null | tail -15
journalctl -u tracker-server --since "24 hours ago" | grep -iE 'timeout|kill|failed|error' | tail -15
echo "=== nginx errors 24h ==="
journalctl -u nginx --since "24 hours ago" -p err --no-pager 2>/dev/null | tail -10
echo "=== memory ==="
free -h
echo "=== curl local ==="
for i in 1 2 3; do
  curl -s -m 3 -o /dev/null -w "try$i:%{http_code} time:%{time_total}s\n" http://127.0.0.1:8888/ || echo "try$i:fail"
done
curl -s -m 3 -o /dev/null -w "3000:%{http_code}\n" http://127.0.0.1:3000/ || echo 3000:fail
"""

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username="root", password=PASSWORD, timeout=20, allow_agent=False, look_for_keys=False)
_, o, e = c.exec_command(CMD, timeout=60)
print(o.read().decode())
if e.read():
    print(e.read().decode())
c.close()
