#include <SoftwareSerial.h>
#include <LoRa_E220.h>
#include <EEPROM.h>
#include <avr/wdt.h>
#include <FastLED.h>

// === Broches du module E220 (identiques a digit.ino) ===
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

// === LED / Afficheur 7 segments ===
#define DATA_PIN 10
#define LED_TYPE WS2811
#define COLOR_ORDER RGB

#define NUM_DIGITS 10
#define SEGMENTS_PER_DIGIT 7
#define NUM_LEDS (NUM_DIGITS * SEGMENTS_PER_DIGIT)

// Couleur des compteurs (hors animations)
const CRGB COUNTER_COLOR = CRGB::Red;

// Intensite pour les compteurs (0..255)
const uint8_t COUNTER_BRIGHTNESS = 255;

// Intensite pour les animations (0..255)
const uint8_t ANIM_BRIGHTNESS = 255;

// Cadence d'affichage
const uint16_t FRAME_MS = 20;

// Intervalle entre les sequences d'animations
const uint32_t ANIM_INTERVAL_MS = 60UL * 1000UL * 5UL; // 1 minute

// Duree d'une animation
const uint16_t ANIM_MS = 8000;

const bool BOTTOM_ROW_REVERSED = true;

CRGB leds[NUM_LEDS];

// Segment index order: a, b, c, d, e, f, g
// First row (digits 0..4): f=0, a=1, b=2, g=3, e=4, d=5, c=6
const uint8_t segmentMapTop[SEGMENTS_PER_DIGIT] = {
  1, // a
  2, // b
  6, // c
  5, // d
  4, // e
  0, // f
  3  // g
};

// Second row (digits 5..9): b=0, a=1, f=2, g=3, c=4, d=5, e=6
const uint8_t segmentMapBottom[SEGMENTS_PER_DIGIT] = {
  1, // a
  0, // b
  4, // c
  5, // d
  6, // e
  2, // f
  3  // g
};

// 7-seg glyphs, bits are a..g (bit0 = a, bit6 = g)
const uint8_t digitMask[10] = {
  0b0111111, // 0: a b c d e f
  0b0000110, // 1: b c
  0b1011011, // 2: a b d e g
  0b1001111, // 3: a b c d g
  0b1100110, // 4: b c f g
  0b1101101, // 5: a c d f g
  0b1111101, // 6: a c d e f g
  0b0000111, // 7: a b c
  0b1111111, // 8: a b c d e f g
  0b1101111  // 9: a b c d f g
};

// Animations retenues (ID + 1 affiche)
// 1 Snake, 3 RainbowWave, 18 Confetti
const uint8_t NUM_ANIMATIONS = 3;
const uint8_t animIdList[NUM_ANIMATIONS] = {1, 3, 18};

// === Variables globales ===
int lastTotal = 0;
int lastSession = 0;
unsigned long lastSaveTime = 0;
bool newData = false;

bool lastButtonState = HIGH;
unsigned long buttonPressStart = 0;

const uint32_t BUTTON_SAVE_MS = 2000;
const uint32_t BUTTON_RESET_SESSION_MS = 10000;
const uint32_t BUTTON_RESET_TOTAL_MS = 20000;
const uint16_t BUTTON_BLINK_MS = 250;

// === Watchdog ===
const uint32_t WDT_TIMEOUT_MS = 2000;

// === LoRa ===
SoftwareSerial loraSerial(PIN_RX, PIN_TX);
LoRa_E220 e220(&loraSerial, PIN_AUX, PIN_M0, PIN_M1);

// === Etat affichage ===
const uint8_t MODE_COUNT = 0;
const uint8_t MODE_ANIM = 1;
uint8_t mode = MODE_ANIM;
uint8_t animIndex = 0;
uint32_t modeStartMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastAnimTriggerMs = 0;

const uint8_t BUTTON_ACTION_NONE = 0;
const uint8_t BUTTON_ACTION_SAVE = 1;
const uint8_t BUTTON_ACTION_RESET_SESSION = 2;
const uint8_t BUTTON_ACTION_RESET_TOTAL = 3;

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
// OUTILS AFFICHAGE
// --------------------------------------------------------
uint16_t baseLedIndexForDigit(uint8_t digitIndex) {
  return (uint16_t)digitIndex * SEGMENTS_PER_DIGIT;
}

uint16_t ledIndexForSegment(uint8_t digitIndex, uint8_t segmentIndex) {
  uint16_t base = baseLedIndexForDigit(digitIndex);
  if (digitIndex < 5) {
    return base + segmentMapTop[segmentIndex];
  }
  return base + segmentMapBottom[segmentIndex];
}

void renderDigitSolid(uint8_t digitIndex, uint8_t value, CRGB color) {
  uint8_t mask = (value <= 9) ? digitMask[value] : 0;
  for (uint8_t seg = 0; seg < SEGMENTS_PER_DIGIT; seg++) {
    leds[ledIndexForSegment(digitIndex, seg)] = (mask & (1 << seg)) ? color : CRGB::Black;
  }
}

