// ============================================================
//  ESP8266 + SSD1306 OLED + Telegram Bot
//  Дисплей: SSD1306Wire (ThingPulse), SDA=D5, SCL=D6
//  Функции: смайлики, NTP (МСК), курс USD/RUB, тамагочи,
//           вывод произвольного текста с Telegram
// ============================================================

#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <SSD1306Wire.h>
#include <NTPClient.h>
#include <WiFiUDP.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// ──────────────────────────────────────────────
//  НАСТРОЙКИ — заполни перед прошивкой
// ──────────────────────────────────────────────
const char* WIFI_SSID     = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";
const char* BOT_TOKEN     = "TELEGRAM_BOT_TOKEN";

// ExchangeRate-API (бесплатный ключ на https://www.exchangerate-api.com)
const char* EXCHANGE_API_KEY = "EXCHANGERATE_API_KEY";

// ──────────────────────────────────────────────
//  HARDWARE
// ──────────────────────────────────────────────
// ВАЖНО: стандартные I2C пины не работают на этой плате!
#define SDA_PIN D5
#define SCL_PIN D6
SSD1306Wire display(0x3C, SDA_PIN, SCL_PIN);

// ──────────────────────────────────────────────
//  TELEGRAM
// ──────────────────────────────────────────────
WiFiClientSecure secureClient;
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

const unsigned long BOT_POLL_INTERVAL = 1000; // мс между опросами
unsigned long lastBotCheck = 0;

// ──────────────────────────────────────────────
//  NTP (UTC+3 = Москва)
// ──────────────────────────────────────────────
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3 * 3600, 60000);

// ──────────────────────────────────────────────
//  СОСТОЯНИЕ ДИСПЛЕЯ
// ──────────────────────────────────────────────
enum DisplayMode {
  MODE_IDLE,
  MODE_TEXT,
  MODE_TIME,
  MODE_CURRENCY,
  MODE_TAMAGOTCHI
};

DisplayMode currentMode = MODE_IDLE;

String displayLine1 = "Ready";   // основная строка
String displayLine2 = "";        // вторая строка (для валюты)

// ──────────────────────────────────────────────
//  ТАМАГОЧИ
// ──────────────────────────────────────────────
enum TamaFace {
  TAMA_HAPPY,   // ^_^
  TAMA_SAD,     // >_<
  TAMA_SHOCKED, // O_O
  TAMA_LOVE,    // @_@
};

TamaFace tamaFace = TAMA_HAPPY;
unsigned long tamaAnimTimer = 0;
bool tamaBlinkState = false;

String getTamaString(TamaFace face) {
  switch (face) {
    case TAMA_HAPPY:   return "( ^_^ )";
    case TAMA_SAD:     return "( >_< )";
    case TAMA_SHOCKED: return "( O_O )";
    case TAMA_LOVE:    return "( @_@ )";
    default:           return "( ^_^ )";
  }
}

// ──────────────────────────────────────────────
//  ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ──────────────────────────────────────────────

// Получить курс USD/RUB через ExchangeRate-API
String fetchCurrency() {
  if (WiFi.status() != WL_CONNECTED) return "No WiFi";

  WiFiClientSecure httpClient;
  httpClient.setInsecure();

  HTTPClient https;
  String url = String("https://v6.exchangerate-api.com/v6/") + EXCHANGE_API_KEY + "/latest/USD";

  https.begin(httpClient, url);
  int code = https.GET();

  if (code == HTTP_CODE_OK) {
    String payload = https.getString();
    https.end();

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      float rub = doc["conversion_rates"]["RUB"];
      return String("1 USD = ") + String(rub, 2) + " RUB";
    }
    return "Parse error";
  }
  https.end();
  return "HTTP err " + String(code);
}

