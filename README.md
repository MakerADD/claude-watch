# Claude Watch

A wearable ESP32-C6 status display that shows Claude Code activity on a Waveshare ESP32-C6-LCD-1.47 while you walk away from your desk.

## Hardware
- Waveshare ESP32-C6-LCD-1.47 (172x320 display, RGB LED, WiFi 6)
- Worn as a watch / carried in pocket

## How it works
- Claude Code hooks fire on tool start/stop, writing status to a local Python server
- The ESP32 polls the server over WiFi every 2 seconds and displays the current status
- BOOT button (GPIO 9): short press = "got it, keep going" / long press = "pause"
- RGB LED: green = idle/done, yellow = working, red = needs attention

## Project Structure
```
claude-watch/
├── esp32/          # Arduino sketch for the watch firmware
├── server/         # Python status server (runs on your PC)
├── hooks/          # Claude Code hook scripts
└── setup.ps1       # One-command setup for a new Windows machine
```

## Setup on a new machine
1. Install Arduino CLI: `winget install ArduinoSA.ArduinoCLI`
2. Clone this repo: `git clone https://github.com/MakerADD/claude-watch`
3. Run setup: `./setup.ps1`
4. Flash the watch: see `esp32/README.md`
5. Start the server: `cd server && python server.py`

---

## Current Status (as of 2026-04-15)

### What's working
- Display tested and confirmed working (172x320 ST7789, col_offset=34)
- RGB LED tested and confirmed working (GPIO 8, NeoPixel)
- Python status server fully built and tested
- Claude Code hooks wired up (PreToolUse, PostToolUse, Stop)
- Firmware compiled and uploads successfully
- Button short/long press logic implemented

### Known issue — WiFi not connecting at home
The ESP32 is not finding the home network (NETGEAR13-2, 5GHz band).
Possible causes:
- ESP32-C6 WiFi 6 / 5GHz compatibility issue with this router
- Router may need to allow new device MAC: `b0:a6:04:8b:27:f4`

**Plan for tomorrow at work:**
1. Try connecting to work WiFi — if it works, the home router is the issue
2. If work WiFi connects, get work PC IP (`ipconfig`) and fill in `config.h`:
   - `WIFI_SSID_2` = work network name
   - `WIFI_PASS_2` = work network password
   - `SERVER_HOST_2` = work PC IP address
3. Start `server.py` on work PC (`pip install flask` then `python server.py`)
4. The Claude Code hooks are already configured in `~/.claude/settings.json` —
   copy that hooks config to the work PC's settings too

### config.h (never committed — create manually on each machine)
Copy `esp32/claude_watch/config.h.example` to `config.h` and fill in credentials.
Home PC IP: `192.168.1.3`
