
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// =====================================
// SMART DESK ROBOT - CAMERA UNIT
// ESP32-S3 N16R8 + OV2640
// =====================================

// Wi-Fi hotspot
const char* AP_SSID = "SMART_DESK_ROBOT";
const char* AP_PASSWORD = "DeskRobot8266";

// Candidate camera pin mapping
// Must be verified against the exact PCB.
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    15
#define CAM_PIN_SIOD    4
#define CAM_PIN_SIOC    5

#define CAM_PIN_D7      16
#define CAM_PIN_D6      17
#define CAM_PIN_D5      18
#define CAM_PIN_D4      12
#define CAM_PIN_D3      10
#define CAM_PIN_D2      8
#define CAM_PIN_D1      9
#define CAM_PIN_D0      11

#define CAM_PIN_VSYNC   6
#define CAM_PIN_HREF    7
#define CAM_PIN_PCLK    13

WebServer server(80);

bool cameraReady = false;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Desk Robot</title>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  padding: 20px;
  background: #10131b;
  color: #f1f5f9;
  font-family: Arial, sans-serif;
}

.container {
  max-width: 850px;
  margin: auto;
}

.header {
  margin-bottom: 20px;
}

.header h1 {
  margin-bottom: 5px;
}

.status {
  color: #4ade80;
  font-size: 14px;
}

.camera {
  width: 100%;
  background: #202633;
  border-radius: 14px;
  overflow: hidden;
  border: 1px solid #374151;
}

.camera img {
  display: block;
  width: 100%;
  min-height: 220px;
  object-fit: contain;
}

.controls {
  display: flex;
  gap: 10px;
  margin-top: 15px;
}

button {
  flex: 1;
  padding: 14px;
  border: 0;
  border-radius: 9px;
  background: #2563eb;
  color: white;
  font-size: 15px;
  cursor: pointer;
}

button:active {
  opacity: .7;
}

.info {
  margin-top: 20px;
  padding: 16px;
  background: #1e293b;
  border-radius: 10px;
  font-size: 14px;
  line-height: 1.8;
}
</style>
</head>

<body>
<div class="container">

  <div class="header">
    <h1>SMART DESK ROBOT</h1>
    <div class="status">● CAMERA DASHBOARD</div>
  </div>

  <div class="camera">
    <img id="camera" alt="Waiting for camera...">
  </div>

  <div class="controls">
    <button onclick="refreshCamera()">
      Refresh Preview
    </button>

    <button onclick="takeSnapshot()">
      Take Snapshot
    </button>
  </div>

  <div class="info">
    <b>Robot Status</b><br>
    Camera: <span id="cameraStatus">Checking...</span><br>
    Connection: Wi-Fi Hotspot<br>
    Address: 192.168.4.1<br>
    AI Mode: Offline architecture
  </div>

</div>

<script>
const camera = document.getElementById("camera");

function refreshCamera() {
  camera.src = "/capture?t=" + Date.now();
}

function takeSnapshot() {
  const link = document.createElement("a");
  link.href = "/capture?t=" + Date.now();
  link.download = "robot_snapshot.jpg";
  link.click();
}

camera.onload = () => {
  document.getElementById("cameraStatus").textContent =
    "Connected";
};

camera.onerror = () => {
  document.getElementById("cameraStatus").textContent =
    "Waiting for image";
};

refreshCamera();
setInterval(refreshCamera, 700);
</script>

</body>
</html>
)rawliteral";

// =====================================
// CAMERA INITIALIZATION
// =====================================

bool initializeCamera() {

  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;

  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;

  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;

  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;

  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  Serial.println("Initializing OV2640...");

  esp_err_t result = esp_camera_init(&config);

  if (result != ESP_OK) {
    Serial.printf(
      "Camera initialization failed: 0x%x\n",
      result
    );

    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();

  if (sensor != nullptr) {
    sensor->set_brightness(sensor, 0);
    sensor->set_contrast(sensor, 0);
    sensor->set_saturation(sensor, 0);
  }

  Serial.println("OV2640 initialized successfully!");

  return true;
}

// =====================================
// WEB DASHBOARD
// =====================================

void handleRoot() {
  server.send_P(
    200,
    "text/html",
    INDEX_HTML
  );
}

void handleCapture() {

  if (!cameraReady) {
    server.send(
      503,
      "text/plain",
      "Camera unavailable"
    );
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    server.send(
      500,
      "text/plain",
      "Camera capture failed"
    );
    return;
  }

  server.sendHeader(
    "Cache-Control",
    "no-store, no-cache, must-revalidate"
  );

  server.sendHeader(
    "Access-Control-Allow-Origin",
    "*"
  );

  server.send_P(
    200,
    "image/jpeg",
    fb->buf,
    fb->len
  );

  esp_camera_fb_return(fb);
}

void handleStatus() {

  String json = "{";
  json += "\"camera\":";
  json += cameraReady ? "\"ready\"" : "\"error\"";
  json += ",\"psram\":";
  json += psramFound() ? "true" : "false";
  json += "}";

  server.send(
    200,
    "application/json",
    json
  );
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);
  delay(1500);

  Serial.println();
  Serial.println("==============================");
  Serial.println(" SMART DESK ROBOT");
  Serial.println(" ESP32-S3 CAMERA UNIT");
  Serial.println("==============================");

  Serial.printf(
    "PSRAM detected: %s\n",
    psramFound() ? "YES" : "NO"
  );

  Serial.printf(
    "PSRAM size: %u bytes\n",
    ESP.getPsramSize()
  );

  cameraReady = initializeCamera();

  Serial.println();
  Serial.println("Starting Wi-Fi hotspot...");

  WiFi.mode(WIFI_AP);

  bool apStarted = WiFi.softAP(
    AP_SSID,
    AP_PASSWORD
  );

  if (apStarted) {

    Serial.println("Hotspot started!");

    Serial.print("SSID: ");
    Serial.println(AP_SSID);

    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);

    Serial.print("Dashboard: http://");
    Serial.println(WiFi.softAPIP());

  } else {

    Serial.println("Hotspot startup failed!");

  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();

  Serial.println("Web server started!");
  Serial.println("==============================");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {
  server.handleClient();
  delay(2);
}