// Клавиатура Reply Keyboard для Telegram
void sendMainKeyboard(String chat_id) {
  String keyboard = F(
    "[[\"UwU\",\":3\",\">_<\"],"
    "[\"Hello World\",\"Time\"],"
    "[\"Currency\",\"Tamagotchi\"]]"
  );
  bot.sendMessageWithReplyKeyboard(chat_id, "Выбери действие:", "", keyboard, true);
}

// ──────────────────────────────────────────────
//  ОБРАБОТКА СООБЩЕНИЙ TELEGRAM
// ──────────────────────────────────────────────
void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text    = bot.messages[i].text;

    Serial.print("MSG [");
    Serial.print(chat_id);
    Serial.print("]: ");
    Serial.println(text);

    // ── /start ──────────────────────────────
    if (text == "/start") {
      bot.sendMessage(chat_id, "Привет! Я управляю OLED-дисплеем. Выбери команду:", "");
      sendMainKeyboard(chat_id);
      displayLine1 = "Bot started";
      displayLine2 = "";
      currentMode  = MODE_TEXT;
      return;
    }

    // ── Смайлики ───────────────────────────
    if (text == "UwU" || text == ":3" || text == ">_<" || text == "Hello World") {
      displayLine1 = text;
      displayLine2 = "";
      currentMode  = MODE_TEXT;
      bot.sendMessage(chat_id, "Выведено: " + text, "");
      return;
    }

    // ── Время ──────────────────────────────
    if (text == "Time") {
      currentMode  = MODE_TIME;
      displayLine2 = "";
      bot.sendMessage(chat_id, "Показываю время МСК", "");
      return;
    }

    // ── Курс валюты ────────────────────────
    if (text == "Currency") {
      currentMode  = MODE_TEXT;
      displayLine1 = "Loading...";
      displayLine2 = "";
      drawDisplay(); // сразу показать "loading"

      String rate  = fetchCurrency();
      currentMode  = MODE_CURRENCY;
      displayLine1 = "USD/RUB";
      displayLine2 = rate;
      bot.sendMessage(chat_id, rate, "");
      return;
    }

    // ── Тамагочи ───────────────────────────
    if (text == "Tamagotchi") {
      currentMode = MODE_TAMAGOTCHI;
      tamaFace    = TAMA_HAPPY;
      bot.sendMessage(chat_id, "Тамагочи запущен!\nУправляй: UwU=^_^  >_<=sad  :3=@_@", "");
      // перекинем кнопки тамагочи
      String kb = F("[[\"UwU\",\">_<\",\":3\"],[\"Back\"]]");
      bot.sendMessageWithReplyKeyboard(chat_id, "Выбери эмоцию:", "", kb, true);
      return;
    }

    // ── В режиме тамагочи: смена лица ──────
    if (currentMode == MODE_TAMAGOTCHI) {
      if (text == "UwU")    { tamaFace = TAMA_HAPPY;   bot.sendMessage(chat_id, "( ^_^ )", ""); return; }
      if (text == ">_<")    { tamaFace = TAMA_SAD;     bot.sendMessage(chat_id, "( >_< )", ""); return; }
      if (text == ":3")     { tamaFace = TAMA_LOVE;    bot.sendMessage(chat_id, "( @_@ )", ""); return; }
      if (text == "Back")   { currentMode = MODE_IDLE; sendMainKeyboard(chat_id); return; }
    }

    // ── Произвольный текст ─────────────────
    if (text.length() > 0 && text[0] != '/') {
      displayLine1 = text.substring(0, 20);          // первые 20 символов
      displayLine2 = (text.length() > 20)
                     ? text.substring(20, 40)        // следующие 20
                     : "";
      currentMode  = MODE_TEXT;
      bot.sendMessage(chat_id, "Выведено на дисплей!", "");
      return;
    }

    // ── Неизвестная команда ─────────────────
    bot.sendMessage(chat_id, "Не понял. Используй /start для меню.", "");
  }
}

