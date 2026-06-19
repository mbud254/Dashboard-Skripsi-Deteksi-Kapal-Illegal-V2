#include <SPI.h>
#include <LoRa.h>

// =====================================================
// LORA CONFIG
// =====================================================
#define SS_PIN   5
#define RST_PIN  14
#define DIO0_PIN 26

// =====================================================
// SHARED VARIABLES
// =====================================================
volatile bool loraReady = false;
volatile bool packetReceived = false;

String lastPacket = "";

float latitude = 0.0;
float longitude = 0.0;
int satellites = 0;

int packetRSSI = 0;
float packetSNR = 0.0;

unsigned long lastPacketTime = 0;
unsigned long packetCounter = 0;

// =====================================================
// PARSE PAYLOAD
// Format:
// LAT=-6.214194,LON=106.759166,SAT=7
// =====================================================
void parseGPSData(String data)
{
  int latIndex = data.indexOf("LAT=");
  int lonIndex = data.indexOf(",LON=");
  int satIndex = data.indexOf(",SAT=");

  if (latIndex == -1 || lonIndex == -1 || satIndex == -1)
    return;

  String latStr =
      data.substring(
          latIndex + 4,
          lonIndex);

  String lonStr =
      data.substring(
          lonIndex + 5,
          satIndex);

  String satStr =
      data.substring(
          satIndex + 5);

  latitude = latStr.toFloat();
  longitude = lonStr.toFloat();
  satellites = satStr.toInt();

  packetReceived = true;
}

// =====================================================
// LORA RX TASK
// =====================================================
void TaskLoRaRX(void *pvParameters)
{
  Serial.println("[LORA] RX Task Started");

  for (;;)
  {
    int packetSize = LoRa.parsePacket();

    if (packetSize)
    {
      String incoming = "";

      while (LoRa.available())
      {
        incoming += (char)LoRa.read();
      }

      packetRSSI = LoRa.packetRssi();
      packetSNR = LoRa.packetSnr();

      lastPacket = incoming;

      parseGPSData(incoming);

      packetCounter++;
      lastPacketTime = millis();

      Serial.println();
      Serial.println("========== RX ==========");
      Serial.println(incoming);

      Serial.print("RSSI : ");
      Serial.println(packetRSSI);

      Serial.print("SNR  : ");
      Serial.println(packetSNR);

      Serial.println("========================");
      Serial.println();
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =====================================================
// MONITOR TASK
// =====================================================
void TaskMonitor(void *pvParameters)
{
  for (;;)
  {
    Serial.println();
    Serial.println("========== STATUS ==========");

    Serial.print("LoRa Init     : ");
    Serial.println(loraReady ? "OK" : "FAIL");

    Serial.print("Packets RX    : ");
    Serial.println(packetCounter);

    if (packetReceived)
    {
      Serial.print("Latitude      : ");
      Serial.println(latitude, 6);

      Serial.print("Longitude     : ");
      Serial.println(longitude, 6);

      Serial.print("Satellites    : ");
      Serial.println(satellites);

      Serial.print("RSSI          : ");
      Serial.println(packetRSSI);

      Serial.print("SNR           : ");
      Serial.println(packetSNR);

      Serial.print("Last Packet   : ");
      Serial.println(lastPacket);
    }
    else
    {
      Serial.println("Waiting packet...");
    }

    if (packetReceived)
    {
      unsigned long age =
          (millis() - lastPacketTime) / 1000;

      Serial.print("Last Update   : ");
      Serial.print(age);
      Serial.println(" sec ago");
    }

    Serial.println("============================");
    Serial.println();

    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

// =====================================================
// LORA INIT
// =====================================================
bool initLoRa()
{
  Serial.println();
  Serial.println("[INIT] LoRa Start");

  pinMode(RST_PIN, OUTPUT);

  digitalWrite(RST_PIN, LOW);
  delay(20);

  digitalWrite(RST_PIN, HIGH);
  delay(100);

  LoRa.setPins(
      SS_PIN,
      RST_PIN,
      DIO0_PIN);

  int retry = 0;

  while (!LoRa.begin(433E6))
  {
    retry++;

    Serial.print("[INIT] LoRa init failed... Attempt ");
    Serial.println(retry);

    delay(1000);

    digitalWrite(RST_PIN, LOW);
    delay(20);

    digitalWrite(RST_PIN, HIGH);
    delay(100);

    if (retry >= 10)
    {
      Serial.println("[INIT] LoRa Gave Up");
      return false;
    }
  }

  LoRa.setSyncWord(0xF3);

  Serial.println("[INIT] LoRa OK");

  return true;
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("      LoRa GPS Receiver");
  Serial.println("=================================");

  loraReady = initLoRa();

  xTaskCreatePinnedToCore(
      TaskLoRaRX,
      "TaskLoRaRX",
      4096,
      NULL,
      2,
      NULL,
      0);

  xTaskCreatePinnedToCore(
      TaskMonitor,
      "TaskMonitor",
      4096,
      NULL,
      1,
      NULL,
      1);
}

// =====================================================
// LOOP
// =====================================================
void loop()
{
  vTaskDelete(NULL);
}
