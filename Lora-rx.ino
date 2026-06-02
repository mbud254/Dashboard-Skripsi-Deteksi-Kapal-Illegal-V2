#include <SPI.h>
#include <LoRa.h>

#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

void setup() {

  Serial.begin(115200);

  Serial.println("LoRa Receiver");

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  while (!LoRa.begin(433E6)) {

    Serial.println("LoRa init failed...");
    delay(1000);
  }

  // Harus sama dengan transmitter
  LoRa.setSyncWord(0xF3);

  Serial.println("LoRa Receiver Ready");
}

void loop() {

  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    Serial.print("Received packet: ");

    while (LoRa.available()) {

      Serial.print((char)LoRa.read());
    }

    Serial.print(" | RSSI: ");
    Serial.print(LoRa.packetRssi());

    Serial.print(" dBm | SNR: ");
    Serial.print(LoRa.packetSnr());

    Serial.println(" dB");
  }
}
