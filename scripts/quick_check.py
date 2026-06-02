#!/usr/bin/env python3
import os
import paramiko

HOST = "59.110.14.226"
PASSWORD = os.environ.get("DEPLOY_PASSWORD", "")

CMD = """
systemctl is-active tracker-server nginx mysql 2>&1
ss -tlnp | grep -E '8888|6969' | head -5
journalctl -u tracker-server --since '1 hour ago' -n 8 --no-pager 2>/dev/null
"""

c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(HOST, username="root", password=PASSWORD, timeout=15, allow_agent=False, look_for_keys=False)
_, o, _ = c.exec_command(CMD, timeout=20)
print(o.read().decode())
c.close()
