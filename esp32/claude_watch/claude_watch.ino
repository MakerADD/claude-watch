#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// --- Display pins ---
#define LCD_BL   22
#define LCD_DC   15
#define LCD_CS   14
#define LCD_RST  21
#define LCD_SCK   7
#define LCD_MOSI  6

// --- RGB LED ---
#define LED_PIN   8
#define NUM_LEDS  1

// --- Button ---
#define BOOT_BTN        9
#define LONG_PRESS_MS   1000

// --- Colors (16-bit) ---
#define COL_BLACK    0x0000
#define COL_WHITE    0xFFFF
#define COL_ORANGE   0xFC60   // working
#define COL_GREEN    0x3666   // done
#define COL_RED      0xD000   // error
#define COL_GRAY     0x8410   // idle
#define COL_DARKGRAY 0x2104

// --- Display setup ---
Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, true, 172, 320, 34, 0, 34, 0);

// --- LED setup ---
Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// --- WiFi ---
WiFiMulti wifiMulti;

// --- State ---
String currentLevel   = "";
String currentMessage = "";
String currentTool    = "";
String serverHost     = SERVER_HOST_1;  // will switch based on which WiFi connected
bool   lastWifiOk     = false;

// --- Button state ---
bool    btnWasPressed  = false;
unsigned long btnPressTime = 0;

// --- Timing ---
unsigned long lastPoll = 0;
const unsigned long POLL_MS = 2000;


// ============================================================
// LED helpers
// ============================================================

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
}

void ledForLevel(const String &level) {
  if      (level == "working") setLed(120, 60, 0);    // orange
  else if (level == "done")    setLed(0, 80, 0);      // green
  else if (level == "error")   setLed(120, 0, 0);     // red
  else                         setLed(10, 10, 10);    // idle: dim white
}

uint16_t colorForLevel(const String &level) {
  if      (level == "working") return COL_ORANGE;
  else if (level == "done")    return COL_GREEN;
  else if (level == "error")   return COL_RED;
  else                         return COL_GRAY;
}


// ============================================================
// Display helpers
// ============================================================

// Word-wrap text into the body area
void drawWrappedText(const String &text, int x, int y, int maxWidth, uint8_t textSize) {
  int charW = 6 * textSize;
  int charH = 8 * textSize;
  int charsPerLine = maxWidth / charW;

  String remaining = text;
  int curY = y;

  while (remaining.length() > 0 && curY < 300) {
    String line;
    if ((int)remaining.length() <= charsPerLine) {
      line = remaining;
      remaining = "";
    } else {
      // Try to break at a space
      int breakAt = charsPerLine;
      for (int i = charsPerLine; i >= 0; i--) {
        if (remaining[i] == ' ' || remaining[i] == ':') {
          breakAt = i;
          break;
        }
      }
      line = remaining.substring(0, breakAt);
      remaining = remaining.substring(breakAt + 1);
    }
    gfx->setCursor(x, curY);
    gfx->print(line);
    curY += charH + 2;
  }
}

void drawScreen(const String &level, const String &message, const String &tool, bool wifiOk) {
  uint16_t accent = colorForLevel(level);

  gfx->fillScreen(COL_BLACK);

  // --- Header bar (top 52px) ---
  gfx->fillRect(0, 0, 172, 52, accent);
  gfx->setTextColor(COL_BLACK);
  gfx->setTextSize(2);
  String levelLabel = level;
  levelLabel.toUpperCase();
  int labelX = (172 - levelLabel.length() * 12) / 2;
  gfx->setCursor(labelX, 16);
  gfx->print(levelLabel);

  // --- Main message (body) ---
  gfx->setTextColor(COL_WHITE);
  gfx->setTextSize(2);
  drawWrappedText(message, 6, 68, 160, 2);

  // --- Tool name (if present) ---
  if (tool.length() > 0) {
    gfx->setTextColor(COL_DARKGRAY);
    gfx->setTextSize(1);
    gfx->setCursor(6, 260);
    gfx->print("tool: " + tool);
  }

  // --- WiFi status (bottom) ---
  gfx->setTextSize(1);
  if (wifiOk) {
    gfx->setTextColor(COL_GREEN);
    gfx->setCursor(6, 304);
    gfx->print("wifi ok");
  } else {
    gfx->setTextColor(COL_RED);
    gfx->setCursor(6, 304);
    gfx->print("no wifi");
  }
}

void drawConnecting() {
  gfx->fillScreen(COL_BLACK);
  gfx->setTextColor(COL_ORANGE);
  gfx->setTextSize(2);
  gfx->setCursor(20, 140);
  gfx->print("Connecting");
  gfx->setCursor(20, 165);
  gfx->print("to WiFi...");
  setLed(60, 30, 0);
}

