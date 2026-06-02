#include <SPI.h>
#include <LoRa.h>

#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

unsigned long lastDebug = 0;

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("LoRa Receiver Starting...");
  Serial.println("================================");

  // Inisialisasi SPI ESP32
  SPI.begin(18, 19, 23, 5);

  Serial.println("SPI Initialized");

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  Serial.println("Trying to initialize LoRa...");

  while (!LoRa.begin(433E6)) {

    Serial.println("LoRa init failed!");
    Serial.println("Check:");
    Serial.println("- Wiring NSS");
    Serial.println("- Wiring RST");
    Serial.println("- Wiring DIO0");
    Serial.println("- Power 3.3V");
    Serial.println("- Antenna");

    delay(2000);
  }

  LoRa.setSyncWord(0xF3);

  Serial.println("LoRa Receiver Ready!");
  Serial.println("Frequency : 433 MHz");
  Serial.println("Sync Word : 0xF3");
  Serial.println("Waiting for packets...");
  Serial.println();
}

void loop() {

  // Debug heartbeat tiap 5 detik
  if (millis() - lastDebug > 5000) {

    Serial.print("[");
    Serial.print(millis() / 1000);
    Serial.println("s] Waiting packet...");

    lastDebug = millis();
  }

  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    Serial.println();
    Serial.println("========== PACKET RECEIVED ==========");

    Serial.print("Packet Size : ");
    Serial.println(packetSize);

    String receivedData = "";

    while (LoRa.available()) {

      char c = (char)LoRa.read();

      receivedData += c;
    }

    Serial.print("Message     : ");
    Serial.println(receivedData);

    Serial.print("RSSI        : ");
    Serial.print(LoRa.packetRssi());
    Serial.println(" dBm");

    Serial.print("SNR         : ");
    Serial.print(LoRa.packetSnr());
    Serial.println(" dB");

    Serial.print("Time Since Boot : ");
    Serial.print(millis()/1000);
    Serial.println(" s");

    Serial.println("=====================================");
    Serial.println();
  }
}
