#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

// --- KONFIGURASI JARINGAN & API ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
// Nanti diganti URL ngrok saat tes ke backend lokal
const char* serverUrl = "https://your-tunnel-url.ngrok-free.app/api/ingest";
const char* machineId = "MC-01";
const char* deviceKey = "key-mc01-secret-2026";

// --- KONFIGURASI PIN ---
#define PIN_COUNT   19
#define PIN_REJECT  21
#define PIN_FAULT   22
#define PIN_DHT     4
#define PIN_LED_G   25
#define PIN_LED_Y   26
#define PIN_LED_R   27
#define PIN_BUZZER  13

#define DHTTYPE DHT22
DHT dht(PIN_DHT, DHTTYPE);

// --- VARIABEL STATE & TELEMETRI ---
String bootId = "";
unsigned long seqNumber = 1;

String currentState = "IDLE";
String previousSentState = "";
String currentReason = "";

bool faultButtonActive = false;
bool isOverheat = false;
float lastValidTemp = NAN;
bool sensorOk = false;

unsigned long outputDelta = 0;
unsigned long rejectDelta = 0;
unsigned long lastCountTimestamp = 0;

// --- VARIABEL DEBOUNCE TOMBOL ---
const unsigned long DEBOUNCE_MS = 50;
bool lastBtnCount = HIGH, stableBtnCount = HIGH;
unsigned long debounceCountTime = 0;

bool lastBtnReject = HIGH, stableBtnReject = HIGH;
unsigned long debounceRejectTime = 0;

bool lastBtnFault = HIGH, stableBtnFault = HIGH;
unsigned long debounceFaultTime = 0;

// --- TIMER NON-BLOCKING ---
unsigned long lastDhtReadTime = 0;
unsigned long lastTelemetrySendTime = 0;
unsigned long lastWifiCheckTime = 0;
unsigned long lastBuzzerToggleTime = 0;
bool buzzerState = false;

String generateBootId() {
  const char charset[] = "0123456789abcdef";
  String res = "";
  for (int i = 0; i < 8; i++) {
    res += charset[random(0, 16)];
  }
  return res;
}

void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());
  bootId = generateBootId();

  pinMode(PIN_COUNT, INPUT_PULLUP);
  pinMode(PIN_REJECT, INPUT_PULLUP);
  pinMode(PIN_FAULT, INPUT_PULLUP);

  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_Y, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Boot ID: " + bootId);
}

void handleButtons() {
  unsigned long now = millis();

  // 1. Tombol COUNT (Active LOW)
  bool rawCount = digitalRead(PIN_COUNT);
  if (rawCount != lastBtnCount) debounceCountTime = now;
  if ((now - debounceCountTime) > DEBOUNCE_MS) {
    if (rawCount != stableBtnCount) {
      stableBtnCount = rawCount;
      if (stableBtnCount == LOW) {
        outputDelta++;
        lastCountTimestamp = now;
        Serial.println("[BTN] COUNT +1 -> Delta: " + String(outputDelta));
      }
    }
  }
  lastBtnCount = rawCount;

  // 2. Tombol REJECT (Active LOW)
  bool rawReject = digitalRead(PIN_REJECT);
  if (rawReject != lastBtnReject) debounceRejectTime = now;
  if ((now - debounceRejectTime) > DEBOUNCE_MS) {
    if (rawReject != stableBtnReject) {
      stableBtnReject = rawReject;
      if (stableBtnReject == LOW) {
        rejectDelta++;
        Serial.println("[BTN] REJECT +1 -> Delta: " + String(rejectDelta));
      }
    }
  }
  lastBtnReject = rawReject;

  // 3. Tombol FAULT (Toggle saat ditekan)
  bool rawFault = digitalRead(PIN_FAULT);
  if (rawFault != lastBtnFault) debounceFaultTime = now;
  if ((now - debounceFaultTime) > DEBOUNCE_MS) {
    if (rawFault != stableBtnFault) {
      stableBtnFault = rawFault;
      if (stableBtnFault == LOW) {
        faultButtonActive = !faultButtonActive;
        Serial.println("[BTN] FAULT toggled -> " + String(faultButtonActive ? "ACTIVE" : "CLEARED"));
      }
    }
  }
  lastBtnFault = rawFault;
}

