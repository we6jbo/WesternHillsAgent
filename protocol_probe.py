#!/usr/bin/env python3
import json, socket, sys, uuid

host = "127.0.0.1"
port = int(sys.argv[1]) if len(sys.argv) > 1 else 32767
message_type = sys.argv[2] if len(sys.argv) > 2 else "status_request"
request = {
    "protocolVersion": 1,
    "messageId": str(uuid.uuid4()),
    "senderId": "WesternHillsAgentProtocolProbe",
    "senderType": "local_test_client",
    "messageType": message_type,
}
with socket.create_connection((host, port), timeout=3) as s:
    f = s.makefile("rwb", buffering=0)
    print("HELLO:", f.readline().decode().strip())
    f.write((json.dumps(request, separators=(",", ":")) + "\n").encode())
    print("RESPONSE:", f.readline().decode().strip())
