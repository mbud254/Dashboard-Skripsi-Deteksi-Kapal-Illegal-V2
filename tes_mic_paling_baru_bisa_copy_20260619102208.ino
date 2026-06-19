#include <driver/i2s.h>
#include "arduinoFFT.h" 
#include <WiFi.h>           
#include <HTTPClient.h>     

// ================= KONFIGURASI JARINGAN =================
const char* ssid = "POCO X7";
const char* password = "rrayrann"; 
const char* serverName = "http://10.237.210.175:3000/api/sensor"; 

// ================= KONFIGURASI PIN INMP441 =================
#define I2S_WS 15  
#define I2S_SD 32  
#define I2S_SCK 14 
#define I2S_PORT I2S_NUM_0

// ================= PARAMETER AUDIO =================
#define SAMPLE_RATE 8000  
#define SAMPLES 512       

// ================= VARIABEL FFT & EMA =================
double vReal[SAMPLES];
double vImag[SAMPLES];
arduinoFFT FFT = arduinoFFT(vReal, vImag, SAMPLES, SAMPLE_RATE); 

float EMA_a = 0.15; 
float EMA_SPL = 0; // Tren pergerakan SPL

// ================= VARIABEL AUTO-KALIBRASI =================
bool isCalibrated = false;
unsigned long calibrationStartTime = 0;
double sumCalibrationSPL = 0;
int calibrationCount = 0;
float THRESHOLD_IDLE = 0; // Angka ini bakal diisi otomatis sama ESP32!
const unsigned long CALIBRATION_TIME = 15000; // Waktu belajar: 15.000 ms (15 detik)

void setup() {
  Serial.begin(115200);
  Serial.println("Memulai Sistem Deteksi Kapal...");

  // KONEKSI WIFI
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Terhubung!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

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
}

void loop() {
  size_t bytesIn = 0;
  int32_t samples[SAMPLES];

  esp_err_t result = i2s_read(I2S_PORT, &samples, sizeof(samples), &bytesIn, portMAX_DELAY);

  if (result == ESP_OK && bytesIn > 0) {
    int samplesRead = bytesIn / sizeof(int32_t);

    // 1. CARI DC OFFSET (Biar sinyal mic bersih di tengah)
    double sumSamples = 0;
    for (int i = 0; i < samplesRead; i++) {
      sumSamples += (samples[i] >> 14);
    }
    double dc_offset = sumSamples / samplesRead;

    // 2. HITUNG RMS MURNI
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
    if (RMS_Value > 0) {
      SPL_dB = 20.0 * log10(RMS_Value) + 40.0; // 40 itu nilai kalibrasi dasar 
    }

    // =======================================================
    // 4. FASE AUTO-KALIBRASI (CUMA JALAN DI AWAL)
    // =======================================================
    if (!isCalibrated) {
      if (calibrationStartTime == 0) {
        calibrationStartTime = millis();
        Serial.println("=====================================");
        Serial.println("MEMULAI AUTO-KALIBRASI LINGKUNGAN...");
        Serial.println("Harap diamkan alat di dalam air selama 15 detik.");
      }

      sumCalibrationSPL += SPL_dB;
      calibrationCount++;
      
      Serial.print("Mempelajari noise sekitar... SPL Saat ini: "); 
      Serial.println(SPL_dB);

      // Cek apakah waktu belajar udah abis
      if (millis() - calibrationStartTime > CALIBRATION_TIME) {
        float rataRataNoise = sumCalibrationSPL / calibrationCount;
        // Set ambang batas: 5 dB di atas rata-rata noise laut/bak air
        THRESHOLD_IDLE = rataRataNoise + 5.0; 
        isCalibrated = true;
        
        Serial.println("=====================================");
        Serial.print("KALIBRASI SELESAI! Rata-rata Noise: "); Serial.println(rataRataNoise);
        Serial.print("Threshold Idle diset ke: "); Serial.println(THRESHOLD_IDLE);
        Serial.println("ALAT SIAP MENDETEKSI KAPAL!");
        Serial.println("=====================================");
      }
      
      delay(1000); 
      return; // Stop di sini, nge-loop dari atas lagi sampe kalibrasi kelar
    }

    // =======================================================
    // 5. MODE NORMAL (DETEKSI KAPAL SESUNGGUHNYA)
    // =======================================================
    float band_power = 0;

    // GERBANG 1: Apakah suara melebihi Threshold Idle hasil kalibrasi?
    if (SPL_dB > THRESHOLD_IDLE) {
      FFT.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD); 
      FFT.Compute(FFT_FORWARD);                        
      FFT.ComplexToMagnitude();                        

      for (int i = 0; i < (SAMPLES / 2); i++) {
        float frekuensi = (i * 1.0 * SAMPLE_RATE) / SAMPLES;
        // Rentang Propeller Kapal: 50 - 2000 Hz
        if (frekuensi >= 50.0 && frekuensi <= 2000.0) {
          band_power += vReal[i]; 
        }
      }
      band_power = band_power / (SAMPLES / 2); 
      
      // Bikin skala FFT jadi logaritmik biar gampang dibaca (0-100an)
      if (band_power > 0) {
        band_power = 10.0 * log10(band_power + 1); 
      }
    } else {
      band_power = 0; // Alat Idle, ga ngitung FFT
    }

    // GERBANG 3: HITUNG TREN (EMA)
    if (EMA_SPL == 0) {
      EMA_SPL = SPL_dB; 
    } else {
      EMA_SPL = (EMA_a * SPL_dB) + ((1 - EMA_a) * EMA_SPL);
    }

    Serial.print("SPL: "); Serial.print(SPL_dB);
    Serial.print(" | FFT Pwr: "); Serial.print(band_power);
    Serial.print(" | EMA: "); Serial.println(EMA_SPL);

    // KIRIM DATA KE DASHBOARD NEXT.JS
    if(WiFi.status() == WL_CONNECTED){
      HTTPClient http;
      http.begin(serverName); 
      http.addHeader("Content-Type", "application/json");

      String httpRequestData = "{\"spl\":" + String(SPL_dB) + ",\"fft\":" + String(band_power) + ",\"ema\":" + String(EMA_SPL) + "}";
      
      int httpResponseCode = http.POST(httpRequestData);
      http.end(); 
    }

    delay(1000); 
  }
}