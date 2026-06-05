#include <TinyGPS++.h>
#include <SPI.h>
#include <LoRa.h>

// ================= GPS =================
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// ================= LoRa =================
#define SS_PIN   5
#define RST_PIN  14
#define DIO0_PIN 26

// ================= Shared Variables =================
volatile bool gpsReady = false;
volatile bool loraReady = false;
volatile bool gpsFix = false;

float latitude = 0.0;
float longitude = 0.0;
int satellites = 0;

// ================= TASK GPS =================
void TaskGPS(void *pvParameters)
{
  Serial.println("[GPS] Task Started");

  for (;;)
  {
    while (gpsSerial.available())
    {
      gps.encode(gpsSerial.read());
    }

    if (gps.location.isValid())
    {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      satellites = gps.satellites.value();

      gpsFix = true;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ================= TASK LORA =================
void TaskLoRa(void *pvParameters)
{
  Serial.println("[LORA] Task Started");

  for (;;)
  {
    if (gpsReady && loraReady && gpsFix)
    {
      String payload =
          "LAT=" + String(latitude, 6) +
          ",LON=" + String(longitude, 6) +
          ",SAT=" + String(satellites);

      LoRa.beginPacket();
      LoRa.print(payload);
      LoRa.endPacket();

      Serial.println("[LORA TX]");
      Serial.println(payload);
      Serial.println();

      vTaskDelay(pdMS_TO_TICKS(5000));
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }
}

// ================= TASK MONITOR =================
void TaskMonitor(void *pvParameters)
{
  for (;;)
  {
    Serial.println("========== STATUS ==========");

    Serial.print("GPS Init : ");
    Serial.println(gpsReady ? "OK" : "FAIL");

    Serial.print("LoRa Init: ");
    Serial.println(loraReady ? "OK" : "FAIL");

    Serial.print("GPS Fix  : ");
    Serial.println(gpsFix ? "YES" : "NO");

    if (gpsFix)
    {
      Serial.print("LAT : ");
      Serial.println(latitude, 6);

      Serial.print("LON : ");
      Serial.println(longitude, 6);

      Serial.print("SAT : ");
      Serial.println(satellites);
    }

    Serial.println("============================");
    Serial.println();

    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("=================================");
  Serial.println(" GPS + LoRa RTOS Tracker ");
  Serial.println("=================================");

  // ================= GPS INIT =================

  gpsSerial.begin(
      GPS_BAUD,
      SERIAL_8N1,
      RXD2,
      TXD2);

  gpsReady = true;

  Serial.println("[INIT] GPS OK");

  // ================= LORA INIT =================

  SPI.begin(18, 19, 23, 5);

  LoRa.setPins(
      SS_PIN,
      RST_PIN,
      DIO0_PIN);

  if (LoRa.begin(433E6))
  {
    LoRa.setSyncWord(0xF3);
    LoRa.setTxPower(17);

    loraReady = true;

    Serial.println("[INIT] LoRa OK");
  }
  else
  {
    Serial.println("[INIT] LoRa FAILED");
  }

  // ================= TASKS =================

  xTaskCreatePinnedToCore(
      TaskGPS,
      "TaskGPS",
      4096,
      NULL,
      2,
      NULL,
      0);

  xTaskCreatePinnedToCore(
      TaskLoRa,
      "TaskLoRa",
      4096,
      NULL,
      1,
      NULL,
      1);

  xTaskCreatePinnedToCore(
      TaskMonitor,
      "TaskMonitor",
      4096,
      NULL,
      1,
      NULL,
      0);
}

void loop()
{
  vTaskDelete(NULL);
}