uint8_t rowDigitIndex(uint8_t rowStartDigit, uint8_t offset, bool reverse) {
  if (reverse) {
    return (uint8_t)(rowStartDigit + (4 - offset));
  }
  return (uint8_t)(rowStartDigit + offset);
}

void renderRowNumber5(uint8_t rowStartDigit, int value, CRGB color, bool reverse) {
  uint32_t v = (value < 0) ? 0U : (uint32_t)value;
  v %= 100000UL; // limite a 5 digits

  const uint32_t divisors[5] = {10000UL, 1000UL, 100UL, 10UL, 1UL};
  bool started = false;

  for (uint8_t pos = 0; pos < 5; pos++) {
    uint8_t digit = (uint8_t)((v / divisors[pos]) % 10);
    if (digit != 0 || started || pos == 4) {
      renderDigitSolid(rowDigitIndex(rowStartDigit, pos, reverse), digit, color);
      started = true;
    } else {
      renderDigitSolid(rowDigitIndex(rowStartDigit, pos, reverse), 255, color);
    }
  }
}

CRGB scaleColor(CRGB color, uint8_t brightness) {
  CRGB c = color;
  c.nscale8_video(brightness);
  return c;
}

uint8_t getButtonActionForDuration(uint32_t pressDuration) {
  if (pressDuration >= BUTTON_RESET_TOTAL_MS) {
    return BUTTON_ACTION_RESET_TOTAL;
  }
  if (pressDuration >= BUTTON_RESET_SESSION_MS) {
    return BUTTON_ACTION_RESET_SESSION;
  }
  if (pressDuration >= BUTTON_SAVE_MS) {
    return BUTTON_ACTION_SAVE;
  }
  return BUTTON_ACTION_NONE;
}

CRGB buttonActionColor(uint8_t action) {
  switch (action) {
    case BUTTON_ACTION_SAVE: return CRGB::Green;
    case BUTTON_ACTION_RESET_SESSION: return CRGB(255, 96, 0);
    case BUTTON_ACTION_RESET_TOTAL: return CRGB::Red;
    default: return CRGB::Black;
  }
}

void renderButtonFeedback(uint32_t now) {
  uint32_t pressDuration = now - buttonPressStart;
  uint8_t action = getButtonActionForDuration(pressDuration);

  if (action == BUTTON_ACTION_NONE) {
    renderCountDisplay();
    return;
  }

  bool ledsOn = ((now / BUTTON_BLINK_MS) % 2U) == 0U;
  if (!ledsOn) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    return;
  }

  renderCountDisplayWithColor(scaleColor(buttonActionColor(action), COUNTER_BRIGHTNESS));
}

void renderCountDisplayWithColor(CRGB color) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  renderRowNumber5(0, lastTotal, color, false);
  renderRowNumber5(5, lastSession, color, BOTTOM_ROW_REVERSED);
}

void renderCountDisplay() {
  renderCountDisplayWithColor(scaleColor(COUNTER_COLOR, COUNTER_BRIGHTNESS));
}

void renderSnake(CRGB *out, uint32_t t, uint16_t dur) {
  fill_solid(out, NUM_LEDS, CRGB::Black);

  uint16_t head = (uint16_t)((t / 80) % NUM_LEDS);
  uint16_t maxLen = (NUM_LEDS < 40) ? NUM_LEDS : 40;
  uint16_t len = (uint16_t)(3 + (t * (maxLen - 3)) / (dur - 1));

  for (uint16_t i = 0; i < len; i++) {
    uint16_t idx = (head + NUM_LEDS - (i % NUM_LEDS)) % NUM_LEDS;
    CRGB c = CHSV(0, 255, 255);
    c.nscale8((uint8_t)(255 - (i * 255 / len)));
    out[idx] = c;
  }
}

void renderRainbowWave(CRGB *out, uint32_t t) {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t phase = (uint8_t)(i * 8 + t / 10);
    uint8_t v = sin8(phase);
    out[i] = CHSV((uint8_t)(t / 8 + i * 3), 255, v);
  }
}

void renderConfetti(CRGB *out, uint32_t t) {
  (void)t;
  fadeToBlackBy(out, NUM_LEDS, 20);
  uint16_t pos = random16(NUM_LEDS);
  out[pos] += CHSV(random8(), 200, 255);
}

void renderAnimation(CRGB *out, uint8_t animId, uint32_t t, uint16_t dur) {
  switch (animId) {
    case 1: renderSnake(out, t, dur); break;
    case 3: renderRainbowWave(out, t); break;
    case 18: renderConfetti(out, t); break;
    default: fill_solid(out, NUM_LEDS, CRGB::Black); break;
  }
}

