#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DHTPIN D3
#define DHTTYPE DHT11

const char* ssid = "A.T.L.A.S.";
const char* password = "Password";

// Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
Adafruit_BME280 bme;
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

// Global Variables
float t, h, p, airQuality;
int batteryPct;

// --- HTML UI (Stark Style) ---
const char INDEX_HTML[] PROGMEM = R"rawtext(
<!DOCTYPE html>
<html>
<head>
    <title>A.T.L.A.S.</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: 'Segoe UI', sans-serif; background: #121212; color: white; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
        .card { background: rgba(255, 255, 255, 0.05); backdrop-filter: blur(10px); border-radius: 20px; padding: 30px; border: 1px solid rgba(255, 255, 255, 0.1); width: 90%; max-width: 400px; text-align: center; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-top: 20px; }
        .stat-box { background: rgba(255, 255, 255, 0.03); padding: 20px; border-radius: 15px; }
        .temp { color: #ff7675; font-size: 2em; font-weight: bold; }
        .hum { color: #74b9ff; font-size: 2em; font-weight: bold; }
        .label { font-size: 0.8em; color: #888; margin-top: 5px; }
        .status-bar { margin-top: 20px; font-size: 0.9em; color: #55efc4; }
        .battery { font-size: 0.8em; color: #fab1a0; margin-top: 10px; }
    </style>
    <script>
        setInterval(() => { fetch('/data').then(r => r.json()).then(d => {
            document.getElementById('t').innerText = d.temp;
            document.getElementById('h').innerText = d.hum;
            document.getElementById('b').innerText = d.batt + '%';
            document.getElementById('a').innerText = d.air;
        });}, 2000);
    </script>
</head>
<body>
    <div class="card">
        <h2>A.T.L.A.S. Dashboard</h2>
        <div class="grid">
            <div class="stat-box"><div class="temp"><span id="t">--</span>°</div><div class="label">Temp</div></div>
            <div class="stat-box"><div class="hum"><span id="h">--</span>%</div><div class="label">Humidity</div></div>
        </div>
        <div class="status-bar">System Active: <span id="a">--</span> AQI</div>
        <div class="battery">Battery: <span id="b">--</span></div>
    </div>
</body>
</html>
)rawtext";

void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

void handleData() {
  String json = "{\"temp\":\"" + String(t, 1) + "\",\"hum\":\"" + String(h, 0) + 
                "\",\"air\":\"" + String(airQuality, 0) + "\",\"batt\":\"" + String(batteryPct) + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  
  // Sensors Init
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED Fail");
  if(!bme.begin(0x76)) Serial.println("BME280 Fail");
  dht.begin();

  // AP Mode
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0,0);
  display.print("AP: Stark_Home");
  display.display();
}

void loop() {
  server.handleClient();

  // Read Sensors
  t = bme.readTemperature();
  h = bme.readHumidity();
  airQuality = analogRead(A0); // MQ135 and Battery usually share A0 via a Mux or Switch
  
  // Battery Calculation (Assumes voltage divider on A0)
  // Max LiPo 4.2V -> Map to 100%
  int rawV = analogRead(A0); 
  batteryPct = map(rawV, 600, 1024, 0, 100); 
  batteryPct = constrain(batteryPct, 0, 100);

  // Update OLED
  display.clearDisplay();
  display.setCursor(0,10);
  display.printf("Temp: %.1f C", t);
  display.setCursor(0,30);
  display.printf("Hum: %.0f%%", h);
  display.display();

  delay(100); 
}