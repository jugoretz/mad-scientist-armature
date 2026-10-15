// ============================================================
// Steampunk gauge Sweep (voltmeter) + NeoPixels
// Classic ESP32, (NOT ESP32S3) DAC on GPIO25, 2kΩ + 1kΩ series to meter +
// NeoPixels on GPIO18 (or any safe pin - avoid 6,7,8,9,10,11)
// ============================================================

#include "driver/dac.h"
#include <Adafruit_NeoPixel.h>

// ============================================================
// NEOPIXEL CONFIGURATION
// Change PIN to whatever GPIO you're using
// Change NUMPIXELS to match your strip/ring count
// ============================================================
#define NEOPIXEL_PIN 18
#define NUMPIXELS    2
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRBW + NEO_KHZ800);

// ============================================================
// NEOPIXEL BRIGHTNESS
// 0 = off, 255 = full brightness
// ============================================================
#define NEO_BRIGHTNESS 125

// ============================================================
// NEOPIXEL COLORS PER STEP
// One row per sweep step, plus a final row for resting/drop
// Format: { R, G, B }
// Index 0 = first rise step (~35%)
// Index 1 = second rise step (~65%)
// Index 2 = third rise step (~85%)
// Index 3 = peak step (100%) — will flash
// Index 4 = resting/dropping — shown during drop back to zero
// ============================================================
const uint8_t STEP_COLORS[5][3] = {
  { 0,   255,   0 },   // step 0 ~35%:  green
  { 255, 200,   0 },   // step 1 ~65%:  yellow
  { 255, 100,   0 },   // step 2 ~85%:  amber
  { 255,   0,   0 },   // step 3 100%:  red (flashes)
  { 255, 255, 255 },   // resting/drop: white
};

// ============================================================
// NEOPIXEL CROSSFADE DURATION
// How long (ms) the color blends from one step color to the next
// Happens at the moment the needle settles into each pause
// ============================================================
const unsigned long NEO_FADE_MS = 400;

// ============================================================
// FLASHING RED TIMING (peak pause only)
// FLASH_ON_MS  = how long the red stays lit each flash
// FLASH_OFF_MS = how long it stays dark each flash
// ============================================================
const unsigned long FLASH_ON_MS  = 200;
const unsigned long FLASH_OFF_MS = 200;

// ============================================================
// NEEDLE SWEEP STEPS
// Each row defines one upward staircase step:
//   targetPosition: where the needle goes (0.0 = zero, 1.0 = full scale)
//   riseMs:         how long the needle takes to reach that position
//   pauseMs:        how long the needle holds at that position
//
// Add, remove, or reorder rows to change the staircase shape.
// Make sure STEP_COLORS above has a matching entry for each step
// (plus one extra for the resting/drop color at the end).
//
// Total cycle time ≈ sum of all (riseMs + pauseMs) + DROP_MS
// ============================================================
struct SweepStep {
  float targetPosition;
  unsigned long riseMs;
  unsigned long pauseMs;
};

SweepStep steps[] = {
  //  target   rise    pause
  {   0.18,    3000,    2000  },   // step 0: rise to target, pause for pause
  {   0.50,    1000,    2000  },   // step 1: rise to target, pause for pause
  {   0.70,    1000,    3000  },   // step 2: rise to target, pause for pause
  {   1.00,    1000,    4000 },   // step 3: rise to target, pause for pause
};

// ============================================================
// DROP DURATION
// How long (ms) the needle takes to fall back to zero
// after completing all steps
// ============================================================
const unsigned long DROP_MS = 6000;

// ============================================================
// RESTING PAUSE DURATION
// How long (ms) the needle holds at zero before starting
// the next cycle. Pixels stay white during this pause.
// ============================================================
const unsigned long REST_MS = 6000;

// --- Nothing below this line normally needs adjustment ---

const int NUM_STEPS = sizeof(steps) / sizeof(steps[0]);

