#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFiClientSecure.h>

#define SS_PIN 21
#define RST_PIN 22
#define SENSOR_DIGITAL 4

const char* ssid = "Chwy";
const char* password = "karinakagura";
const char* server = "https://kampus-jwt3.onrender.com";

MFRC522 mfrc522(SS_PIN, RST_PIN);

bool sessionActive = false;
bool tickCountingStarted = false;
bool lastMagnetState = HIGH;

unsigned long lastRFIDCheck = 0;
unsigned long lastTickTime = 0;

const unsigned long RFID_INTERVAL = 1000;
const unsigned long DEBOUNCE_INTERVAL = 1000;

String activeCardID = "";
unsigned long tickCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_DIGITAL, INPUT_PULLUP);  // ✅ Use pull-up once here only
  SPI.begin();
  mfrc522.PCD_Init();

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
  Serial.println(WiFi.localIP());
  Serial.println("System ready...");
}

void loop() {
  unsigned long currentMillis = millis();

  // RFID scanning every second
  if (currentMillis - lastRFIDCheck >= RFID_INTERVAL) {
    lastRFIDCheck = currentMillis;

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      String scannedID = "";
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        scannedID += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
        scannedID += String(mfrc522.uid.uidByte[i], HEX);
      }
      scannedID.toUpperCase();
      Serial.println("Scanned UID: " + scannedID);

      if (!sessionActive) {
        sessionActive = true;
        tickCountingStarted = false;
        tickCount = 0;
        activeCardID = scannedID;
        sendStartSession(scannedID);
        Serial.println("SESSION STARTED");
      } else {
        sessionActive = false;
        sendEndSession(scannedID, tickCount);
        Serial.println("SESSION ENDED — Total Ticks: " + String(tickCount));
      }

      mfrc522.PICC_HaltA();
    }
  }

  // Tick logic only runs during session
  if (sessionActive) {
    int raw = digitalRead(SENSOR_DIGITAL);
    bool magnetDetected = (raw == LOW);
    unsigned long now = millis();

    // Debug print (once every 500ms)
    static unsigned long lastDebug = 0;
    if (now - lastDebug >= 500) {
      lastDebug = now;
      Serial.print("Sensor state: ");
      Serial.println(magnetDetected ? "DETECTED (LOW)" : "NOT DETECTED (HIGH)");
    }

    if (magnetDetected && !lastMagnetState && now - lastTickTime >= DEBOUNCE_INTERVAL) {
      lastTickTime = now;

      if (!tickCountingStarted) {
        tickCountingStarted = true;
        Serial.println("First magnet detected — tick count starts now");
      } else {
        tickCount++;
        Serial.println("TICK! Total: " + String(tickCount));
      }
    }

    lastMagnetState = magnetDetected;
  }
}

void sendStartSession(String cardId) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    String url = String(server) + "/sessions/start";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"cardId\":\"" + cardId + "\"}";
    int code = http.POST(json);

    Serial.println("Start session HTTP response: " + String(code));
    Serial.println("Response: " + http.getString());

    http.end();
  } else {
    Serial.println("❌ WiFi not connected");
  }
}

void sendEndSession(String cardId, unsigned long tickCount) {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    String url = String(server) + "/sessions/end";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"cardId\":\"" + cardId + "\", \"tickCount\":" + String(tickCount) + "}";
    int code = http.POST(json);

    Serial.println("End session HTTP response: " + String(code));
    Serial.println("Response: " + http.getString());

    http.end();
  } else {
    Serial.println("❌ WiFi not connected");
  }
}


