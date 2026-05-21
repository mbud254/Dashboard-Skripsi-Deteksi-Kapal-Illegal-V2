#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPS++.h>

// ================= WIFI =================
const char* ssid = "vivo Y33S";
const char* password = "arduinouno";

// ================= GPS ==================
#define RXD2 16
#define TXD2 17
#define GPS_BAUD 9600

TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

// ================= SERVER ===============
WebServer server(80);

// Variabel GPS
String latitude = "0";
String longitude = "0";
String satellites = "0";

// ========================================
void handleRoot() {

  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<title>ESP32 GPS</title>";

  html += "<style>";
  html += "body{font-family:Arial;text-align:center;margin-top:40px;background:#f2f2f2;}";
  html += ".card{background:white;padding:20px;border-radius:10px;width:300px;margin:auto;box-shadow:0 0 10px gray;}";
  html += "h1{color:#333;}";
  html += "p{font-size:20px;}";
  html += "a{font-size:18px;color:blue;}";
  html += "</style>";

  html += "</head>";
  html += "<body>";

  html += "<div class='card'>";
  html += "<h1>ESP32 GPS</h1>";

  html += "<p><b>Latitude:</b><br>" + latitude + "</p>";
  html += "<p><b>Longitude:</b><br>" + longitude + "</p>";
  html += "<p><b>Satellites:</b><br>" + satellites + "</p>";

  html += "<a href='https://maps.google.com/?q=" + latitude + "," + longitude + "' target='_blank'>";
  html += "Open in Google Maps";
  html += "</a>";

  html += "</div>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

// ========================================
void setup() {

  Serial.begin(115200);

  // GPS Serial
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, RXD2, TXD2);

  // WiFi
  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Web Server
  server.on("/", handleRoot);

  server.begin();

  Serial.println("Web Server Started");
}

// ========================================
void loop() {

  // Handle web client
  server.handleClient();

  // Read GPS data
  while (gpsSerial.available()) {

    char c = gpsSerial.read();

    gps.encode(c);
  }

  // Update GPS data
  if (gps.location.isUpdated()) {

    latitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
    satellites = String(gps.satellites.value());

    Serial.println("===== GPS DATA =====");
    Serial.print("LAT: ");
    Serial.println(latitude);

    Serial.print("LON: ");
    Serial.println(longitude);

    Serial.print("SAT: ");
    Serial.println(satellites);

    Serial.println("====================");
  }
}