// Number of color segments in the drop sequence (always NUM_STEPS colors: indices 3 down to 0, ending at 4)
// Drop goes: STEP_COLORS[3] → STEP_COLORS[2] → STEP_COLORS[1] → STEP_COLORS[0] → STEP_COLORS[4]
// That's NUM_STEPS segments total
const int DROP_COLOR_SEGMENTS = NUM_STEPS;

enum State { STATE_RISING, STATE_PAUSING, STATE_DROPPING, STATE_RESTING };

State currentState     = STATE_RISING;
int   currentStep      = 0;
unsigned long stateStartMs = 0;
float startPosition    = 0.0;
float currentPosition  = 0.0;
// Used during rise/pause crossfades
uint8_t neoFromColor[3] = { 255, 255, 255 };
uint8_t neoToColor[3]   = { 255, 255, 255 };
unsigned long neoFadeStartMs = 0;

bool flashOn = true;
unsigned long flashStartMs = 0;

// Stored so drop color interpolation knows where to start
float dropStartT = 0.0;  // not used directly, drop uses global elapsed

float smoothstep(float t) {
  if (t < 0.0) t = 0.0;
  if (t > 1.0) t = 1.0;
  return t * t * (3.0 - 2.0 * t);
}

float smoothLerp(float a, float b, float t) {
  return a + (b - a) * smoothstep(t);
}

void setNeedle(float position) {
  if (position < 0.0) position = 0.0;
  if (position > 1.0) position = 1.0;
  currentPosition = position;
  dac_output_voltage(DAC_CHANNEL_1, (uint8_t)(position * 255));
}

void setPixels(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < NUMPIXELS; i++)
    pixels.setPixelColor(i, pixels.Color(r, g, b));
  pixels.show();
}

void startFadeTo(int colorIndex) {
  memcpy(neoFromColor, neoToColor, 3);
  memcpy(neoToColor, STEP_COLORS[colorIndex], 3);
  neoFadeStartMs = millis();
}
// -------------------------------------------------------
// Get the drop color for a given progress t (0.0 - 1.0)
// Sequences through: red(3) → amber(2) → yellow(1) → green(0) → white(4)
// Each segment gets an equal slice of t
// -------------------------------------------------------
void getDropColor(float t, uint8_t &r, uint8_t &g, uint8_t &b) {
  // Color waypoints in order: red, amber, yellow, green, white
  const int waypoints[] = { 3, 2, 1, 0, 4 };
  const int NUM_WAYPOINTS = 5;
  const int NUM_SEGMENTS  = NUM_WAYPOINTS - 1;  // 4 segments

  // Which segment are we in?
  float scaled   = t * NUM_SEGMENTS;
  int   segment  = (int)scaled;
  if (segment >= NUM_SEGMENTS) segment = NUM_SEGMENTS - 1;
  float segT = scaled - segment;  // 0.0-1.0 within this segment

  // Smoothstep within each segment for organic feel
  segT = smoothstep(segT);

  const uint8_t *from = STEP_COLORS[waypoints[segment]];
  const uint8_t *to   = STEP_COLORS[waypoints[segment + 1]];

  r = (uint8_t)(from[0] + (to[0] - (int)from[0]) * segT);
  g = (uint8_t)(from[1] + (to[1] - (int)from[1]) * segT);
  b = (uint8_t)(from[2] + (to[2] - (int)from[2]) * segT);
}
void updateNeoPixels(unsigned long now) {

 // ====================================================
 // this block makes the step 3 (red) be solid.  If we want it
 // flashing, we will comment this out and uncomment the next block
 // ====================================================

  if (currentState == STATE_PAUSING && currentStep == NUM_STEPS - 1) {
  // Peak pause: keep solid red, no flashing
  setPixels(STEP_COLORS[3][0],
            STEP_COLORS[3][1],
            STEP_COLORS[3][2]);
  return;
}
// ====================================================
//  uncomment and use this block to make the step 3 (red) flash
// ====================================================
 // if (currentState == STATE_PAUSING && currentStep == NUM_STEPS - 1) {
  //  unsigned long flashElapsed = now - flashStartMs;
  //  unsigned long flashPeriod  = flashOn ? FLASH_ON_MS : FLASH_OFF_MS;
  //  if (flashElapsed >= flashPeriod) {
  //    flashOn = !flashOn;
  //    flashStartMs = now;
  //  }
  //  setPixels(flashOn ? STEP_COLORS[3][0] : 0,
  //            flashOn ? STEP_COLORS[3][1] : 0,
  //            flashOn ? STEP_COLORS[3][2] : 0);
  //  return;
  //  }
  // ====================================================


// Color cascade during drop
  if (currentState == STATE_DROPPING) {
    unsigned long elapsed = now - stateStartMs;
    float t = min(1.0f, (float)elapsed / (float)DROP_MS);
    uint8_t r, g, b;
    getDropColor(t, r, g, b);
    setPixels(r, g, b);
    return;
  }

  // Crossfade during rise and resting
  unsigned long fadeElapsed = now - neoFadeStartMs;
  float t = min(1.0f, (float)fadeElapsed / (float)NEO_FADE_MS);

  uint8_t r = (uint8_t)(neoFromColor[0] + (neoToColor[0] - (int)neoFromColor[0]) * t);
  uint8_t g = (uint8_t)(neoFromColor[1] + (neoToColor[1] - (int)neoFromColor[1]) * t);
  uint8_t b = (uint8_t)(neoFromColor[2] + (neoToColor[2] - (int)neoFromColor[2]) * t);

  setPixels(r, g, b);
}

