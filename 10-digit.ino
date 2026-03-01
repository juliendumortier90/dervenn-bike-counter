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

// Timing general
const uint16_t FRAME_MS = 20;
const uint16_t ANIM_MS = 8000;
const uint16_t COUNT_TOTAL_MS = 10000;
const bool BOTTOM_ROW_REVERSED = true;

// Animations retenues (ID + 1 affiché)
// 1 Snake, 3 RainbowWave, 18 Confetti
const uint8_t NUM_ANIMATIONS = 3;
const uint8_t animIdList[NUM_ANIMATIONS] = {1, 3, 18};

// State
uint8_t currentCount = 0;
uint8_t cycleCount = 1;
uint32_t modeStartMs = 0;
const uint8_t MODE_COUNT = 0;
const uint8_t MODE_ANIM = 1;
uint8_t mode = MODE_COUNT;
uint8_t animIndex = 0;

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

  fill_solid(leds, NUM_LEDS, CRGB::Black);
  FastLED.show();
  randomSeed(analogRead(A0));
  modeStartMs = millis();
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

void renderRowNumber(uint8_t rowStartDigit, uint8_t value, CRGB color, bool reverse) {
  for (uint8_t d = 0; d < 5; d++) {
    renderDigitSolid(rowDigitIndex(rowStartDigit, d, reverse), 255, color);
  }

  uint8_t tens = value / 10;
  uint8_t ones = value % 10;
  if (tens > 0) {
    renderDigitSolid(rowDigitIndex(rowStartDigit, 3, reverse), tens, color);
    renderDigitSolid(rowDigitIndex(rowStartDigit, 4, reverse), ones, color);
  } else {
    renderDigitSolid(rowDigitIndex(rowStartDigit, 4, reverse), ones, color);
  }
}

void renderCountDisplay() {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  renderRowNumber(0, currentCount, CRGB::White, false);
  renderRowNumber(5, cycleCount, CRGB::White, BOTTOM_ROW_REVERSED);
}

void renderSnake(CRGB *out, uint32_t t, uint16_t dur) {
  fill_solid(out, NUM_LEDS, CRGB::Black);

  uint16_t head = (uint16_t)((t / 80) % NUM_LEDS);
  uint16_t maxLen = (NUM_LEDS < 40) ? NUM_LEDS : 40;
  uint16_t len = (uint16_t)(3 + (t * (maxLen - 3)) / (dur - 1));

  for (uint16_t i = 0; i < len; i++) {
    uint16_t idx = (head + NUM_LEDS - (i % NUM_LEDS)) % NUM_LEDS;
    CRGB c = CHSV(96, 255, 255);
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

void loop() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) {
    return;
  }
  lastFrameMs = now;

  if (mode == MODE_COUNT) {
    uint32_t elapsed = now - modeStartMs;
    if (elapsed >= COUNT_TOTAL_MS) {
      currentCount = 10;
      mode = MODE_ANIM;
      modeStartMs = now;
      animIndex = 0;
      return;
    }
    currentCount = (uint8_t)((elapsed * 11UL) / COUNT_TOTAL_MS);
    if (currentCount > 10) {
      currentCount = 10;
    }
    renderCountDisplay();
    FastLED.show();
    return;
  }

  uint32_t animElapsed = now - modeStartMs;
  if (animElapsed >= ANIM_MS) {
    animIndex++;
    if (animIndex >= NUM_ANIMATIONS) {
      mode = MODE_COUNT;
      modeStartMs = now;
      currentCount = 0;
      cycleCount++;
      if (cycleCount > 99) {
        cycleCount = 1;
      }
    } else {
      modeStartMs = now;
    }
    return;
  }

  uint8_t animId = animIdList[animIndex];
  renderAnimation(leds, animId, (uint16_t)animElapsed, ANIM_MS);
  FastLED.show();
}
