import os
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

client = paramiko.SSHClient()
client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
client.connect(HOST, username="root", password=PASSWORD, timeout=30, allow_agent=False, look_for_keys=False)

cmds = [
    "test -f /opt/tracker-server/build/tracker-server && echo BIN_OK || echo BIN_MISSING",
    "test -d /opt/tracker-server/dashboard/dist && echo DASH_OK || echo DASH_MISSING",
    "systemctl is-active tracker-server 2>/dev/null || echo service_inactive",
    "curl -s http://127.0.0.1:8080/api/health 2>/dev/null || echo api_down",
    "ss -tlnp | grep -E '6969|8080|3000' || true",
]

for cmd in cmds:
    print(">>>", cmd)
    _, stdout, stderr = client.exec_command(cmd, timeout=30)
    print(stdout.read().decode())
    err = stderr.read().decode()
    if err:
        print(err)

client.close()