// ──────────────────────────────────────────────
//  ОТРИСОВКА ДИСПЛЕЯ
// ──────────────────────────────────────────────
void drawDisplay() {
  display.clear();

  // Синяя зона: Y от 16 до 64 (желтая зона Y 0-15 остаётся чёрной)
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  int centerX = 64;

  switch (currentMode) {

    // ── Idle ───────────────────────────────
    case MODE_IDLE:
      display.setFont(ArialMT_Plain_16);
      display.drawString(centerX, 24, "ESP8266");
      display.setFont(ArialMT_Plain_10);
      display.drawString(centerX, 44, "Send /start");
      break;

    // ── Произвольный текст / смайлики ──────
    case MODE_TEXT:
      if (displayLine2.isEmpty()) {
        // Одна строка — крупно по центру синей зоны
        display.setFont(ArialMT_Plain_24);
        display.drawString(centerX, 20, displayLine1);
      } else {
        // Две строки
        display.setFont(ArialMT_Plain_16);
        display.drawString(centerX, 18, displayLine1);
        display.setFont(ArialMT_Plain_10);
        display.drawString(centerX, 40, displayLine2);
      }
      break;

    // ── Время МСК ──────────────────────────
    case MODE_TIME: {
      timeClient.update();
      String timeStr = timeClient.getFormattedTime(); // HH:MM:SS
      display.setFont(ArialMT_Plain_24);
      display.drawString(centerX, 18, timeStr);
      display.setFont(ArialMT_Plain_10);
      display.drawString(centerX, 48, "Moscow (UTC+3)");
      break;
    }

    // ── Курс валюты ────────────────────────
    case MODE_CURRENCY:
      display.setFont(ArialMT_Plain_16);
      display.drawString(centerX, 18, displayLine1);
      display.setFont(ArialMT_Plain_10);
      display.drawString(centerX, 40, displayLine2);
      break;

    // ── Тамагочи ───────────────────────────
    case MODE_TAMAGOTCHI: {
      // Анимация мигания каждые 800 мс
      if (millis() - tamaAnimTimer > 800) {
        tamaAnimTimer = millis();
        tamaBlinkState = !tamaBlinkState;
      }

      String face = getTamaString(tamaFace);

      // Мигание: в состоянии "моргания" заменяем _ на -
      if (tamaBlinkState && tamaFace == TAMA_HAPPY) {
        face = "( -_- )";
      }

      display.setFont(ArialMT_Plain_16);
      display.drawString(centerX, 22, face);
      display.setFont(ArialMT_Plain_10);
      display.drawString(centerX, 46, "Tamagotchi");
      break;
    }
  }

  display.display();
}

// ──────────────────────────────────────────────
//  SETUP
// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n\nStarting...");

  // Инициализация дисплея
  display.init();
  display.flipScreenVertically();   // убрать если изображение перевёрнуто
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 28, "Connecting WiFi...");
  display.display();

  // Подключение к WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // Показать IP на дисплее
  display.clear();
  display.drawString(64, 24, "WiFi OK");
  display.drawString(64, 38, WiFi.localIP().toString());
  display.display();
  delay(1500);

  // SSL: пропустить проверку сертификата Telegram
  secureClient.setInsecure();

  // Запуск NTP
  timeClient.begin();
  timeClient.update();

  currentMode = MODE_IDLE;
  Serial.println("Setup complete.");
}

// ──────────────────────────────────────────────
//  LOOP
// ──────────────────────────────────────────────
void loop() {
  // Опрос Telegram с интервалом
  if (millis() - lastBotCheck > BOT_POLL_INTERVAL) {
    lastBotCheck = millis();
    int numMsg = bot.getUpdates(bot.last_message_received + 1);
    if (numMsg > 0) {
      handleNewMessages(numMsg);
    }
  }

  // Обновление дисплея
  drawDisplay();

  // NTP обновляется автоматически через WiFiUDP
  if (currentMode == MODE_TIME) {
    timeClient.update();
  }

  delay(50); // небольшая пауза для стабильности
}
