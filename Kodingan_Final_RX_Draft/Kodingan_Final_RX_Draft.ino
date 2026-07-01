#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>

// =====================================================
// KONFIGURASI WIFI & SERVER (Samain kayak di Mic)
// =====================================================
const char* ssid = "POCO X7";
const char* password = "rrayrann"; 
const char* serverName = "http://10.237.210.175:3000/api/sensor"; 

// =====================================================
// KONFIGURASI LORA
// =====================================================
#define SS_PIN   5
#define RST_PIN  14
#define DIO0_PIN 26

volatile bool loraReady = false;
unsigned long packetCounter = 0;

// =====================================================
// FUNGSI PARSING & KIRIM KE DASHBOARD NEXT.JS
// =====================================================
void processAndSendData(String incoming, int rssi, float snr) {
  // Format dari TX: LAT=-6.234,LON=106.123,SAT=0,SPL=65.2,FFT=0.0,EMA=65.2
  int latIdx = incoming.indexOf("LAT=");
  int lonIdx = incoming.indexOf(",LON=");
  int satIdx = incoming.indexOf(",SAT=");
  int splIdx = incoming.indexOf(",SPL=");
  int fftIdx = incoming.indexOf(",FFT=");
  int emaIdx = incoming.indexOf(",EMA=");

  // Kalau data corrupt/kpotong di jalan, batalin biar ga error
  if (latIdx == -1 || splIdx == -1) {
    Serial.println("[ERROR] Payload tidak lengkap!");
    return; 
  }

  // Motong teks buat ngambil angkanya doang
  String latStr = incoming.substring(latIdx + 4, lonIdx);
  String lonStr = incoming.substring(lonIdx + 5, satIdx);
  String satStr = incoming.substring(satIdx + 5, splIdx);
  String splStr = incoming.substring(splIdx + 5, fftIdx);
  String fftStr = incoming.substring(fftIdx + 5, emaIdx);
  String emaStr = incoming.substring(emaIdx + 5);

  Serial.println("\n[PARSED DATA]");
  Serial.print("GPS -> LAT: " + latStr + " | LON: " + lonStr + " | SAT: " + satStr + "\n");
  Serial.print("MIC -> SPL: " + splStr + " | FFT: " + fftStr + " | EMA: " + emaStr + "\n");
  Serial.print("LORA-> RSSI: " + String(rssi) + " | SNR: " + String(snr) + "\n");

  // Kirim HTTP POST ke Dashboard
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    // Bungkus jadi JSON (Sekalian masukin parameter Lora & GPS)
    String jsonPayload = "{";
    jsonPayload += "\"spl\":" + splStr + ",";
    jsonPayload += "\"fft\":" + fftStr + ",";
    jsonPayload += "\"ema\":" + emaStr + ",";
    jsonPayload += "\"lat\":" + latStr + ",";
    jsonPayload += "\"lon\":" + lonStr + ",";
    jsonPayload += "\"sat\":" + satStr + ",";
    jsonPayload += "\"rssi\":" + String(rssi) + ",";
    jsonPayload += "\"snr\":" + String(snr);
    jsonPayload += "}";

    int httpCode = http.POST(jsonPayload);
    if (httpCode > 0) {
      Serial.println("[HTTP] Data Sukses Ditembak ke Dashboard! Code: " + String(httpCode));
    } else {
      Serial.println("[HTTP] Gagal Nembak IP Laptop! Code: " + String(httpCode));
    }
    http.end();
  } else {
    Serial.println("[WIFI] Putus dari Tethering!");
  }
}

// =====================================================
// TASK: LORA RX (Jalan di Background via RTOS)
// =====================================================
void TaskLoRaRX(void *pvParameters) {
  Serial.println("[LORA] RX Task Started - Menunggu Data Pelampung...");
  for (;;) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String incoming = "";
      while (LoRa.available()) {
        incoming += (char)LoRa.read();
      }
      
      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();
      packetCounter++;

      Serial.println("\n========== RX (PAKET KE-" + String(packetCounter) + ") ==========");
      Serial.println("RAW: " + incoming);
      
      // Lempar ke fungsi parsing buat dikirim ke Next.js
      processAndSendData(incoming, rssi, snr);
      Serial.println("======================================");
    }
    
    // Kasih napas ke Core ESP32 (Delay 20ms)
    vTaskDelay(pdMS_TO_TICKS(20)); 
  }
}

// =====================================================
// INISIALISASI LORA
// =====================================================
bool initLoRa() {
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW); delay(20);
  digitalWrite(RST_PIN, HIGH); delay(100);
  
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  
  int retry = 0;
  while (!LoRa.begin(433E6)) {
    retry++; delay(1000);
    if (retry >= 10) return false;
  }
  LoRa.setSyncWord(0xF3); // Wajib sama dengan TX
  return true;
}

// =====================================================
// SETUP UTAMA
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("  BASE STATION: LORA RX -> WIFI  ");
  Serial.println("=================================");

  // 1. KONEK WIFI TETHERING
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n[WIFI] Connected! IP ESP32 Darat: " + WiFi.localIP().toString());

  // 2. INIT LORA
  loraReady = initLoRa();
  if (loraReady) {
    Serial.println("[INIT] LoRa OK");
    // Jalankan Task RX di RTOS
    xTaskCreatePinnedToCore(TaskLoRaRX, "TaskLoRaRX", 8192, NULL, 1, NULL, 1);
  } else {
    Serial.println("[INIT] LoRa GAGAL! Cek Kabel SPI.");
  }
}

void loop() {
  // Biarkan RTOS yang bekerja
  vTaskDelete(NULL);
}