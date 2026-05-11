// ============================================================
//  Omega Kerfur — Face Animation v4
//  NORMAL:   OwO
//  BLINKING: -w-
//  HAPPY:    >w<
//  Одна строка, по центру синей зоны (Y 16..63)
// ============================================================

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <Wire.h>
#include "SSD1306Wire.h"

const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";
const char* BOT_TOKEN     = "YOUR_BOT_TOKEN";

SSD1306Wire display(0x3C, D5, D6);

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

constexpr unsigned long BOT_POLL_MS  = 1000;
constexpr unsigned long BLINK_MS     = 200;
constexpr unsigned long HAPPY_MS     = 5000;
constexpr unsigned long BLINK_MIN_MS = 3000;
constexpr unsigned long BLINK_MAX_MS = 7000;

enum FaceState { NORMAL, BLINKING, HAPPY };
FaceState currentState = NORMAL;

unsigned long lastBotPoll    = 0;
unsigned long blinkStartTime = 0;
unsigned long happyStartTime = 0;
unsigned long nextBlinkTime  = 0;

void scheduleNextBlink() {
  nextBlinkTime = millis() + random(BLINK_MIN_MS, BLINK_MAX_MS + 1);
}

// ── Отрисовка ────────────────────────────────────────────────
void drawFace() {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);

  // Синяя зона Y=16..63, центр = 39
  // ArialMT_Plain_24 высота ~24px, центрируем: Y = 39 - 12 = 27
  const int Y = 27;

  switch (currentState) {
    case NORMAL:   display.drawString(64, Y, "OwO"); break;
    case BLINKING: display.drawString(64, Y, "-w-"); break;
    case HAPPY:    display.drawString(64, Y, ">w<"); break;
  }

  display.display();
}

// ── Машина состояний ─────────────────────────────────────────
void updateFace() {
  unsigned long now = millis();

  if (currentState == HAPPY) {
    if (now - happyStartTime >= HAPPY_MS) {
      currentState = NORMAL;
      scheduleNextBlink();
    }
    return;
  }

  if (currentState == BLINKING) {
    if (now - blinkStartTime >= BLINK_MS) {
      currentState = NORMAL;
      scheduleNextBlink();
    }
    return;
  }

  if (now >= nextBlinkTime) {
    currentState   = BLINKING;
    blinkStartTime = now;
  }
}

// ── Telegram ─────────────────────────────────────────────────
void handleNewMessages(int n) {
  for (int i = 0; i < n; i++) {
    String text    = bot.messages[i].text;
    String chat_id = bot.messages[i].chat_id;

    if (text == "/pet") {
      currentState   = HAPPY;
      happyStartTime = millis();
      bot.sendMessage(chat_id, ">w< *happy noises*", "");
    } else if (text == "/start" || text == "/help") {
      bot.sendMessage(chat_id, "Omega Kerfur online!\n/pet — погладить", "");
    }
  }
}

// ── Setup / Loop ─────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  display.init();
  display.flipScreenVertically();
  display.setContrast(255);
  display.clear();
  display.display();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(64, 28, "Connecting...");
    display.display();
  }

  client.setInsecure();
  scheduleNextBlink();
}

void loop() {
  updateFace();
  drawFace();

  unsigned long now = millis();
  if (now - lastBotPoll >= BOT_POLL_MS) {
    lastBotPoll = now;
    int n = bot.getUpdates(bot.last_message_received + 1);
    while (n) {
      handleNewMessages(n);
      n = bot.getUpdates(bot.last_message_received + 1);
    }
  }
}
