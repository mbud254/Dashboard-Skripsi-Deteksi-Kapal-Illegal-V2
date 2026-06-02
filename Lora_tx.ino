#include <SPI.h>
#include <LoRa.h>

#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  26

int counter = 0;

void setup() {

  Serial.begin(115200);

  Serial.println("LoRa Sender");

  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);

  while (!LoRa.begin(433E6)) {

    Serial.println("LoRa init failed...");
    delay(1000);
  }

  LoRa.setSyncWord(0xF3);

  Serial.println("LoRa Initializing OK!");
}

void loop() {

  Serial.print("Sending packet ");
  Serial.println(counter);

  LoRa.beginPacket();
  LoRa.print("hello ");
  LoRa.print(counter);
  LoRa.endPacket();

  counter++;

  delay(2000);
}
