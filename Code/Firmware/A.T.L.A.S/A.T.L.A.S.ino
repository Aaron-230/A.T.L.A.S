#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Revised Pin Definitions ---
#define DHTPIN 2          // GPIO 2  (D4) - DHT Data
#define OLED_SDA 4        // GPIO 4  (D2) - OLED SDA
#define OLED_SCL 5        // GPIO 5  (D1) - OLED SCL
#define BMP_SDA 14        // GPIO 14 (D5) - BMP SDA
#define BMP_SCL 12        // GPIO 12 (D6) - BMP SCL
#define MQ135_PIN A0
#define DHTTYPE DHT11

Adafruit_SSD1306 display(128, 64, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BMP280 bmp; 
ESP8266WebServer server(80);

float t = 0, h = 0, p = 0;
int airQuality = 0;
unsigned long lastEyeAnimate = 0;
unsigned long lastGazeShift = 0;
unsigned long lastBlinkTrigger = 0;
int curEyeX = 0, curEyeY = 0, targetEyeX = 0, targetEyeY = 0;
int emotion = 0; 
bool isBlinking = false;

// --- Animation Engine ---
void drawEyes(int offsetX, int offsetY, bool blink, int state) {
  display.clearDisplay();
  int eyeW = 22; int eyeH = 28; int eyeR = 7;
  int eyeY = 18 + offsetY; 
  int leftX = 32 + offsetX; int rightX = 72 + offsetX;
  
  if (blink) {
    display.fillRoundRect(leftX, eyeY + 12, eyeW, 5, 2, WHITE);
    display.fillRoundRect(rightX, eyeY + 12, eyeW, 5, 2, WHITE);
  } else if (state == 1) { // HOT
    display.fillRoundRect(leftX, eyeY + 8, eyeW, 20, 5, WHITE);
    display.fillRoundRect(rightX, eyeY + 8, eyeW, 20, 5, WHITE);
    display.fillRect(leftX, eyeY - 5, eyeW, 13, BLACK);
    display.fillRect(rightX, eyeY - 5, eyeW, 13, BLACK);
  } else if (state == 2) { // ALERT
    display.fillRoundRect(leftX - 2, eyeY - 4, eyeW + 4, eyeH + 8, 11, WHITE);
    display.fillRoundRect(rightX - 2, eyeY - 4, eyeW + 4, eyeH + 8, 11, WHITE);
  } else if (state == 3) { // COLD
    int shiver = (millis() % 100 < 50) ? 1 : -1;
    display.fillRoundRect(leftX + shiver, eyeY + 10, eyeW, 10, 3, WHITE);
    display.fillRoundRect(rightX + shiver, eyeY + 10, eyeW, 10, 3, WHITE);
  } else { // NEUTRAL
    display.fillRoundRect(leftX, eyeY, eyeW, eyeH, eyeR, WHITE);
    display.fillRoundRect(rightX, eyeY, eyeW, eyeH, eyeR, WHITE);
  }
  display.display();
}

int moveTowards(int current, int target, int step) {
  if (current < target) return current + step;
  if (current > target) return current - step;
  return current;
}

// --- Web UI with Degree C ---
const char INDEX_HTML[] PROGMEM = R"rawtext(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #0f0f0f; color: white; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; }
    .container { background: #1a1a1a; padding: 30px; border-radius: 30px; width: 300px; text-align: center; box-shadow: 0 10px 30px rgba(0,0,0,0.5); }
    .card { background: #222; margin: 10px 0; padding: 20px; border-radius: 20px; }
    .val { font-size: 32px; font-weight: bold; display: block; }
    .label { font-size: 12px; color: #888; text-transform: uppercase; letter-spacing: 1px; }
  </style>
</head>
<body>
  <div class="container">
    <h2>A.T.L.A.S.</h2>
    <div class="card"><span class="val" id="t" style="color:#ff7675">--</span><span class="label">Temperature</span></div>
    <div class="card"><span class="val" id="h" style="color:#74b9ff">--</span><span class="label">Humidity</span></div>
    <div class="card"><span class="val" id="p" style="color:#a29bfe">--</span><span class="label">Pressure</span></div>
    <div class="card"><span class="val" id="a" style="color:#ffeaa7">--</span><span class="label">Air Quality</span></div>
  </div>
  <script>
    setInterval(() => {
      fetch('/data').then(r => r.json()).then(d => {
        document.getElementById('t').innerHTML = d.t + "&deg;C";
        document.getElementById('h').innerText = d.h + "%";
        document.getElementById('p').innerText = d.p + " hPa";
        document.getElementById('a').innerText = d.a + " PPM";
      });
    }, 2000);
  </script>
</body>
</html>
)rawtext";

void handleRoot() { server.send(200, "text/html", INDEX_HTML); }
void handleData() {
  String json = "{\"t\":\"" + String(t, 1) + "\",\"h\":\"" + String(h, 0) + 
                "\",\"p\":\"" + String(p, 1) + "\",\"a\":\"" + String(airQuality) + "\"}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Initialize OLED (I2C Bus 1)
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  // Initialize BMP280 (I2C Bus 2)
  Wire.begin(BMP_SDA, BMP_SCL);
  if (!bmp.begin(0x76)) { bmp.begin(0x77); }

  WiFi.softAP("A.T.L.A.S.", "Password");
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();
  unsigned long currentMillis = millis();

  // Read DHT & MQ135
  float newT = dht.readTemperature();
  float newH = dht.readHumidity();
  if (!isnan(newT)) t = newT;
  if (!isnan(newH)) h = newH;
  airQuality = analogRead(MQ135_PIN);

  // BMP280 Bus Switch
  Wire.begin(BMP_SDA, BMP_SCL);
  p = bmp.readPressure() / 100.0F;
  
  // Emotion Updates
  if (airQuality > 550) emotion = 2;
  else if (t > 32) emotion = 1;
  else if (t < 18) emotion = 3;
  else emotion = 0;

  // OLED Animation Bus Switch
  if (currentMillis - lastEyeAnimate >= 35) {
    lastEyeAnimate = currentMillis;
    if (currentMillis - lastGazeShift >= random(3000, 7000)) {
      lastGazeShift = currentMillis;
      targetEyeX = random(-6, 7);
      targetEyeY = random(-4, 5);
    }
    if (currentMillis - lastBlinkTrigger >= random(4000, 10000)) {
      lastBlinkTrigger = currentMillis;
      isBlinking = true;
    }
    if (isBlinking && (currentMillis - lastBlinkTrigger > 120)) isBlinking = false;

    curEyeX = moveTowards(curEyeX, targetEyeX, 1);
    curEyeY = moveTowards(curEyeY, targetEyeY, 1);

    Wire.begin(OLED_SDA, OLED_SCL);
    drawEyes(curEyeX, curEyeY, isBlinking, emotion);
  }
}