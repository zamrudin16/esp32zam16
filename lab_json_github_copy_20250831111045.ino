#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "DHT.h"

// ==== Konfigurasi Wi-Fi ====
const char* ssid     = "IoT-KPTK";
const char* password = "iotkptk123";

// ==== Konfigurasi DHT11 ====
#define DHTPIN 15
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ==== Konfigurasi OTA ====
#define FW_VERSION "1.3"           // Versi firmware lokal
#define SECRET_TOKEN "ABC123XYZ"   // Token rahasia (harus sama di JSON server)
const char* update_json_url = "https://raw.githubusercontent.com/zamrudin16/esp32zam16/main/update.json";

// Interval pengecekan OTA (ms)
unsigned long checkInterval = 10000; 
unsigned long lastCheck = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("\nInisialisasi...");

  // Mulai Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Mulai DHT
  dht.begin();
}

void loop() {
  // === Baca DHT11 ===
  float suhu = dht.readTemperature();
  float kelembaban = dht.readHumidity();

  if (!isnan(suhu) && !isnan(kelembaban)) {
    //Serial.printf("Temperature: %.1f°C | Humadity: %.1f%%\n", suhu, kelembaban);
    Serial.println(suhu);
    Serial.println(kelembaban);
  } else {
    Serial.println("Sensor Suhu Rusak!");
  }

  // === Cek update OTA setiap interval ===
  Serial.println("Cek millis");
  if (millis() - lastCheck > checkInterval) {
    Serial.println("lakukan cek update");
    lastCheck = millis();
    checkForUpdate();
  }

  delay(2000); // jeda baca sensor
}

void checkForUpdate() {
  Serial.println("\nMemeriksa update firmware...");
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(update_json_url);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Data JSON diterima:");
      Serial.println(payload);

      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.println("Gagal parsing JSON!");
        return;
      }

      String newVersion = doc["version"];
      String serverToken = doc["token"];
      String firmwareURL = doc["firmware_url"];

      // Verifikasi token
      if (serverToken != SECRET_TOKEN) {
        Serial.println("Token tidak valid! Update dibatalkan.");
        return;
      }

      Serial.printf("Versi lokal: %s | Versi server: %s\n", FW_VERSION, newVersion.c_str());

      if (newVersion != FW_VERSION) {
        Serial.println("Versi baru ditemukan! Memulai OTA...");
        performOTA(firmwareURL);
      } else {
        Serial.println("Versi sudah terbaru.");
      }
    } else {
      Serial.printf("HTTP Error saat akses JSON: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("WiFi terputus, tidak bisa cek update.");
  }
}

void performOTA(String firmwareURL) {
  WiFiClient client;
  HTTPClient http;

  Serial.println("Mengunduh firmware baru...");
  if (http.begin(client, firmwareURL)) {
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      WiFiClient *stream = http.getStreamPtr();

      if (contentLength > 0) {
        if (!Update.begin(contentLength)) {
          Serial.println("Gagal memulai update");
          return;
        }

        size_t written = Update.writeStream(*stream);
        if (written == contentLength) {
          Serial.println("Update firmware selesai diunduh.");
        } else {
          Serial.printf("Hanya %d dari %d byte yang tertulis\n", written, contentLength);
        }

        if (Update.end()) {
          if (Update.isFinished()) {
            Serial.println("Update sukses! Reboot...");
            delay(1000);
            ESP.restart();
          } else {
            Serial.println("Update gagal diselesaikan.");
          }
        } else {
          Serial.printf("Error Update: %s\n", Update.errorString());
        }
      } else {
        Serial.println("Ukuran file tidak valid.");
      }
    } else {
      Serial.printf("HTTP Error saat unduh firmware: %d\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("Gagal terhubung ke server firmware.");
  }
}