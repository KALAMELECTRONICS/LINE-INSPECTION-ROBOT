/*
====================================================
 PROJECT TITLE : IOT-ENABLED AUTONOMOUS POWER
                 TRANSMISSION LINE INSPECTION ROBOT

 DEVELOPED BY  : KALAM ELECTRONICS
 DATE          : 11.10.2026
 PROJECT       : GRIDGUARD
 CONTROLLER    : ESP32
 PLATFORM      : BLYNK IoT

 COMPANY       : KALAM ELECTRONICS
====================================================
*/

#define BLYNK_TEMPLATE_ID "TMPL305lYAOUV"
#define BLYNK_TEMPLATE_NAME "LINE INSPECTION BOT"
#define BLYNK_AUTH_TOKEN "S7QgptkRLUHGMFsmWSjm4amxdz0AQCGe"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- WIFI ----------
char ssid[] = "Kalam_004";
char pass[] = "kalam@2026";

// ---------- L298N PINS ----------
const int ENA = 25;
const int IN1 = 26;
const int IN2 = 27;

const int ENB = 14;
const int IN3 = 18;
const int IN4 = 19;

// ---------- SENSOR PINS ----------
const int TRIG_PIN = 13;
const int ECHO_PIN = 34;
const int TEMP_PIN = 4;
const int BUZZER_PIN = 23;

// ---------- SENSORS ----------
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

BlynkTimer timer;

// ---------- SETTINGS ----------
const float OBSTACLE_LIMIT_CM = 25.0;
const float HIGH_TEMP_C = 70.0;
const unsigned long SENSOR_TIMEOUT_MS = 3000;

// ---------- LIVE DATA ----------
float distanceCM = -1.0;
float temperatureC = -127.0;

int motorSpeed = 180;

String motorStatus = "STOP";
String obstacleStatus = "UNKNOWN";

bool distanceValid = false;
bool temperatureValid = false;
bool obstacleDetected = false;
bool highTemperature = false;
bool safetyFault = true;

bool tempConversionPending = false;
unsigned long tempRequestTime = 0;
unsigned long lastDistanceTime = 0;
unsigned long lastValidTempTime = 0;
unsigned long lastStatusTime = 0;

bool previousObstacle = false;
bool previousHighTemp = false;
bool previousSensorFault = false;

// ---------- MOTOR CONTROL ----------

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  motorStatus = "STOP";
}

void applySpeed() {
  if (motorStatus == "FORWARD" ||
      motorStatus == "REVERSE") {
    analogWrite(ENA, motorSpeed);
    analogWrite(ENB, motorSpeed);
  }
}

bool safetyAllowsMovement(bool reverseDirection) {
  if (safetyFault || highTemperature) {
    Serial.println("[SAFETY] Movement blocked: sensor fault or temperature");
    return false;
  }

  // An obstacle blocks forward movement.
  // Reverse is allowed to retreat from an obstacle,
  // provided the sensors are valid and temperature is safe.
  if (!reverseDirection && obstacleDetected) {
    Serial.println("[SAFETY] Forward blocked: obstacle");
    return false;
  }

  return true;
}

void moveForward() {
  if (!safetyAllowsMovement(false)) {
    stopMotors();
    return;
  }

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  motorStatus = "FORWARD";
  applySpeed();

  Serial.println("[MOTOR] FORWARD");
}

void moveReverse() {
  if (!safetyAllowsMovement(true)) {
    stopMotors();
    return;
  }

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  motorStatus = "REVERSE";
  applySpeed();

  Serial.println("[MOTOR] REVERSE");
}

// ---------- BLYNK BUTTONS ----------

// Configure these buttons in PUSH mode.
BLYNK_WRITE(V4) {
  if (param.asInt() == 1) {
    Serial.println("[BLYNK COMMAND] FORWARD");
    moveForward();
  } else {
    Serial.println("[BLYNK COMMAND] FORWARD RELEASE");
    stopMotors();
  }
}

BLYNK_WRITE(V5) {
  if (param.asInt() == 1) {
    Serial.println("[BLYNK COMMAND] REVERSE");
    moveReverse();
  } else {
    Serial.println("[BLYNK COMMAND] REVERSE RELEASE");
    stopMotors();
  }
}

BLYNK_WRITE(V6) {
  if (param.asInt() == 1) {
    Serial.println("[BLYNK COMMAND] STOP");
    stopMotors();
  }
}

BLYNK_WRITE(V7) {
  motorSpeed = constrain(param.asInt(), 0, 255);

  Serial.print("[BLYNK COMMAND] SPEED = ");
  Serial.println(motorSpeed);

  applySpeed();

  if (motorSpeed == 0) {
    stopMotors();
  }
}

// ---------- DISTANCE ----------

void readDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Maximum echo wait: 25 ms
  unsigned long duration =
      pulseIn(ECHO_PIN, HIGH, 25000);

  if (duration == 0) {
    distanceValid = false;
    distanceCM = -1.0;
  } else {
    distanceCM = duration * 0.0343 / 2.0;
    distanceValid = true;
    lastDistanceTime = millis();
  }

  obstacleDetected =
      distanceValid && distanceCM <= OBSTACLE_LIMIT_CM;

  obstacleStatus =
      !distanceValid ? "UNKNOWN" :
      obstacleDetected ? "YES" : "NO";
}

// ---------- TEMPERATURE ----------

void updateTemperature() {
  unsigned long now = millis();

  if (!tempConversionPending &&
      now - tempRequestTime >= 1000) {

    tempSensor.requestTemperatures();
    tempConversionPending = true;
    tempRequestTime = now;
  }

  if (tempConversionPending &&
      now - tempRequestTime >= 750) {

    float t = tempSensor.getTempCByIndex(0);
    tempConversionPending = false;

    if (t != DEVICE_DISCONNECTED_C &&
        t >= -55.0 && t <= 125.0) {

      temperatureC = t;
      temperatureValid = true;
      lastValidTempTime = millis();

    } else {
      temperatureValid = false;
      temperatureC = -127.0;
    }
  }

  highTemperature =
      temperatureValid && temperatureC >= HIGH_TEMP_C;
}

// ---------- SAFETY ----------

void safetyCheck() {
  unsigned long now = millis();

  // Invalid or stale readings stop the robot.
  bool distanceFault =
      !distanceValid ||
      (now - lastDistanceTime > SENSOR_TIMEOUT_MS);

  bool tempFault =
      !temperatureValid ||
      (now - lastValidTempTime > SENSOR_TIMEOUT_MS);

  safetyFault = distanceFault || tempFault;

  if (safetyFault || highTemperature ||
      (!Blynk.connected())) {

    if (motorStatus != "STOP") {
      Serial.println("[SAFETY] STOP ACTIVATED");
    }

    stopMotors();
  }

  digitalWrite(BUZZER_PIN,
      (obstacleDetected || highTemperature || safetyFault)
      ? HIGH : LOW);

  // Alert only when the condition first appears.
  if (Blynk.connected()) {
    if (obstacleDetected && !previousObstacle) {
      Blynk.logEvent(
          "obstacle_detected",
          "GridGuard: obstacle detected"
      );
    }

    if (highTemperature && !previousHighTemp) {
      Blynk.logEvent(
          "high_temperature",
          "GridGuard: high temperature"
      );
    }

    if (safetyFault && !previousSensorFault) {
      Serial.println("[ALERT] Sensor fault");
    }
  }

  previousObstacle = obstacleDetected;
  previousHighTemp = highTemperature;
  previousSensorFault = safetyFault;
}

// ---------- DASHBOARD ----------

void updateDashboard() {
  if (!Blynk.connected()) return;

  Blynk.virtualWrite(V0,
      distanceValid ? distanceCM : 0);

  Blynk.virtualWrite(V1,
      temperatureValid ? temperatureC : 0);

  Blynk.virtualWrite(V2, motorStatus);
  Blynk.virtualWrite(V3, obstacleStatus);
}

// ---------- SERIAL MONITOR ----------

void printStatus() {
  Serial.println("\n========== GRIDGUARD ==========");

  Serial.print("Distance: ");
  if (distanceValid) {
    Serial.print(distanceCM, 1);
    Serial.println(" cm");
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Temperature: ");
  if (temperatureValid) {
    Serial.print(temperatureC, 1);
    Serial.println(" C");
  } else {
    Serial.println("INVALID");
  }

  Serial.print("Motor: ");
  Serial.println(motorStatus);

  Serial.print("Speed: ");
  Serial.println(motorSpeed);

  Serial.print("Obstacle: ");
  Serial.println(obstacleStatus);

  Serial.print("High temperature: ");
  Serial.println(highTemperature ? "YES" : "NO");

  Serial.print("Safety fault: ");
  Serial.println(safetyFault ? "YES" : "NO");

  Serial.print("Blynk: ");
  Serial.println(Blynk.connected() ? "CONNECTED" : "DISCONNECTED");

  Serial.println("===============================");
}

// ---------- BLYNK CONNECTION ----------

BLYNK_CONNECTED() {
  Serial.println("[BLYNK] Connected");

  // Never resume old movement after reconnection.
  stopMotors();
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  stopMotors();
  digitalWrite(BUZZER_PIN, LOW);

  tempSensor.begin();
  tempSensor.setWaitForConversion(false);

  // Start the first conversion.
  tempSensor.requestTemperatures();
  tempConversionPending = true;
  tempRequestTime = millis();

  Serial.println("\nGRIDGUARD PHASE-I");
  Serial.println("Main ESP32 controller starting...");

  WiFi.setSleep(false);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  timer.setInterval(200L, readDistance);
  timer.setInterval(100L, updateTemperature);
  timer.setInterval(100L, safetyCheck);
  timer.setInterval(1000L, updateDashboard);
  timer.setInterval(1000L, printStatus);
}

// ---------- LOOP ----------

void loop() {
  Blynk.run();
  timer.run();
}
