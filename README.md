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
