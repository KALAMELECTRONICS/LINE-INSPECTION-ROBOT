
/*
====================================================
                  GRIDGUARD
       LIVE CAMERA MONITORING SYSTEM

 Project Title : IoT-Enabled Autonomous Power
                 Transmission Line Inspection Robot

 Developed By  : KALAM ELECTRONICS
 DATE          : 11.10.2026
 Controller    : ESP32-CAM AI-Thinker
 Connectivity  : Wi-Fi Access Point
 Web Interface : Live Camera Streaming

 Copyright (c) 2026 KALAM ELECTRONICS
 All Rights Reserved.
====================================================
*/

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "esp_http_server.h"

// =============== WIFI ACCESS POINT ===============
const char* AP_SSID = "GRIDGUARD_CAM";
const char* AP_PASSWORD = "12345678";

IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// =============== AI THINKER ESP32-CAM PINS ===============
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WebServer server(80);
httpd_handle_t stream_httpd = NULL;

// =============== WEBPAGE ===============
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>IOT-ENABLED AUTONOMOUS POWER TRANSMISSION LINE INSPECTION ROBOT</title>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  padding: 25px 15px;
  background: #ffffff;
  color: #17365d;
  font-family: "Times New Roman", serif;
  text-align: center;
}

h1 {
  color: #0759b5;
  font-size: clamp(25px, 5vw, 38px);
  margin: 12px 0 28px;
  animation: titleIn 1s ease;
  text-shadow: 0 3px 10px rgba(0, 75, 180, 0.15);
}

.camera-card {
  max-width: 850px;
  margin: auto;
  padding: 18px;
  background: #ffffff;
  border: 1px solid #cfe2ff;
  border-radius: 14px;
  box-shadow: 0 8px 25px rgba(0, 65, 130, 0.12);
  animation: cardIn 0.8s ease;
}

.camera-header {
  font-size: 21px;
  color: #0759b5;
  margin-bottom: 15px;
}

.status {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 15px;
  color: #34495e;
  font-size: 17px;
}

.dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: #f0a020;
  animation: pulse 1.5s infinite;
}

.camera-frame {
  width: 100%;
  min-height: 220px;
  background: #101820;
  border-radius: 10px;
  overflow: hidden;
  display: flex;
  justify-content: center;
  align-items: center;
}

#camera {
  display: block;
  width: 100%;
  height: auto;
  min-height: 220px;
  object-fit: contain;

  /* Correct opposite mounting direction */
  transform: rotate(180deg);
}

@keyframes titleIn {
  from {
    opacity: 0;
    transform: translateY(-12px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

@keyframes cardIn {
  from {
    opacity: 0;
    transform: translateY(12px);
  }
  to {
    opacity: 1;
    transform: translateY(0);
  }
}

@keyframes pulse {
  0%, 100% { opacity: 1; }
  50% { opacity: 0.35; }
}
</style>
</head>

<body>

<h1>IOT-ENABLED AUTONOMOUS POWER TRANSMISSION LINE INSPECTION ROBOT</h1>

<div class="camera-card">
  <div class="camera-header">Live Camera Monitoring</div>

  <div class="status">
    <span class="dot" id="statusDot"></span>
    <span id="statusText">Connecting to camera...</span>
  </div>

  <div class="camera-frame">
    <img id="camera" alt="ESP32-CAM live stream">
  </div>
</div>

<script>
const camera = document.getElementById("camera");
const statusText = document.getElementById("statusText");
const statusDot = document.getElementById("statusDot");

let connected = false;

// Stream server runs on port 81
camera.src = "http://" + location.hostname + ":81/stream";

camera.onload = function() {
  connected = true;
  statusText.textContent = "Camera stream active";
  statusDot.style.background = "#16a34a";
};

camera.onerror = function() {
  connected = false;
  statusText.textContent = "Stream unavailable - check camera";
  statusDot.style.background = "#dc2626";
};

// MJPEG streams may not trigger onload until the stream ends.
// Update the status after allowing the stream to start.
setTimeout(function() {
  if (!connected) {
    statusText.textContent = "Camera stream starting...";
  }
}, 2500);
</script>

</body>
</html>
)rawliteral";

// =============== CAMERA STREAM HANDLER ===============
static esp_err_t stream_handler(httpd_req_t *req) {
  esp_err_t res = ESP_OK;
  camera_fb_t *fb = NULL;
  char part_buf[96];

  res = httpd_resp_set_type(
    req, "multipart/x-mixed-replace;boundary=frame"
  );

  if (res != ESP_OK) {
    return res;
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  while (true) {
    fb = esp_camera_fb_get();

    if (!fb) {
      Serial.println("Camera capture failed");
      return ESP_FAIL;
    }

    size_t header_len = snprintf(
      part_buf,
      sizeof(part_buf),
      "--frame\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Length: %u\r\n\r\n",
      (unsigned int)fb->len
    );

    res = httpd_resp_send_chunk(req, part_buf, header_len);

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(
        req,
        (const char *)fb->buf,
        fb->len
      );
    }

    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, "\r\n", 2);
    }

    esp_camera_fb_return(fb);
    fb = NULL;

    if (res != ESP_OK) {
      break;
    }

    vTaskDelay(1);
  }

  if (fb) {
    esp_camera_fb_return(fb);
  }

  return res;
}

// =============== START STREAM SERVER ===============
void startCameraStream() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  config.server_port = 81;
  config.ctrl_port = 32769;
  config.max_uri_handlers = 4;

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };

  esp_err_t result = httpd_start(&stream_httpd, &config);

  if (result == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    Serial.println("Camera stream server started on port 81");
  } else {
    Serial.printf("Stream server failed: %d\n", result);
  }
}

// =============== WEBPAGE HANDLER ===============
void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

// =============== SETUP ===============
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Starting GRIDGUARD ESP32-CAM...");

  // Camera configuration
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  if (psramFound()) {
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } else {
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  // Initialize camera
  esp_err_t cameraResult = esp_camera_init(&config);

  if (cameraResult != ESP_OK) {
    Serial.printf(
      "Camera initialization failed: 0x%x\n",
      cameraResult
    );
    return;
  }

  Serial.println("Camera initialized successfully");

  // Start Wi-Fi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  WiFi.softAPConfig(local_IP, gateway, subnet);

  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Wi-Fi AP startup failed");
    return;
  }

  Serial.println("Wi-Fi AP started");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);

  Serial.print("Website: http://");
  Serial.println(WiFi.softAPIP());

  // Start website server on port 80
  server.on("/", HTTP_GET, handleRoot);
  server.begin();

  Serial.println("Website server started on port 80");

  // Start live stream on port 81
  startCameraStream();
}

// =============== MAIN LOOP ===============
void loop() {
  server.handleClient();
  delay(2);
}
