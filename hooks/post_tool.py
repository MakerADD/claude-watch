"""
PostToolUse hook - fires after every Claude tool call.
Updates the watch with what just completed.
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

    tool_name = hook_data.get("tool_name", "unknown")
    notify("/hook/tool_done", {"tool": tool_name})


if __name__ == "__main__":
    main()
