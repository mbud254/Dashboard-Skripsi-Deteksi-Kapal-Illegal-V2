#include <TinyGPS++.h>

#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

void setup() {

  Serial.begin(115200);
  delay(1000);

  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);

  Serial.println("GPS Serial Started");
}

void loop() {

  Serial.println("Loop berjalan");

  while (gpsSerial.available()) {

    char c = gpsSerial.read();

    Serial.write(c); // tampilkan raw data GPS

    gps.encode(c);
  }

  if (gps.location.isUpdated()) {

    Serial.print("LAT: ");
    Serial.println(gps.location.lat(), 6);

    Serial.print("LON: ");
    Serial.println(gps.location.lng(), 6);

    Serial.print("SAT: ");
    Serial.println(gps.satellites.value());

    Serial.println("----------------");
  }

  delay(1000);
}
