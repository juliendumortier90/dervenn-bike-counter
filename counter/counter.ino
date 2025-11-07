#include <SoftwareSerial.h>
#include <LoRa_E220.h>
#include <EEPROM.h>

// === Broches LoRa E220 ===
#define PIN_RX 2
#define PIN_TX 3
#define PIN_M0 4
#define PIN_M1 5
#define PIN_AUX 6

// === Bouton ===
#define PIN_SAVE_BUTTON 7

// === Capteur ===
const int SENSOR_PIN = A0;
const int DETECTION_THRESHOLD = 124;
const unsigned long MIN_DELAY_BETWEEN_BIKES = 800; // ms

// === EEPROM ===
#define EEPROM_ADDR_TOTAL   0
#define EEPROM_ADDR_SESSION sizeof(int)
const unsigned long SAVE_INTERVAL = 3UL * 60UL * 60UL * 1000UL; // 3 heures

// === Variables ===
unsigned long lastDetectionTime = 0;
unsigned long lastSaveTime = 0;
int bikeCountTotal = 0;
int bikeCountSession = 0;
bool lastButtonState = HIGH;
unsigned long buttonPressStart = 0;
bool buttonHeld = false;

// === LoRa ===
SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E220 e220(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

// --------------------------------------------------------
// SETUP
// --------------------------------------------------------
void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("Initialisation du système de comptage...");

  pinMode(PIN_SAVE_BUTTON, INPUT_PULLUP);

  setupLoRa();
  loadCountsFromEEPROM();

  Serial.print("Valeurs initiales → Total: ");
  Serial.print(bikeCountTotal);
  Serial.print(" | Session: ");
  Serial.println(bikeCountSession);

  lastSaveTime = millis();
  Serial.println("Système prêt !");
}

// --------------------------------------------------------
// LOOP PRINCIPALE
// --------------------------------------------------------
void loop() {
  unsigned long now = millis();

  handleDetection(now);
  handleLoRaReception();
  handleAutoSave(now);
  handleButton(now);
}

// --------------------------------------------------------
// INITIALISATION LORA
// --------------------------------------------------------
void setupLoRa() {
  e220.begin();
  e220.setMode(MODE_0_NORMAL);
  Serial.println("Module LoRa E220 initialisé en MODE_0_NORMAL");
}

// --------------------------------------------------------
// DÉTECTION DE PASSAGE DE VÉLO
// --------------------------------------------------------
void handleDetection(unsigned long now) {
  int sensorValue = analogRead(SENSOR_PIN);
  if (sensorValue > DETECTION_THRESHOLD && (now - lastDetectionTime > MIN_DELAY_BETWEEN_BIKES)) {
    bikeCountSession++;
    bikeCountTotal++;
    lastDetectionTime = now;

    Serial.print("Passage détecté → Total: ");
    Serial.print(bikeCountTotal);
    Serial.print(" | Session: ");
    Serial.print(bikeCountSession);
    Serial.print("Sessor Value: ");
    Serial.println(sensorValue);

    sendLoRaMessage(bikeCountTotal, bikeCountSession);
  }
}

// --------------------------------------------------------
// RÉCEPTION DES COMMANDES LORA
// --------------------------------------------------------
void handleLoRaReception() {
  if (e220.available() <= 1) return;

  ResponseContainer rc = e220.receiveMessage();
  if (rc.status.code != 1) {
    Serial.print("⚠️ Erreur LoRa : ");
    Serial.println(rc.status.getResponseDescription());
    return;
  }

  String msg = rc.data;
  msg.trim();

  if (msg == "SAVE") {
    Serial.println("Commande SAVE reçue via LoRa !");
    saveCountsToEEPROM();
    e220.sendMessage("SAVED_OK");  // Confirmation envoyée au récepteur
  }
}

// --------------------------------------------------------
// SAUVEGARDE AUTOMATIQUE
// --------------------------------------------------------
void handleAutoSave(unsigned long now) {
  if (now - lastSaveTime >= SAVE_INTERVAL) {
    saveCountsToEEPROM();
    e220.sendMessage("SAVED_OK"); // Notification du récepteur
    lastSaveTime = now;
  }
}

// --------------------------------------------------------
// GESTION DU BOUTON LOCAL
// --------------------------------------------------------
void handleButton(unsigned long now) {
  bool buttonState = digitalRead(PIN_SAVE_BUTTON);

  // Début d'appui
  if (lastButtonState == HIGH && buttonState == LOW) {
    buttonPressStart = now;
    buttonHeld = false;
  }

  // Appui long -> reset session
  if (buttonState == LOW && !buttonHeld && (now - buttonPressStart >= 8000)) {
    Serial.println("Appui long → Réinitialisation du compteur de session !");
    resetSessionCounter();
    e220.sendMessage("SAVED_OK");  // Confirmation pour le récepteur
    buttonHeld = true;
  }

  // Relâchement du bouton -> appui court
  if (lastButtonState == LOW && buttonState == HIGH) {
    unsigned long pressDuration = now - buttonPressStart;
    if (!buttonHeld && pressDuration < 8000) {
      Serial.println("Appui court → Sauvegarde en EEPROM");
      saveCountsToEEPROM();
      e220.sendMessage("SAVED_OK");  // Notification du récepteur
    }
  }

  lastButtonState = buttonState;
}

// --------------------------------------------------------
// SAUVEGARDE / RESTAURATION EEPROM
// --------------------------------------------------------
void loadCountsFromEEPROM() {
  EEPROM.get(EEPROM_ADDR_TOTAL, bikeCountTotal);
  EEPROM.get(EEPROM_ADDR_SESSION, bikeCountSession);

  if (bikeCountTotal < 0 || bikeCountTotal > 999999) bikeCountTotal = 0;
  if (bikeCountSession < 0 || bikeCountSession > 999999) bikeCountSession = 0;
}

void saveCountsToEEPROM() {
  EEPROM.put(EEPROM_ADDR_TOTAL, bikeCountTotal);
  EEPROM.put(EEPROM_ADDR_SESSION, bikeCountSession);
  Serial.print("Sauvegarde → Total: ");
  Serial.print(bikeCountTotal);
  Serial.print(" | Session: ");
  Serial.println(bikeCountSession);
}

void resetSessionCounter() {
  bikeCountSession = 0;
  EEPROM.put(EEPROM_ADDR_SESSION, bikeCountSession);
  Serial.println("Compteur de session remis à zéro !");
}

// --------------------------------------------------------
// ENVOI LORA
// --------------------------------------------------------
void sendLoRaMessage(int total, int session) {
  String message = String(total) + "," + String(session);
  e220.sendMessage(message);
}
