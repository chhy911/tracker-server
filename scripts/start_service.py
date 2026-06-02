import os
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

SCRIPT = r"""
set -e
systemctl stop tracker-server 2>/dev/null || true

# Ensure API port 8081 in config
grep '^port=' /opt/tracker-server/config/tracker.conf | head -3
sed -i 's/^port=8080/port=8081/' /opt/tracker-server/config/tracker.conf 2>/dev/null || true

systemctl start mysql || systemctl start mariadb || true
sleep 2

# Test run
timeout 3 /opt/tracker-server/build/tracker-server /opt/tracker-server/config/tracker.conf 2>&1 || true

systemctl restart tracker-server
sleep 2
systemctl status tracker-server --no-pager
journalctl -u tracker-server -n 20 --no-pager

curl -s http://127.0.0.1:8081/api/health || echo api_8081_fail
curl -s -o /dev/null -w "dash:%{http_code}\n" http://127.0.0.1:3000/
ss -tlnp | grep -E '6969|8081|3000' || true
"""

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
_, stdout, stderr = c.exec_command(SCRIPT, timeout=60)
print(stdout.read().decode())
print(stderr.read().decode())
c.close()
