#include <SoftwareSerial.h>
#include <LoRa_E220.h>
#include <EEPROM.h>

// === Broches du module E220 ===
#define PIN_RX 2
#define PIN_TX 3
#define PIN_M0 4
#define PIN_M1 5
#define PIN_AUX 6

// === Broche du bouton ===
#define PIN_REMOTE_SAVE_BUTTON 8

// === EEPROM ===
#define EEPROM_ADDR_TOTAL   0
#define EEPROM_ADDR_SESSION sizeof(int)
const unsigned long SAVE_INTERVAL = 3UL * 60UL * 60UL * 1000UL; // 3 heures

// === Variables globales ===
int lastTotal = 0;
int lastSession = 0;
unsigned long lastSaveTime = 0;
bool newData = false;

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
  Serial.println("=== Récepteur LoRa E220 prêt ===");

  pinMode(PIN_REMOTE_SAVE_BUTTON, INPUT_PULLUP);

  setupLoRa();
  loadCountsFromEEPROM();

  Serial.print("Valeurs restaurées → Total: ");
  Serial.print(lastTotal);
  Serial.print(" | Session: ");
  Serial.println(lastSession);

  lastSaveTime = millis();
}

// --------------------------------------------------------
// LOOP PRINCIPALE
// --------------------------------------------------------
void loop() {
  unsigned long now = millis();

  handleLoRaReception();
  handlePeriodicSave(now);
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
// GESTION RECEPTION LORA
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

  if (msg == "SAVED_OK") {
    Serial.println("Confirmation de sauvegarde reçue depuis l’émetteur.");
    return;
  }

  int comma = msg.indexOf(',');
  if (comma > 0) {
    lastTotal = msg.substring(0, comma).toInt();
    lastSession = msg.substring(comma + 1).toInt();
    newData = true;

    Serial.print("📩 Données reçues → Total: ");
    Serial.print(lastTotal);
    Serial.print(" | Session: ");
    Serial.println(lastSession);
  }
}

// --------------------------------------------------------
// GESTION SAUVEGARDE PERIODIQUE
// --------------------------------------------------------
void handlePeriodicSave(unsigned long now) {
  if (now - lastSaveTime >= SAVE_INTERVAL && newData) {
    saveCountsToEEPROM();
    newData = false;
    lastSaveTime = now;
  }
}

// --------------------------------------------------------
// GESTION DU BOUTON
// --------------------------------------------------------
void handleButton(unsigned long now) {
  bool buttonState = digitalRead(PIN_REMOTE_SAVE_BUTTON);

  // Début d'appui
  if (lastButtonState == HIGH && buttonState == LOW) {
    buttonPressStart = now;
    buttonHeld = false;
  }

  // Appui long détecté
  if (buttonState == LOW && !buttonHeld && (now - buttonPressStart >= 4000)) {
    Serial.println("Appui long → Envoi commande SAVE à l’émetteur et sauvegarde locale.");
    e220.sendMessage("SAVE");
    saveCountsToEEPROM();
    buttonHeld = true;
  }

  lastButtonState = buttonState;
}

// --------------------------------------------------------
// GESTION EEPROM
// --------------------------------------------------------
void loadCountsFromEEPROM() {
  EEPROM.get(EEPROM_ADDR_TOTAL, lastTotal);
  EEPROM.get(EEPROM_ADDR_SESSION, lastSession);

  if (lastTotal < 0 || lastTotal > 999999) lastTotal = 0;
  if (lastSession < 0 || lastSession > 999999) lastSession = 0;
}

void saveCountsToEEPROM() {
  EEPROM.put(EEPROM_ADDR_TOTAL, lastTotal);
  EEPROM.put(EEPROM_ADDR_SESSION, lastSession);
  Serial.print("Sauvegarde locale → Total: ");
  Serial.print(lastTotal);
  Serial.print(" | Session: ");
  Serial.println(lastSession);
}
