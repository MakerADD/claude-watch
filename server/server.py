from flask import Flask, request, jsonify
from datetime import datetime

app = Flask(__name__)

# Current status state
state = {
    "level": "idle",        # idle | working | done | error
    "message": "Waiting...",
    "tool": "",
    "updated": datetime.now().isoformat(),
    "pending_reply": None   # set when button press needs Claude's attention
}

# Recent history (last 20 entries)
history = []


def update_state(level, message, tool=""):
    state["level"] = level
    state["message"] = message
    state["tool"] = tool
    state["updated"] = datetime.now().isoformat()
    history.append({
        "level": level,
        "message": message,
        "tool": tool,
        "time": state["updated"]
    })
    if len(history) > 20:
        history.pop(0)
    print(f"[{datetime.now().strftime('%H:%M:%S')}] {level.upper():8} {message}")


# --- ESP32 endpoints ---

@app.route("/status", methods=["GET"])
def get_status():
    """ESP32 polls this every 2 seconds."""
    return jsonify(state)


@app.route("/button", methods=["POST"])
def button_press():
    """ESP32 calls this when the BOOT button is pressed."""
    data = request.get_json(silent=True) or {}
    press_type = data.get("type", "short")  # short | long

    if press_type == "short":
        state["pending_reply"] = "continue"
        msg = "Got it - continuing"
    else:
        state["pending_reply"] = "pause"
        msg = "Pause requested"

    print(f"[{datetime.now().strftime('%H:%M:%S')}] BUTTON   {press_type} press -> {msg}")
    return jsonify({"ok": True, "message": msg})


# --- Claude Code hook endpoints ---

@app.route("/hook/tool_start", methods=["POST"])
def tool_start():
    """Fires when Claude starts using a tool."""
    data = request.get_json(silent=True) or {}
    tool = data.get("tool", "unknown")
    update_state("working", f"Running: {tool}", tool)
    return jsonify({"ok": True})


@app.route("/hook/tool_done", methods=["POST"])
def tool_done():
    """Fires when Claude finishes a tool."""
    data = request.get_json(silent=True) or {}
    tool = data.get("tool", "unknown")
    update_state("working", f"Done: {tool}", tool)
    return jsonify({"ok": True})


@app.route("/hook/thinking", methods=["POST"])
def thinking():
    """Fires when Claude is thinking between tools."""
    data = request.get_json(silent=True) or {}
    message = data.get("message", "Thinking...")
    update_state("working", message)
    return jsonify({"ok": True})


@app.route("/hook/idle", methods=["POST"])
def idle():
    """Fires when Claude finishes and is waiting for input."""
    data = request.get_json(silent=True) or {}
    message = data.get("message", "Done - waiting for you")
    update_state("done", message)
    return jsonify({"ok": True})


@app.route("/hook/error", methods=["POST"])
def error():
    """Fires when Claude hits an error."""
    data = request.get_json(silent=True) or {}
    message = data.get("message", "Something needs attention")
    update_state("error", message)
    return jsonify({"ok": True})


# --- Debug endpoints ---

@app.route("/history", methods=["GET"])
def get_history():
    return jsonify(history)


@app.route("/", methods=["GET"])
def index():
    """Simple status page you can open in a browser."""
    level_colors = {
        "idle": "#888",
        "working": "#f5a623",
        "done": "#7ed321",
        "error": "#d0021b"
    }
    color = level_colors.get(state["level"], "#888")
    html = f"""
    <html>
    <head>
        <title>Claude Watch</title>
        <meta http-equiv="refresh" content="2">
        <style>
            body {{ font-family: monospace; background: #1a1a1a; color: #eee; padding: 40px; }}
            .status {{ font-size: 2em; color: {color}; margin: 20px 0; }}
            .level {{ font-size: 1em; color: {color}; text-transform: uppercase; }}
            .time {{ font-size: 0.8em; color: #555; }}
            .history {{ margin-top: 40px; font-size: 0.85em; }}
            .entry {{ padding: 4px 0; border-bottom: 1px solid #222; }}
        </style>
    </head>
    <body>
        <h2>Claude Watch Status</h2>
        <div class="level">{state["level"]}</div>
        <div class="status">{state["message"]}</div>
        <div class="time">Updated: {state["updated"]}</div>
        <div class="history">
            <h3>Recent</h3>
            {"".join(f'<div class="entry">[{e["time"][11:19]}] {e["level"].upper()}: {e["message"]}</div>' for e in reversed(history))}
        </div>
    </body>
    </html>
    """
    return html


if __name__ == "__main__":
    print("Claude Watch server starting on http://localhost:5000")
    print("Open http://localhost:5000 in a browser to monitor status")
    print("ESP32 should poll http://<your-pc-ip>:5000/status")
    app.run(host="0.0.0.0", port=5000, debug=False)
