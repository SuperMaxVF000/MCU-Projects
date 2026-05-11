#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include "SSD1306Wire.h" // библиотека от Think Pulse

// --- НАСТРОЙКИ WI-FI И ТЕЛЕГРАМ ---
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* botToken = "BOT_TOKEN";

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

// --- НАСТРОЙКИ ДИСПЛЕЯ ---
// Используем те самые пины, которые у тебя заработали!
SSD1306Wire display(0x3c, D5, D6); 

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    Serial.println("Пришло сообщение: " + text);

    // Очищаем экран и выводим текст сообщения
    display.clear();
    display.setFont(ArialMT_Plain_10); // Мелкий шрифт для имени
    display.drawString(0, 0, "From: " + from_name);
    
    display.setFont(ArialMT_Plain_16); // Крупный шрифт для сообщения
    // Функция drawString может не переносить строки сама, 
    // поэтому длинные сообщения могут обрезаться.
    display.drawStringMaxWidth(0, 15, 128, text); 
    
    display.display();

    bot.sendMessage(chat_id, "Принял! Вывел на экран: " + text, "");
  }
}

void setup() {
  Serial.begin(115200);
  
  // Инициализация дисплея
  display.init();
  display.flipScreenVertically(); // Если текст будет вверх ногами — закомментируй
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.drawString(0, 0, "Waiting for...");
  display.drawString(0, 20, "Telegram");
  display.display();

  // Подключение к Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  client.setInsecure(); // Важно для ESP8266, чтобы не возиться с сертификатами

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi подключен!");
  display.clear();
  display.drawString(0, 0, "Bot Ready!");
  display.display();
}

void loop() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    handleNewMessages(numNewMessages);
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
  delay(1000);
}
