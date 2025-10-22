// === Configuration ===
const int sensorPin = A0;
const int detectionThreshold = 104;
const unsigned long antiDoubleDelay = 100;  // 0.1 s ignore time for second wheel
const unsigned long minDelayBetweenBikes = 2000;  // 0.5 s minimum between bikes

// === Variables ===
bool wheelDetected = false;
unsigned long lastDetection = 0;
int bikeCount = 0;

void setup() {
  Serial.begin(9600);
  Serial.println("Bike counting system ready!");
}

void loop() {
  int sensorValue = analogRead(sensorPin);
  float voltage = sensorValue * (5.0 / 1023.0);
  unsigned long now = millis();

  if (sensorValue > detectionThreshold && !wheelDetected) {
    wheelDetected = true;
    lastDetection = now;
  }

  if (sensorValue < detectionThreshold && wheelDetected) {
    wheelDetected = false;
  }

  if (wheelDetected) {
    unsigned long timeSinceLast = now - lastDetection;

    if (timeSinceLast > antiDoubleDelay) {
      bikeCount++;
      printValue(sensorValue, voltage, bikeCount);
      wheelDetected = false;
      delay(minDelayBetweenBikes);
    }
  }

  delay(5);
}

void printValue(int sensorValue, float voltage, int count) {
  Serial.print("Raw value: ");
  Serial.print(sensorValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage);
  Serial.print(" V | Bike count: ");
  Serial.println(count);
}
