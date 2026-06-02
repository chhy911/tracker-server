import os
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

cmds = [
    "ss -tlnp | grep tracker",
    "curl -s http://127.0.0.1:8081/api/health",
    'curl -s -o /dev/null -w "3000:%{http_code}" http://127.0.0.1:3000/',
    "grep -E '^(host|port|static)' /opt/tracker-server/config/tracker.conf",
    "tail -25 /opt/tracker-server/logs/tracker.log 2>/dev/null || journalctl -u tracker-server -n 20 --no-pager",
]

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)
for cmd in cmds:
    print(">>>", cmd)
    _, o, e = c.exec_command(cmd)
    print(o.read().decode())
    err = e.read().decode()
    if err:
        print(err)
c.close()
