#include <SoftwareSerial.h>
#include <LoRa_E220.h>

// === Broches du module E220 (mêmes que l'émetteur) ===
#define PIN_RX 2    // Arduino reçoit depuis TXD du module
#define PIN_TX 3    // Arduino envoie vers RXD du module (via diviseur)
#define PIN_M0 4
#define PIN_M1 5
#define PIN_AUX 6

// === Création du port série LoRa ===
SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E220 e220(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("=== Récepteur LoRa E220 prêt ===");

  e220.begin();
  e220.setMode(MODE_0_NORMAL);  // Mode normal pour réception/transmission
  Serial.println("Module initialisé en MODE_0_NORMAL");
}

void loop() {
  // Vérifie si un message est disponible
  if (e220.available() > 1) {
    ResponseContainer rc = e220.receiveMessage();
    
    if (rc.status.code == 1) {
      Serial.print("📩 Message reçu : ");
      Serial.println(rc.data);
    } else {
      Serial.print("⚠️ Erreur de réception : ");
      Serial.println(rc.status.getResponseDescription());
    }
  }
}
