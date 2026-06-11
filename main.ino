#include <TinyGPS++.h>
#include <SPI.h>
#include <LoRa.h>

// =====================================================
// GPS CONFIG
// =====================================================
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// =====================================================
// LORA CONFIG
// =====================================================
#define SS_PIN   5
#define RST_PIN  14
#define DIO0_PIN 26

// =====================================================
// SHARED VARIABLES
// =====================================================
volatile bool gpsReady  = false;
volatile bool loraReady = false;
volatile bool gpsFix    = false;

float latitude  = 0.0;
float longitude = 0.0;

int satellites = 0;
float hdopValue = 0.0;
uint32_t gpsChars = 0;

// =====================================================
// GPS TASK
// =====================================================
void TaskGPS(void *pvParameters)
{
  Serial.println("[GPS] Task Started");

  for (;;)
  {
    while (gpsSerial.available())
    {
      char c = gpsSerial.read();

      gps.encode(c);

      // tampilkan raw NMEA sebelum fix
      if (!gpsFix)
      {
        Serial.write(c);
      }
    }

    gpsChars = gps.charsProcessed();

    if (gps.satellites.isValid())
      satellites = gps.satellites.value();

    if (gps.hdop.isValid())
      hdopValue = gps.hdop.hdop();

    if (gps.location.isValid())
    {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      gpsFix = true;
    }
    else
    {
      gpsFix = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// =====================================================
// LORA TX TASK
// =====================================================
void TaskLoRa(void *pvParameters)
{
  Serial.println("[LORA] Task Started");

  for (;;)
  {
    if (gpsFix && loraReady)
    {
      String payload =
          "LAT=" + String(latitude, 6) +
          ",LON=" + String(longitude, 6) +
          ",SAT=" + String(satellites);

      LoRa.beginPacket();
      LoRa.print(payload);

      int result = LoRa.endPacket();

      Serial.println();
      Serial.println("========== LORA TX ==========");
      Serial.println(payload);

      Serial.print("TX Result : ");
      Serial.println(result);

      Serial.println("=============================");
      Serial.println();

      vTaskDelay(pdMS_TO_TICKS(5000));
    }
    else
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
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

    Serial.print("GPS Init   : ");
    Serial.println(gpsReady ? "OK" : "FAIL");

    Serial.print("LoRa Init  : ");
    Serial.println(loraReady ? "OK" : "FAIL");

    Serial.print("GPS Fix    : ");
    Serial.println(gpsFix ? "YES" : "NO");

    Serial.print("GPS Chars  : ");
    Serial.println(gpsChars);

    Serial.print("Satellites : ");
    Serial.println(satellites);

    Serial.print("HDOP       : ");
    Serial.println(hdopValue);

    if (gpsFix)
    {
      Serial.print("Latitude   : ");
      Serial.println(latitude, 6);

      Serial.print("Longitude  : ");
      Serial.println(longitude, 6);
    }
    else
    {
      Serial.println("Latitude   : ---");
      Serial.println("Longitude  : ---");
    }

    if (millis() > 15000 && gpsChars < 10)
    {
      Serial.println();
      Serial.println("[ERROR] GPS NOT DETECTED!");
    }

    Serial.println("============================");
    Serial.println();

    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

// =====================================================
// LORA INIT FUNCTION
// =====================================================
bool initLoRa()
{
  Serial.println();
  Serial.println("[INIT] LoRa Start");

  // Hardware Reset SX1278
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
  LoRa.setTxPower(17);

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
  Serial.println(" GPS + LoRa RTOS Tracker ");
  Serial.println("=================================");

  // =================================================
  // GPS INIT
  // =================================================

  gpsSerial.begin(
      GPS_BAUD,
      SERIAL_8N1,
      RXD2,
      TXD2);

  gpsReady = true;

  Serial.println("[INIT] GPS OK");

  // beri waktu GPS stabil
  delay(1000);

  // =================================================
  // LORA INIT
  // =================================================

  loraReady = initLoRa();

  // =================================================
  // CREATE TASKS
  // =================================================

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

// =====================================================
// LOOP
// =====================================================
void loop()
{
  vTaskDelete(NULL);
}
