"""
Stop hook - fires when Claude finishes a response and is waiting for input.
Updates the watch to show Claude is done and waiting.
"""
import sys
import json
import urllib.request

SERVER = "http://localhost:5000"


def notify(endpoint, payload):
    try:
        data = json.dumps(payload).encode()
        req = urllib.request.Request(
            f"{SERVER}{endpoint}",
            data=data,
            headers={"Content-Type": "application/json"},
            method="POST"
        )
        urllib.request.urlopen(req, timeout=1)
    except Exception:
        pass


def main():
    try:
        hook_data = json.loads(sys.stdin.read())
    except Exception:
        hook_data = {}

    # Check if Claude is waiting for input vs just stopping
    stop_reason = hook_data.get("stop_hook_active", False)

    if stop_reason:
        notify("/hook/idle", {"message": "Done - needs your input"})
    else:
        notify("/hook/idle", {"message": "Done - waiting for you"})


if __name__ == "__main__":
    main()