void setup() {
  Serial.begin(115200);

  dac_output_enable(DAC_CHANNEL_1);

  pixels.begin();
  pixels.setBrightness(NEO_BRIGHTNESS);
  setPixels(255, 255, 255);

  memcpy(neoFromColor, STEP_COLORS[4], 3);
  memcpy(neoToColor,   STEP_COLORS[4], 3);

  setNeedle(0.0);
  stateStartMs  = millis();
  currentState  = STATE_RISING;
  currentStep   = 0;
  startPosition = 0.0;

  Serial.println("Steampunk gauge sweep starting...");
}

void loop() {
  unsigned long now     = millis();
  unsigned long elapsed = now - stateStartMs;

  switch (currentState) {

    case STATE_RISING: {
      float t = (float)elapsed / (float)steps[currentStep].riseMs;
      if (t >= 1.0) {
        setNeedle(steps[currentStep].targetPosition);
        currentState = STATE_PAUSING;
        stateStartMs = now;
        flashOn      = true;
        flashStartMs = now;
      } else {
        setNeedle(smoothLerp(startPosition, steps[currentStep].targetPosition, t));
      }
      break;
    }

    case STATE_PAUSING: {
      if (elapsed >= steps[currentStep].pauseMs) {
        currentStep++;
        if (currentStep >= NUM_STEPS) {
          startFadeTo(4);
          currentState  = STATE_DROPPING;
          startPosition = currentPosition;
          stateStartMs  = now;
        } else {
          startFadeTo(currentStep);
          currentState  = STATE_RISING;
          startPosition = currentPosition;
          stateStartMs  = now;
        }
      }
      break;
    }

    case STATE_DROPPING: {
      float t = (float)elapsed / (float)DROP_MS;
      if (t >= 1.0) {
        setNeedle(0.0);
         // Snap NeoPixels to white so resting state is clean
        memcpy(neoFromColor, STEP_COLORS[4], 3);
        memcpy(neoToColor,   STEP_COLORS[4], 3);
        currentState  = STATE_RESTING;  
        stateStartMs  = now;
      } else {
        setNeedle(smoothLerp(startPosition, 0.0, t));
      }
      break;
    }
     case STATE_RESTING: {
      if (elapsed >= REST_MS) {
        startFadeTo(0);          // begin fading toward green
        currentStep   = 0;
        startPosition = 0.0;
        currentState  = STATE_RISING;
        stateStartMs  = now;
        Serial.println("Cycle restart.");
      }
      break;
    }
  }

  updateNeoPixels(now);
}