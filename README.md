# WesternHillsAgent v0.3

Version 3 preserves the persistent JeremiahPortGuard-managed TCP assignments introduced in v2 and adds a deliberately small local protocol.

## Local endpoints

WesternHillsAgent binds its two JeremiahPortGuard assignments to `127.0.0.1`. On the user's current T14 registry these have been observed as primary 32767 and secondary 32766, but the program continues to read the persistent assignments from JeremiahPortGuard instead of hard-coding those numbers.

Both ports use newline-delimited JSON (NDJSON). Each request is one compact JSON object terminated by `\n`.

Supported informational message types in v3:

- `ping`
- `identity`
- `status_request`
- `port_status`
- `capabilities`
- `network_specifications`

These requests do not authorize computer control. v3 does not execute arbitrary commands received over TCP and does not expose the continuation launcher over the unauthenticated protocol.

Example request:

```json
{"protocolVersion":1,"messageId":"test-1","senderId":"JeremiahPortGuard","senderType":"local_service","messageType":"ping"}
```

## Continuation URL

WesternHillsAgent uses `/opt/web213/continuation.json`.

If the file does not exist and the current user can write the directory, it creates:

```json
{
  "schemaVersion": 1,
  "url": "https://www.google.com/",
  "browser": "google-chrome"
}
```

The application never invokes sudo/root to create this file. The GUI button validates that the URL uses `http` or `https`, then attempts `/opt/google/chrome/chrome`, `google-chrome-stable`, `google-chrome`, and finally the desktop URL handler. No shell is used.

The existing `/opt/web213/website.json` is left untouched.

## Future connectors

`MoltbookConnector` is a placeholder until a documented/verified Moltbook interface is selected. `LocalAgentConnector` is a placeholder for the other Linux local AI whose network specifications are planned for September 20, 2026 before 2 PM.

## Safety boundary

Version 3 is useful for discovery/status communication but is not a remote-control API. Authentication/signatures/trust-list authorization remain required before any future control messages are enabled.

## Quick protocol test

With WesternHillsAgent running, use the actual primary port shown in the GUI. For the current persistent assignment this is expected to be 32767 unless the registry was changed:

```bash
python3 protocol_probe.py 32767 status_request
python3 protocol_probe.py 32767 capabilities
python3 protocol_probe.py 32767 network_specifications
```
