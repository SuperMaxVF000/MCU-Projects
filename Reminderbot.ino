#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <EEPROM.h>

// ======= НАСТРОЙКИ =======
#define WIFI_SSID     "SSID"
#define WIFI_PASSWORD "PASSWORD"
#define BOT_TOKEN     "BOT_TOKEN"
#define TIMEZONE      3          // Москва UTC+3 (измени под себя)
// =========================

#define LED_PIN       2
#define CHECK_DELAY   1000
#define MAX_REMINDERS 10
#define EEPROM_SIZE   1024
#define EEPROM_MAGIC  0xAB   // Метка что данные валидны

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
unsigned long lastTimeBotRan = 0;

//Структура напоминания
struct Reminder {
  int    hour;
  int    minute;
  char   text[64];
  char   chat_id[20];
  bool   active;
  bool   triggered;
};

Reminder reminders[MAX_REMINDERS];
int reminderCount = 0;

// ==============================================
//              МИГАНИЕ СВЕТОДИОДА
// ==============================================

void blinkLED(int times, int delayMs = 100) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, LOW);   // Включить (инвертирован)
    delay(delayMs);
    digitalWrite(LED_PIN, HIGH);  // Выключить
    delay(delayMs);
  }
}

// /add  — 2 быстрых мигания
// /list — 3 быстрых мигания
// /del  — 1 долгое мигание
// Напоминание сработало — 5 быстрых миганий

// ==============================================
//                   EEPROM
// ==============================================

// Структура хранения в EEPROM:
// [0]        — magic byte (0xAB)
// [1]        — кол-во напоминаний
// [2...]     — массив Reminder

void saveToEEPROM() {
  EEPROM.begin(EEPROM_SIZE);

  // Записываем magic byte
  EEPROM.write(0, EEPROM_MAGIC);

  // Считаем только активные
  int count = 0;
  for (int i = 0; i < reminderCount; i++) {
    if (reminders[i].active) count++;
  }
  EEPROM.write(1, count);

  // Записываем активные напоминания
  int offset = 2;
  for (int i = 0; i < reminderCount; i++) {
    if (!reminders[i].active) continue;
    EEPROM.put(offset, reminders[i]);
    offset += sizeof(Reminder);
  }

  EEPROM.commit();
  EEPROM.end();
  Serial.println("💾 Сохранено в EEPROM. Напоминаний: " + String(count));
}

void loadFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);

  byte magic = EEPROM.read(0);
  if (magic != EEPROM_MAGIC) {
    Serial.println("⚠️ EEPROM пуст или повреждён — начинаем чисто.");
    EEPROM.end();
    reminderCount = 0;
    return;
  }

  int count = EEPROM.read(1);
  if (count > MAX_REMINDERS) count = MAX_REMINDERS;

  int offset = 2;
  reminderCount = 0;
  for (int i = 0; i < count; i++) {
    EEPROM.get(offset, reminders[reminderCount]);
    reminders[reminderCount].active    = true;
    reminders[reminderCount].triggered = false;  // Сбрасываем триггер после перезагрузки
    offset += sizeof(Reminder);
    reminderCount++;
  }

  EEPROM.end();
  Serial.println("✅ Загружено из EEPROM. Напоминаний: " + String(reminderCount));
}

// ==============================================
//              РАБОТА С ВРЕМЕНЕМ
// ==============================================

void getTime(int &h, int &m, int &s) {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  h = t->tm_hour;
  m = t->tm_min;
  s = t->tm_sec;
}

// ==============================================
//            РАБОТА С НАПОМИНАНИЯМИ
// ==============================================

bool addReminder(String chat_id, String input) {
  int colonIdx = input.indexOf(':');
  int spaceIdx = input.indexOf(' ');

  if (colonIdx == -1 || spaceIdx == -1 || spaceIdx < colonIdx) return false;

  int hour   = input.substring(0, colonIdx).toInt();
  int minute = input.substring(colonIdx + 1, spaceIdx).toInt();
  String txt = input.substring(spaceIdx + 1);
  txt.trim();

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || txt.length() == 0) return false;
  if (reminderCount >= MAX_REMINDERS) return false;

  reminders[reminderCount].hour      = hour;
  reminders[reminderCount].minute    = minute;
  reminders[reminderCount].active    = true;
  reminders[reminderCount].triggered = false;

  // Копируем строки в char[]
  strncpy(reminders[reminderCount].text,    txt.c_str(),     63);
  strncpy(reminders[reminderCount].chat_id, chat_id.c_str(), 19);
  reminders[reminderCount].text[63]    = '\0';
  reminders[reminderCount].chat_id[19] = '\0';

  reminderCount++;
  saveToEEPROM();
  return true;
}

String listReminders(String chat_id) {
  String result = "";
  int idx = 1;
  for (int i = 0; i < reminderCount; i++) {
    if (reminders[i].active && String(reminders[i].chat_id) == chat_id) {
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", reminders[i].hour, reminders[i].minute);
      result += String(idx) + ". ⏰ " + String(timeStr) + " — " + String(reminders[i].text) + "\n";
      idx++;
    }
  }
  if (result == "") result = "Напоминаний нет.";
  return result;
}

bool deleteReminder(String chat_id, int num) {
  int idx = 1;
  for (int i = 0; i < reminderCount; i++) {
    if (reminders[i].active && String(reminders[i].chat_id) == chat_id) {
      if (idx == num) {
        reminders[i].active = false;
        saveToEEPROM();
        return true;
      }
      idx++;
    }
  }
  return false;
}

// ==============================================
//          ОБРАБОТКА СООБЩЕНИЙ TELEGRAM
// ==============================================

void handleMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text    = bot.messages[i].text;
    text.trim();

    Serial.println("📨 Получено: " + text);

    // /start
    if (text == "/start") {
      blinkLED(1, 200);
      bot.sendMessage(chat_id,
        "👋 Привет! Я бот-напоминалка!\n\n"
        "📌 Команды:\n"
        "➕ /add ЧЧ:ММ Текст — добавить\n"
        "    Пример: /add 15:30 Купить молоко\n\n"
        "📋 /list — список напоминаний\n"
        "🗑 /del N — удалить напоминание №N\n"
        "🕐 /time — текущее время\n"
        "🗑 /clear — удалить ВСЕ напоминания", "");

    // /time
    } else if (text == "/time") {
      blinkLED(1, 200);
      int h, m, s;
      getTime(h, m, s);
      char buf[30];
      sprintf(buf, "🕐 Сейчас: %02d:%02d:%02d", h, m, s);
      bot.sendMessage(chat_id, String(buf), "");

    // /list
    } else if (text == "/list") {
      blinkLED(3, 80);   // 3 быстрых мигания
      bot.sendMessage(chat_id,
        "📋 Твои напоминания:\n\n" + listReminders(chat_id), "");

    // /del N
    } else if (text.startsWith("/del")) {
      String numStr = text.substring(4);
      numStr.trim();
      int num = numStr.toInt();
      if (num <= 0) {
        bot.sendMessage(chat_id, "❌ Укажи номер: /del 1", "");
      } else if (deleteReminder(chat_id, num)) {
        blinkLED(1, 400);  // 1 долгое мигание
        bot.sendMessage(chat_id, "✅ Напоминание №" + String(num) + " удалено!", "");
      } else {
        bot.sendMessage(chat_id, "❌ Напоминание №" + String(num) + " не найдено.", "");
      }

    // /clear — удалить все
    } else if (text == "/clear") {
      for (int j = 0; j < reminderCount; j++) {
        reminders[j].active = false;
      }
      reminderCount = 0;
      saveToEEPROM();
      blinkLED(5, 60);   // 5 быстрых миганий
      bot.sendMessage(chat_id, "🗑 Все напоминания удалены!", "");

    // /add ЧЧ:ММ Текст
    } else if (text.startsWith("/add")) {
      String input = text.substring(4);
      input.trim();
      if (input.length() == 0) {
        bot.sendMessage(chat_id,
          "❌ Формат: /add ЧЧ:ММ Текст\nПример: /add 15:30 Купить молоко", "");
      } else if (reminderCount >= MAX_REMINDERS) {
        bot.sendMessage(chat_id,
          "❌ Максимум " + String(MAX_REMINDERS) + " напоминаний!\nУдали старые через /del", "");
      } else if (addReminder(chat_id, input)) {
        blinkLED(2, 100);  // 2 быстрых мигания

        int colonIdx = input.indexOf(':');
        int spaceIdx = input.indexOf(' ');
        int h = input.substring(0, colonIdx).toInt();
        int m = input.substring(colonIdx + 1, spaceIdx).toInt();
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", h, m);

        bot.sendMessage(chat_id,
          "✅ Напоминание добавлено!\n"
          "⏰ Время: " + String(timeStr) +
          "\n📝 Текст: " + input.substring(spaceIdx + 1) +
          "\n💾 Сохранено в память!", "");
      } else {
        bot.sendMessage(chat_id,
          "❌ Ошибка! Проверь формат:\n/add ЧЧ:ММ Текст", "");
      }

    } else {
      bot.sendMessage(chat_id,
        "❓ Не понял команду.\nНапиши /start для справки.", "");
    }
  }
}

// ==============================================
//           ПРОВЕРКА НАПОМИНАНИЙ
// ==============================================

void checkReminders() {
  int h, m, s;
  getTime(h, m, s);

  for (int i = 0; i < reminderCount; i++) {
    if (!reminders[i].active) continue;

    if (reminders[i].hour == h && reminders[i].minute == m) {
      if (!reminders[i].triggered) {
        char timeStr[6];
        sprintf(timeStr, "%02d:%02d", h, m);

        bot.sendMessage(String(reminders[i].chat_id),
          "🔔 *НАПОМИНАНИЕ!*\n"
          "⏰ " + String(timeStr) +
          "\n📝 " + String(reminders[i].text), "Markdown");

        blinkLED(5, 80);  // 5 миганий при срабатывании
        reminders[i].triggered = true;
        Serial.println("🔔 Напоминание отправлено: " + String(reminders[i].text));
      }
    } else {
      reminders[i].triggered = false;
    }
  }
}

// ==============================================
//                   SETUP
// ==============================================

void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Выключен

  // Загружаем напоминания из EEPROM
  loadFromEEPROM();

  // Wi-Fi
  Serial.print("Подключение к Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));  // Мигаем при подключении
  }
  digitalWrite(LED_PIN, HIGH);  // Выключаем после подключения
  Serial.println("\n✅ Подключено! IP: " + WiFi.localIP().toString());

  client.setInsecure();

  // NTP время
  configTime(TIMEZONE * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Синхронизация времени");
  while (time(nullptr) < 1000000000) {
    delay(500);
    Serial.print(".");
  }

  int h, m, s;
  getTime(h, m, s);
  char buf[40];
  sprintf(buf, "✅ Время: %02d:%02d:%02d", h, m, s);
  Serial.println("\n" + String(buf));

  // Мигаем 3 раза — всё готово!
  blinkLED(3, 150);
  Serial.println("🤖 Бот запущен!");
}

// ==============================================
//                   LOOP
// ==============================================

void loop() {
  // Проверяем Telegram
  if (millis() - lastTimeBotRan > CHECK_DELAY) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  // Проверяем напоминания
  checkReminders();
  delay(500);
}