void handleDhtSensor() {
  unsigned long now = millis();
  if (now - lastDhtReadTime >= 2000) {
    lastDhtReadTime = now;
    float t = dht.readTemperature();
    if (isnan(t)) {
      sensorOk = false;
      Serial.println("[SENSOR] Gagal membaca DHT22 (temperature_c = null)");
    } else {
      sensorOk = true;
      lastValidTemp = t;
      if (!isOverheat && t >= 70.0) {
        isOverheat = true;
      } else if (isOverheat && t < 65.0) {
        isOverheat = false;
      }
    }
  }
}

void evaluateMachineState() {
  unsigned long now = millis();

  if (faultButtonActive) {
    currentState = "DOWN";
    currentReason = "FAULT_BUTTON";
  } else if (isOverheat) {
    currentState = "DOWN";
    currentReason = "OVERHEAT";
  } else if (lastCountTimestamp > 0 && (now - lastCountTimestamp) <= 15000) {
    currentState = "RUNNING";
    currentReason = "";
  } else {
    currentState = "IDLE";
    currentReason = "";
  }
}

void updateOutputs() {
  unsigned long now = millis();

  digitalWrite(PIN_LED_G, currentState == "RUNNING" ? HIGH : LOW);
  digitalWrite(PIN_LED_Y, currentState == "IDLE" ? HIGH : LOW);
  digitalWrite(PIN_LED_R, currentState == "DOWN" ? HIGH : LOW);

  if (currentState == "DOWN") {
    if (now - lastBuzzerToggleTime >= 300) {
      lastBuzzerToggleTime = now;
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(PIN_BUZZER, 2000);
      } else {
        noTone(PIN_BUZZER);
      }
    }
  } else {
    buzzerState = false;
    noTone(PIN_BUZZER);
  }
}

void ensureWifiConnected() {
  unsigned long now = millis();
  if (WiFi.status() != WL_CONNECTED && (now - lastWifiCheckTime >= 5000)) {
    lastWifiCheckTime = now;
    Serial.println("[WIFI] Terputus, mencoba reconnect...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }
}

bool sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] WiFi offline, tunda kirim seq=" + String(seqNumber));
    return false;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.setTimeout(3000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Key", deviceKey);

  StaticJsonDocument<256> doc;
  doc["machine_id"] = machineId;
  doc["boot_id"] = bootId;
  doc["seq"] = seqNumber;
  doc["state"] = currentState;

  if (currentState == "DOWN" && currentReason.length() > 0) {
    doc["reason"] = currentReason;
  } else {
    doc["reason"] = nullptr;
  }

  doc["output_delta"] = outputDelta;
  doc["reject_delta"] = rejectDelta;

  if (sensorOk) {
    doc["temperature_c"] = round(lastValidTemp * 10.0) / 10.0;
  } else {
    doc["temperature_c"] = nullptr;
  }

  String payload;
  serializeJson(doc, payload);

  int httpCode = http.POST(payload);
  http.end();

  if (httpCode >= 200 && httpCode < 300) {
    Serial.println("[HTTP] Sukses kirim seq=" + String(seqNumber) + " | " + payload);
    outputDelta = 0;
    rejectDelta = 0;
    seqNumber++;
    previousSentState = currentState;
    return true;
  } else {
    Serial.println("[HTTP] Gagal kirim (code=" + String(httpCode) + "), retry seq=" + String(seqNumber));
    return false;
  }
}

void loop() {
  ensureWifiConnected();
  handleButtons();
  handleDhtSensor();
  evaluateMachineState();
  updateOutputs();

  unsigned long now = millis();
  bool stateChanged = (previousSentState != "" && currentState != previousSentState);
  bool intervalReached = (now - lastTelemetrySendTime >= 10000);

  if (stateChanged || intervalReached) {
    lastTelemetrySendTime = now;
    sendTelemetry();
  }
}