void updateDisplay(uint32_t now) {
  if (now - lastFrameMs < FRAME_MS) {
    return;
  }
  lastFrameMs = now;

  if (digitalRead(PIN_REMOTE_SAVE_BUTTON) == LOW) {
    renderButtonFeedback(now);
    FastLED.show();
    return;
  }

  if (mode == MODE_ANIM) {
    uint32_t animElapsed = now - modeStartMs;
    if (animElapsed >= ANIM_MS) {
      animIndex++;
      if (animIndex >= NUM_ANIMATIONS) {
        mode = MODE_COUNT;
        lastAnimTriggerMs = now;
      } else {
        modeStartMs = now;
      }
      return;
    }

    uint8_t animId = animIdList[animIndex];
    renderAnimation(leds, animId, animElapsed, ANIM_MS);
    if (ANIM_BRIGHTNESS < 255) {
      nscale8_video(leds, NUM_LEDS, ANIM_BRIGHTNESS);
    }
    FastLED.show();
    return;
  }

  if (now - lastAnimTriggerMs >= ANIM_INTERVAL_MS) {
    mode = MODE_ANIM;
    animIndex = 0;
    modeStartMs = now;
    return;
  }

  renderCountDisplay();
  FastLED.show();
}

// --------------------------------------------------------
// SETUP
// --------------------------------------------------------
void setup() {
  Serial.begin(9600);
  delay(500);
  Serial.println("=== Recepteur LoRa E220 pret ===");

  pinMode(PIN_REMOTE_SAVE_BUTTON, INPUT_PULLUP);

  initWatchdog();

  setupLoRa();
  loadCountsFromEEPROM();

  Serial.print("Valeurs restaurees -> Total: ");
  Serial.print(lastTotal);
  Serial.print(" | Session: ");
  Serial.println(lastSession);

  lastSaveTime = millis();

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  randomSeed(analogRead(A0));

  modeStartMs = millis();
  lastAnimTriggerMs = modeStartMs;
}

// --------------------------------------------------------
// LOOP PRINCIPALE
// --------------------------------------------------------
void loop() {
  uint32_t now = millis();

  refreshWatchdog();
  handleLoRaReception();
  handlePeriodicSave(now);
  handleButton(now);
  updateDisplay(now);
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
// GESTION RECEPTION LORA
// --------------------------------------------------------
void handleLoRaReception() {
  if (e220.available() <= 1) return;

  ResponseContainer rc = e220.receiveMessage();

  if (rc.status.code != 1) {
    Serial.print("Erreur LoRa : ");
    Serial.println(rc.status.getResponseDescription());
    return;
  }

  String msg = rc.data;
  msg.trim();

  if (msg == "BIKE") {
    lastTotal++;
    lastSession++;
    newData = true;

    Serial.print("Velo recu -> Total: ");
    Serial.print(lastTotal);
    Serial.print(" | Session: ");
    Serial.println(lastSession);
  }
}

// --------------------------------------------------------
// GESTION SAUVEGARDE PERIODIQUE
// --------------------------------------------------------
void handlePeriodicSave(uint32_t now) {
  if (now - lastSaveTime >= SAVE_INTERVAL && newData) {
    saveCountsToEEPROM();
    newData = false;
    lastSaveTime = now;
  }
}

// --------------------------------------------------------
// GESTION DU BOUTON
// --------------------------------------------------------
void handleButton(uint32_t now) {
  bool buttonState = digitalRead(PIN_REMOTE_SAVE_BUTTON);

  // Debut d'appui
  if (lastButtonState == HIGH && buttonState == LOW) {
    buttonPressStart = now;
  }

  if (lastButtonState == LOW && buttonState == HIGH) {
    uint32_t pressDuration = now - buttonPressStart;
    uint8_t action = getButtonActionForDuration(pressDuration);

    if (action == BUTTON_ACTION_SAVE) {
      Serial.println("Relachement -> sauvegarde locale.");
      saveCountsToEEPROM();
      newData = false;
      lastSaveTime = now;
    } else if (action == BUTTON_ACTION_RESET_SESSION) {
      Serial.println("Relachement -> remise a zero du compteur session.");
      lastSession = 0;
      EEPROM.put(EEPROM_ADDR_SESSION, lastSession);
      newData = false;
      lastSaveTime = now;
    } else if (action == BUTTON_ACTION_RESET_TOTAL) {
      Serial.println("Relachement -> remise a zero des compteurs total et session.");
      lastTotal = 0;
      lastSession = 0;
      EEPROM.put(EEPROM_ADDR_TOTAL, lastTotal);
      EEPROM.put(EEPROM_ADDR_SESSION, lastSession);
      newData = false;
      lastSaveTime = now;
    }

    mode = MODE_COUNT;
    lastAnimTriggerMs = now;
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
  Serial.print("Sauvegarde locale -> Total: ");
  Serial.print(lastTotal);
  Serial.print(" | Session: ");
  Serial.println(lastSession);
}
