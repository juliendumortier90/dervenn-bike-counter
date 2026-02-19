#include <FastLED.h>

// --- Hardware / LED strip ---
#define DATA_PIN 6
#define LED_TYPE WS2811
#define COLOR_ORDER GRB

// --- Display layout ---
#define NUM_DIGITS 10
#define SEGMENTS_PER_DIGIT 7
#define NUM_LEDS (NUM_DIGITS * SEGMENTS_PER_DIGIT)

// --- Brightness (60%) ---
#define BRIGHTNESS 153

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

CRGB digitColors[NUM_DIGITS];

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

void setup() {
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(BRIGHTNESS);

  // Distinct color per digit
  for (uint8_t i = 0; i < NUM_DIGITS; i++) {
    uint8_t hue = (uint8_t)(i * 256 / NUM_DIGITS);
    digitColors[i] = CHSV(hue, 255, 255);
  }

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
}

void renderDigit(uint8_t digitIndex, uint8_t currentVal, uint8_t nextVal, uint8_t blend) {
  uint8_t maskCurrent = digitMask[currentVal];
  uint8_t maskNext = digitMask[nextVal];

  for (uint8_t seg = 0; seg < SEGMENTS_PER_DIGIT; seg++) {
    uint8_t onA = (maskCurrent & (1 << seg)) ? 255 : 0;
    uint8_t onB = (maskNext & (1 << seg)) ? 255 : 0;
    uint8_t level = lerp8by8(onA, onB, blend);

    CRGB c = digitColors[digitIndex];
    c.nscale8(level);
    leds[ledIndexForSegment(digitIndex, seg)] = c;
  }
}

void loop() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < 20) {
    return;
  }
  lastFrameMs = now;

  const uint16_t HOLD_MS = 800; // temps lisible sur le chiffre courant
  const uint16_t FADE_MS = 200; // transition courte
  const uint16_t STEP_MS = HOLD_MS + FADE_MS;

  uint32_t step = now / STEP_MS;
  uint16_t phase = (uint16_t)(now % STEP_MS);

  uint8_t currentVal = (uint8_t)(step % 10);
  uint8_t nextVal = (uint8_t)((currentVal + 1) % 10);

  uint8_t t = 0;
  if (phase > HOLD_MS) {
    uint16_t fadePhase = (uint16_t)(phase - HOLD_MS);
    t = (uint8_t)((fadePhase * 255UL) / (FADE_MS - 1));
    t = ease8InOutCubic(t);
  }

  for (uint8_t d = 0; d < NUM_DIGITS; d++) {
    // Tous les digits affichent le même chiffre.
    renderDigit(d, currentVal, nextVal, t);
  }

  FastLED.show();
}
