#include <driver/i2s.h>
#include "arduinoFFT.h"

// =====================================================
// KONFIGURASI PIN INMP441
// =====================================================
#define I2S_WS   15   // Pin LRCL / WS
#define I2S_SD   32   // Pin DOUT / SD
#define I2S_SCK  14   // Pin BCLK / SCK
#define I2S_PORT I2S_NUM_0

// =====================================================
// PARAMETER AUDIO & NYQUIST
// =====================================================
#define SAMPLE_RATE 8000   // Fs = 8000 Hz
#define SAMPLES     512    // Harus 2^n (256, 512, 1024, dst)

// =====================================================
// VARIABEL FFT
// =====================================================
double vReal[SAMPLES];
double vImag[SAMPLES];

// Inisialisasi objek FFT
arduinoFFT FFT(vReal, vImag, SAMPLES, SAMPLE_RATE);

// =====================================================
// VARIABEL EMA
// =====================================================
float EMA_a = 0.15;      // Alpha EMA (0.1 - 0.3)
float EMA_BandPower = 0;

void setup()
{
    Serial.begin(115200);
    Serial.println("Memulai Sistem Deteksi Kapal...");

    // =================================================
    // KONFIGURASI I2S UNTUK INMP441
    // =================================================
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = i2s_bits_per_sample_t(32),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format =
            i2s_comm_format_t(I2S_COMM_FORMAT_I2S |
                              I2S_COMM_FORMAT_I2S_MSB),
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

    Serial.println("I2S INMP441 Siap! Buka Serial Plotter (115200 baud).");
}

void loop()
{
    int32_t raw_samples[SAMPLES];
    size_t bytes_read;

    // =================================================
    // 1. BACA DATA AUDIO DARI INMP441
    // =================================================
    i2s_read(
        I2S_PORT,
        &raw_samples,
        sizeof(raw_samples),
        &bytes_read,
        portMAX_DELAY
    );

    int samples_read = bytes_read / sizeof(int32_t);

    if (samples_read == SAMPLES)
    {
        float sum_squares = 0;

        // =============================================
        // 2. SIAPKAN DATA UNTUK RMS DAN FFT
        // =============================================
        for (int i = 0; i < SAMPLES; i++)
        {
            // Konversi data I2S 32-bit
            float sample = (float)(raw_samples[i] >> 14);

            // Untuk RMS
            sum_squares += (sample * sample);

            // Untuk FFT
            vReal[i] = sample;
            vImag[i] = 0.0;
        }

        // =============================================
        // 3. HITUNG RMS
        // =============================================
        float rms = sqrt(sum_squares / SAMPLES);

        // =============================================
        // 4. PROSES FFT
        // =============================================
        FFT.Windowing(
            FFT_WIN_TYP_HAMMING,
            FFT_FORWARD
        );

        FFT.Compute(FFT_FORWARD);

        FFT.ComplexToMagnitude();

        // =============================================
        // 5. HITUNG BAND POWER (50 - 500 Hz)
        // =============================================
        float band_power = 0;

        // Resolusi bin:
        // SAMPLE_RATE / SAMPLES
        // 8000 / 512 = 15.625 Hz/bin

        for (int i = 0; i < (SAMPLES / 2); i++)
        {
            float frekuensi =
                (i * 1.0 * SAMPLE_RATE) / SAMPLES;

            // Fokus pada frekuensi kapal
            if (frekuensi >= 50.0 &&
                frekuensi <= 500.0)
            {
                band_power += vReal[i];
            }
        }

        // Normalisasi
        band_power = band_power / SAMPLES;

        // =============================================
        // 6. EMA (EXPONENTIAL MOVING AVERAGE)
        // =============================================
        if (EMA_BandPower == 0)
        {
            EMA_BandPower = band_power;
        }
        else
        {
            EMA_BandPower =
                (EMA_a * band_power) +
                ((1 - EMA_a) * EMA_BandPower);
        }

        // =============================================
        // 7. TAMPILKAN KE SERIAL PLOTTER
        // =============================================
        Serial.print("RMS_Total:");
        Serial.print(rms);

        Serial.print("\t");

        Serial.print("Power_50_500Hz:");
        Serial.print(band_power);

        Serial.print("\t");

        Serial.print("EMA_Kapal:");
        Serial.println(EMA_BandPower);
    }
}
