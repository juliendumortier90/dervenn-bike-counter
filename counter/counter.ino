// === Configuration ===
const int sensorPin = A0;
const int detectionThreshold = 102;
const unsigned long minDelayBetweenBikes = 600;  // ms à ignorer après une détection

// === Variables ===
unsigned long lastDetectionTime = 0;
int bikeCount = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Bike counting system ready!");
}

void loop() {
  unsigned long now = millis();
  int sensorValue = analogRead(sensorPin);
  float voltage = sensorValue * (5.0 / 1023.0);

  // Si la valeur dépasse le seuil et qu’on a attendu assez longtemps depuis la dernière détection
  if (sensorValue > detectionThreshold && (now - lastDetectionTime > minDelayBetweenBikes)) {
    bikeCount++;
    lastDetectionTime = now;

    Serial.print("Raw: ");
    Serial.print(sensorValue);
    Serial.print(" | Voltage: ");
    Serial.print(voltage);
    Serial.print(" V | Count: ");
    Serial.println(bikeCount);
  }
}
