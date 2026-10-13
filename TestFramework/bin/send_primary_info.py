#sendpodid.py
#!/usr/bin/env python3
import sys

import socket
import select

CTR_TYPE="Primary"

cpuList = sys.argv[1]
acbf = sys.argv[2]
# Optional: targetIdleCores (must be < number of cores in cpuList) and LOW_TIC.
# Defaults match the original 8-core Primary setup.
tic = sys.argv[3] if len(sys.argv) > 3 else "7"
low_tic = sys.argv[4] if len(sys.argv) > 4 else "2"

msg = '{"ctr_type":"' + CTR_TYPE + '","cpuList":"' + cpuList + '","LOWIDLEFREQ_THRESHOLD":"' + acbf + '","LOW_TIC":"' + low_tic + '","targetIdleCores":"' + tic + '","static_targetIdleCores":"' + tic + '","minSecondaryCores":"0"}'

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setblocking(1)

host = "127.0.0.1"
port = 10101

print("Sending Primary id %s (len = %s)" % (msg, len(msg)))

try:
  sock.connect((host, port))
except BlockingIOError as e:
  print("BlockingIOError")
  
select.select([], [sock], [])
sock.sendall(bytes(msg, 'UTF-8'))
rtn = sock.recv(4)
print("Got reply: %s" % (rtn))
sock.close()