void drawButtonFeedback(const String &msg, uint16_t color) {
  // Flash a small banner at the bottom confirming the button press
  gfx->fillRect(0, 270, 172, 40, color);
  gfx->setTextColor(COL_BLACK);
  gfx->setTextSize(2);
  int x = (172 - msg.length() * 12) / 2;
  gfx->setCursor(x, 280);
  gfx->print(msg);
  delay(800);
  // Redraw the last known state
  drawScreen(currentLevel, currentMessage, currentTool, lastWifiOk);
}


// ============================================================
// HTTP helpers
// ============================================================

String buildUrl(const String &path) {
  return "http://" + serverHost + ":" + String(SERVER_PORT) + path;
}

bool pollStatus() {
  HTTPClient http;
  http.begin(buildUrl("/status"));
  http.setTimeout(1500);
  int code = http.GET();

  if (code == 200) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getString());
    http.end();
    if (err) return false;

    String newLevel   = doc["level"]   | "idle";
    String newMessage = doc["message"] | "Waiting...";
    String newTool    = doc["tool"]    | "";

    // Only redraw if something changed
    if (newLevel != currentLevel || newMessage != currentMessage || newTool != currentTool) {
      currentLevel   = newLevel;
      currentMessage = newMessage;
      currentTool    = newTool;
      drawScreen(currentLevel, currentMessage, currentTool, true);
      ledForLevel(currentLevel);
    }
    return true;
  }

  http.end();
  return false;
}

void sendButton(const String &type) {
  HTTPClient http;
  http.begin(buildUrl("/button"));
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(1500);
  String body = "{\"type\":\"" + type + "\"}";
  http.POST(body);
  http.end();
}


// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);

  // Display
  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);
  gfx->begin();

  // LED
  led.begin();
  led.setBrightness(60);
  led.show();

  // Button
  pinMode(BOOT_BTN, INPUT_PULLUP);

  drawConnecting();

  // WiFi — add all networks, connect to strongest in range
  wifiMulti.addAP(WIFI_SSID_1, WIFI_PASS_1);
  wifiMulti.addAP(WIFI_SSID_2, WIFI_PASS_2);

  int tries = 0;
  while (wifiMulti.run() != WL_CONNECTED && tries < 20) {
    delay(500);
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    lastWifiOk = true;
    // Pick server host based on which network we joined
    String ssid = WiFi.SSID();
    if (ssid == WIFI_SSID_1) serverHost = SERVER_HOST_1;
    else                      serverHost = SERVER_HOST_2;

    drawScreen("idle", "Connected!", "", true);
    setLed(0, 60, 0);
    delay(1000);
  } else {
    drawScreen("error", "No WiFi found", "", false);
    setLed(80, 0, 0);
  }
}


// ============================================================
// Loop
// ============================================================

void loop() {
  // --- Button handling ---
  bool btnDown = (digitalRead(BOOT_BTN) == LOW);

  if (btnDown && !btnWasPressed) {
    // Button just pressed
    btnWasPressed = true;
    btnPressTime  = millis();
  }

  if (!btnDown && btnWasPressed) {
    // Button just released
    unsigned long held = millis() - btnPressTime;
    btnWasPressed = false;

    if (held >= LONG_PRESS_MS) {
      // Long press = NO / Pause
      sendButton("long");
      drawButtonFeedback("NO / PAUSE", COL_RED);
    } else {
      // Short press = YES / Continue
      sendButton("short");
      drawButtonFeedback("YES", COL_GREEN);
    }
  }

  // --- WiFi reconnect ---
  if (wifiMulti.run() != WL_CONNECTED) {
    if (lastWifiOk) {
      lastWifiOk = false;
      drawScreen(currentLevel, currentMessage, currentTool, false);
      setLed(60, 0, 0);
    }
    delay(500);
    return;
  }

  if (!lastWifiOk) {
    lastWifiOk = true;
    // Redraw with wifi ok
    drawScreen(currentLevel, currentMessage, currentTool, true);
  }

  // --- Poll server ---
  if (millis() - lastPoll >= POLL_MS) {
    lastPoll = millis();
    bool ok = pollStatus();
    if (!ok && lastWifiOk) {
      // Server not reachable — show idle
      if (currentMessage != "Server offline") {
        currentMessage = "Server offline";
        currentLevel   = "idle";
        drawScreen(currentLevel, currentMessage, "", true);
        ledForLevel("idle");
      }
    }
  }

  delay(50);  // Small delay to debounce and keep loop responsive
}
