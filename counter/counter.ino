#include <SoftwareSerial.h>
#include <LoRa_E220.h>
#include <avr/wdt.h>

// === Broches LoRa E220 ===
#define PIN_RX 2
#define PIN_TX 3
#define PIN_M0 4
#define PIN_M1 5
#define PIN_AUX 6

// === Capteur ===
const int SENSOR_PIN = A0;
const int DETECTION_THRESHOLD = 114;
const unsigned long MIN_DELAY_BETWEEN_BIKES = 800; // ms

// === Watchdog ===
const uint32_t WDT_TIMEOUT_MS = 2000;

// === LoRa ===
SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E220 e220(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

// === Variables ===
unsigned long lastDetectionTime = 0;

void initWatchdog() {
  wdt_disable();
  wdt_enable(WDTO_2S);
  Serial.print("WDT actif (~");
  Serial.print(WDT_TIMEOUT_MS);
  Serial.println(" ms)");
}

void refreshWatchdog() {
  wdt_reset();
}

// --------------------------------------------------------
// SETUP
// --------------------------------------------------------
void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("Initialisation de l'emetteur velo...");

  initWatchdog();
  setupLoRa();

  Serial.println("Emetteur pret : envoi de l'evenement BIKE a chaque passage.");
}

// --------------------------------------------------------
// LOOP PRINCIPALE
// --------------------------------------------------------
void loop() {
  unsigned long now = millis();

  refreshWatchdog();
  handleDetection(now);
}

// --------------------------------------------------------
// INITIALISATION LORA
// --------------------------------------------------------
void setupLoRa() {
  e220.begin();
  e220.setMode(MODE_0_NORMAL);
  Serial.println("Module LoRa E220 initialise en MODE_0_NORMAL");
}

// --------------------------------------------------------
// DETECTION DE PASSAGE
// --------------------------------------------------------
void handleDetection(unsigned long now) {
  int sensorValue = analogRead(SENSOR_PIN);
  if (sensorValue > DETECTION_THRESHOLD &&
      (now - lastDetectionTime > MIN_DELAY_BETWEEN_BIKES)) {
    lastDetectionTime = now;

    Serial.print("Passage detecte, capteur=");
    Serial.println(sensorValue);

    sendBikeEvent();
  }
}

// --------------------------------------------------------
// ENVOI LORA
// --------------------------------------------------------
void sendBikeEvent() {
  e220.sendMessage("BIKE");
}
