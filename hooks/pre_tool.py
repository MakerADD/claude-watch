"""
PreToolUse hook - fires before every Claude tool call.
Checks the watch server for button presses.
  - Long press (pause) -> blocks the tool and notifies Claude
  - Short press (continue) -> clears it and proceeds
"""
import sys
import json
import urllib.request
import urllib.error

SERVER = "http://localhost:5000"


def get_status():
    try:
        with urllib.request.urlopen(f"{SERVER}/status", timeout=1) as r:
            return json.loads(r.read())
    except Exception:
        return None


def clear_reply():
    try:
        req = urllib.request.Request(
            f"{SERVER}/reply/clear",
            data=b"{}",
            headers={"Content-Type": "application/json"},
            method="POST"
        )
        urllib.request.urlopen(req, timeout=1)
    except Exception:
        pass


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
    # Read tool info from stdin
    try:
        hook_data = json.loads(sys.stdin.read())
    except Exception:
        hook_data = {}

    tool_name = hook_data.get("tool_name", "unknown")

    # Check for a pending button reply
    status = get_status()
    if status:
        pending = status.get("pending_reply")

        if pending == "pause":
            clear_reply()
            notify("/hook/thinking", {"message": "Paused by watch button"})
            # Exit code 2 blocks the tool and shows this message to Claude
            print("User pressed PAUSE on watch. Stop and wait for them to return.")
            sys.exit(2)

        elif pending == "continue":
            clear_reply()
            # Just proceed, clear the flag silently

    # Update status to show which tool is starting
    notify("/hook/tool_start", {"tool": tool_name})


if __name__ == "__main__":
    main()
