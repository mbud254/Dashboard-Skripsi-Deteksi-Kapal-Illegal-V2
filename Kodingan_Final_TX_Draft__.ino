#include <driver/i2s.h>
#include "arduinoFFT.h" 
#include <TinyGPS++.h>
#include <SPI.h>
#include <LoRa.h>

// =====================================================
// KONFIGURASI GPS (Serial 2)
// =====================================================
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// =====================================================
// KONFIGURASI LORA
// =====================================================
#define SS_PIN   5
#define RST_PIN  14
#define DIO0_PIN 26

// =====================================================
// KONFIGURASI MIC INMP441 (I2S)
// =====================================================
#define I2S_WS 15  
#define I2S_SD 32  
#define I2S_SCK 14 
#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 8000  
#define SAMPLES 512       

double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLE_RATE); 

// =====================================================
// VARIABEL GLOBAL (Dibagikan antar Task)
// =====================================================
// Status Module
volatile bool gpsReady  = false;
volatile bool loraReady = false;
volatile bool gpsFix    = false;

// Data GPS
float latitude  = 0.0;
float longitude = 0.0;
int satellites = 0;
float hdopValue = 0.0;
uint32_t gpsChars = 0;

// Data Audio
volatile float sharedSPL = 0.0;
volatile float sharedFFT = 0.0;
volatile float sharedEMA = 0.0;

// Variabel Kalibrasi & EMA
float EMA_a = 0.15; 
float EMA_SPL = 0;
bool isCalibrated = false;
unsigned long calibrationStartTime = 0;
double sumCalibrationSPL = 0;
int calibrationCount = 0;
float THRESHOLD_IDLE = 0; 
const unsigned long CALIBRATION_TIME = 15000; // 15 detik kalibrasi

// =====================================================
// TASK 1: PROSES AUDIO MIC (Jalan di Core 0)
// =====================================================
void TaskMic(void *pvParameters) {
  Serial.println("[MIC] Task Audio Started di Core 0");

  for (;;) {
    size_t bytesIn = 0;
    int32_t samples[SAMPLES];
    esp_err_t result = i2s_read(I2S_PORT, &samples, sizeof(samples), &bytesIn, portMAX_DELAY);

    if (result == ESP_OK && bytesIn > 0) {
      int samplesRead = bytesIn / sizeof(int32_t);

      // 1. CARI DC OFFSET
      double sumSamples = 0;
      for (int i = 0; i < samplesRead; i++) {
        sumSamples += (samples[i] >> 14);
      }
      double dc_offset = sumSamples / samplesRead;

      // 2. HITUNG RMS
      double sumSquares = 0;
      for (int i = 0; i < samplesRead; i++) {
        double clean_sample = (samples[i] >> 14) - dc_offset; 
        vReal[i] = clean_sample;
        vImag[i] = 0.0;
        sumSquares += clean_sample * clean_sample;
      }
      float RMS_Value = sqrt(sumSquares / samplesRead);
      
      // 3. KONVERSI KE DESIBEL (dB)
      float SPL_dB = 0;
      if (RMS_Value > 0) SPL_dB = 20.0 * log10(RMS_Value) + 40.0; 

      // 4. AUTO-KALIBRASI
      if (!isCalibrated) {
        if (calibrationStartTime == 0) calibrationStartTime = millis();
        
        sumCalibrationSPL += SPL_dB;
        calibrationCount++;
        
        if (millis() - calibrationStartTime > CALIBRATION_TIME) {
          THRESHOLD_IDLE = (sumCalibrationSPL / calibrationCount) + 5.0; 
          isCalibrated = true;
          Serial.println("\n[MIC] KALIBRASI SELESAI! Threshold: " + String(THRESHOLD_IDLE) + " dB");
        }
        vTaskDelay(pdMS_TO_TICKS(500)); // Delay kalibrasi
        continue; // Ulang loop
      }

      // 5. MODE DETEKSI NORMAL (Gerbang 1)
      float band_power = 0;
      if (SPL_dB > THRESHOLD_IDLE) {
        FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD); 
        FFT.Compute(FFT_FORWARD);                        
        FFT.ComplexToMagnitude();                        

        for (int i = 0; i < (SAMPLES / 2); i++) {
          float frekuensi = (i * 1.0 * SAMPLE_RATE) / SAMPLES;
          if (frekuensi >= 50.0 && frekuensi <= 2000.0) {
            band_power += vReal[i]; 
          }
        }
        band_power = band_power / (SAMPLES / 2); 
        if (band_power > 0) band_power = 10.0 * log10(band_power + 1); 
      }

      // 6. HITUNG EMA
      if (EMA_SPL == 0) EMA_SPL = SPL_dB; 
      else EMA_SPL = (EMA_a * SPL_dB) + ((1 - EMA_a) * EMA_SPL);

      // 7. SIMPAN KE VARIABEL GLOBAL (Biar dikirim LoRa)
      sharedSPL = SPL_dB;
      sharedFFT = band_power;
      sharedEMA = EMA_SPL;

      vTaskDelay(pdMS_TO_TICKS(500)); // Istirahat bentar biar gak overheat
    }
  }
}

