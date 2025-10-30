#include <SoftwareSerial.h>
#include <LoRa_E220.h>

// === Broches LoRa E220 ===
#define PIN_RX 2
#define PIN_TX 3
#define PIN_M0 4
#define PIN_M1 5
#define PIN_AUX 6

// === Capteur ===
const int sensorPin = A0;
const int detectionThreshold = 107;
const unsigned long minDelayBetweenBikes = 800;

// === Variables ===
unsigned long lastDetectionTime = 0;
int bikeCount = 0;

// === Objet LoRa E220 ===
SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E220 e220(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("Initialisation du système de comptage...");

  e220.begin();
  e220.setMode(MODE_0_NORMAL);
  Serial.println("Module LoRa E220 initialisé en mode normal.");

  Serial.println("Système prêt !");
}

void loop() {
  unsigned long now = millis();
  int sensorValue = analogRead(sensorPin);
  float voltage = sensorValue * (5.0 / 1023.0);

  if (sensorValue > detectionThreshold && (now - lastDetectionTime > minDelayBetweenBikes)) {
    bikeCount++;
    lastDetectionTime = now;

    Serial.print("Raw: ");
    Serial.print(sensorValue);
    Serial.print(" | Voltage: ");
    Serial.print(voltage);
    Serial.print(" V | Count: ");
    Serial.println(bikeCount);

    sendLoRaMessage(bikeCount);
  }
}

void sendLoRaMessage(int count) {
  String message = "VELO DETECTE - Compteur: " + String(count);
  ResponseStatus rs = e220.sendMessage(message);
  
  Serial.print("Message LoRa envoyé: ");
  Serial.print(message);
  Serial.print(" | Status: ");
  Serial.println(rs.getResponseDescription());
}
