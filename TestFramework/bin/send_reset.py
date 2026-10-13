#send_reset.py
#!/usr/bin/env python3
# Ask the Listener to pause the Balancer and forget all registered
# Primary/Secondary containers (e.g. between runs without reloading the module).
import socket

msg = '{"ctr_type":"Reset"}'

sock = socket.create_connection(("127.0.0.1", 10101))
sock.sendall(bytes(msg, 'UTF-8'))
sock.shutdown(socket.SHUT_WR)
rtn = sock.recv(4)
print("Got reply: %s" % (rtn))
sock.close()