// =====================================================
// TASK 2: GPS (Jalan di Core 1)
// =====================================================
void TaskGPS(void *pvParameters) {
  Serial.println("[GPS] Task Started di Core 1");
  for (;;) {
    while (gpsSerial.available()) {
      gps.encode(gpsSerial.read());
    }
    gpsChars = gps.charsProcessed();
    
    if (gps.satellites.isValid()) satellites = gps.satellites.value();
    if (gps.hdop.isValid()) hdopValue = gps.hdop.hdop();
    
    if (gps.location.isValid()) {
      latitude = gps.location.lat();
      longitude = gps.location.lng();
      gpsFix = true;
    } else {
      gpsFix = false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// =====================================================
// TASK 3: LORA TX (Jalan di Core 1)
// =====================================================
void TaskLoRa(void *pvParameters) {
  Serial.println("[LORA] TX Task Started di Core 1");
  for (;;) {
    // LoRa ngirim kalau Mic udah kelar kalibrasi dan LoRa siap
    if (isCalibrated && loraReady) { 
      
      // Susun Payload Gabungan (GPS + Audio)
      String payload = "LAT=" + String(latitude, 6) +
                       ",LON=" + String(longitude, 6) +
                       ",SAT=" + String(satellites) +
                       ",SPL=" + String(sharedSPL, 1) +
                       ",FFT=" + String(sharedFFT, 1) +
                       ",EMA=" + String(sharedEMA, 1);

      LoRa.beginPacket();
      LoRa.print(payload);
      int result = LoRa.endPacket();

      Serial.println("\n========== LORA TX ==========");
      Serial.println(payload);
      Serial.println("=============================");

      vTaskDelay(pdMS_TO_TICKS(2000)); // Kirim setiap 2 detik
    } else {
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
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
    retry++;
    delay(1000);
    if (retry >= 10) return false;
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setTxPower(17);
  return true;
}

// =====================================================
// SETUP UTAMA
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=================================");
  Serial.println("   BUOY SYSTEM: MIC + GPS + LORA ");
  Serial.println("=================================");

  // 1. INIT GPS
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);
  gpsReady = true;

  // 2. INIT LORA
  loraReady = initLoRa();
  if(loraReady) Serial.println("[INIT] LoRa OK"); else Serial.println("[INIT] LoRa GAGAL");

  // 3. INIT I2S (MIC)
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = SAMPLES,
    .use_apll = false
  };
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_start(I2S_PORT);

  // 4. BIKIN MULTI-TASKING (RTOS)
  xTaskCreatePinnedToCore(TaskMic, "TaskMic", 10000, NULL, 1, NULL, 0);   // Core 0
  xTaskCreatePinnedToCore(TaskGPS, "TaskGPS", 4096, NULL, 1, NULL, 1);    // Core 1
  xTaskCreatePinnedToCore(TaskLoRa, "TaskLoRa", 4096, NULL, 2, NULL, 1);  // Core 1
}

void loop() {
  vTaskDelete(NULL); // Hapus default loop, serahkan semua ke RTOS
}