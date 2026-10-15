/*
  ============================================================
  ESP32-C3 Super Mini
  Two LED filaments fading one at a time with PWM
  WITH DETAILED SERIAL DEBUG OUTPUT
  ============================================================
*/

// ------------------------------------------------------------
// GPIO PINS
// ------------------------------------------------------------
const int GREEN_PIN  = 4;
const int PURPLE_PIN = 3;

// ------------------------------------------------------------
// USER SETTINGS
// ------------------------------------------------------------
unsigned long DARK_TIME_MS      = 1500;
unsigned long FADE_UP_TIME_MS   = 15000;
unsigned long HOLD_ON_TIME_MS   = 15000;
unsigned long FADE_DOWN_TIME_MS = 15000;

int GREEN_MAX_BRIGHTNESS  = 80;
int PURPLE_MAX_BRIGHTNESS = 255;

int FADE_STEPS = 300;

// ------------------------------------------------------------
// PWM SETTINGS
// ------------------------------------------------------------
const int PWM_FREQUENCY  = 5000;
const int PWM_RESOLUTION = 8;

const int GREEN_CHANNEL  = 0;
const int PURPLE_CHANNEL = 1;

// ------------------------------------------------------------
// DEBUG SETTINGS
// ------------------------------------------------------------

// Print progress during fades every N steps.
// Example: 50 means print at step 0, 50, 100, 150, etc.
const int FADE_PRINT_INTERVAL = 50;

// ============================================================
// setup()
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("Two-filament sequence starting...");
  Serial.println("======================================");
  Serial.println();

  ledcSetup(GREEN_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcSetup(PURPLE_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);

  ledcAttachPin(GREEN_PIN, GREEN_CHANNEL);
  ledcAttachPin(PURPLE_PIN, PURPLE_CHANNEL);

  // Force both off at startup
  setBothFilaments(0, 0);

  printSettings();
}

// ============================================================
// loop()
// ============================================================
void loop() {

  // 1. Dark pause before green
  darkPause("DARK before GREEN", DARK_TIME_MS);

  // 2. Green fades up alone
  SerialStage("GREEN fade UP starting");
  setBothFilaments(0, 0);  // make absolutely sure purple is off
  fadeSingleFilament(GREEN_CHANNEL, 0, GREEN_MAX_BRIGHTNESS, FADE_UP_TIME_MS, "GREEN");

  // 3. Green holds alone
  SerialStage("GREEN hold ON starting");
  setBothFilaments(GREEN_MAX_BRIGHTNESS, 0);
  holdWithCountdown("GREEN hold", HOLD_ON_TIME_MS);

  // 4. Green fades down alone
  SerialStage("GREEN fade DOWN starting");
  fadeSingleFilament(GREEN_CHANNEL, GREEN_MAX_BRIGHTNESS, 0, FADE_DOWN_TIME_MS, "GREEN");

  // Make absolutely sure both are off
  setBothFilaments(0, 0);
  SerialStage("GREEN section complete; both should now be OFF");

  // 5. Dark pause before purple
  darkPause("DARK before PURPLE", DARK_TIME_MS);

  // 6. Purple fades up alone
  SerialStage("PURPLE fade UP starting");
  setBothFilaments(0, 0);  // make absolutely sure green is off
  fadeSingleFilament(PURPLE_CHANNEL, 0, PURPLE_MAX_BRIGHTNESS, FADE_UP_TIME_MS, "PURPLE");

  // 7. Purple holds alone
  SerialStage("PURPLE hold ON starting");
  setBothFilaments(0, PURPLE_MAX_BRIGHTNESS);
  holdWithCountdown("PURPLE hold", HOLD_ON_TIME_MS);

  // 8. Purple fades down alone
  SerialStage("PURPLE fade DOWN starting");
  fadeSingleFilament(PURPLE_CHANNEL, PURPLE_MAX_BRIGHTNESS, 0, FADE_DOWN_TIME_MS, "PURPLE");

  // Make absolutely sure both are off before repeating
  setBothFilaments(0, 0);
  SerialStage("PURPLE section complete; both should now be OFF");

  SerialStage("FULL CYCLE COMPLETE - restarting");
  Serial.println();
}

// ============================================================
// printSettings()
// ============================================================
void printSettings() {
  Serial.println("Current settings:");
  Serial.print("  DARK_TIME_MS            = "); Serial.println(DARK_TIME_MS);
  Serial.print("  FADE_UP_TIME_MS         = "); Serial.println(FADE_UP_TIME_MS);
  Serial.print("  HOLD_ON_TIME_MS         = "); Serial.println(HOLD_ON_TIME_MS);
  Serial.print("  FADE_DOWN_TIME_MS       = "); Serial.println(FADE_DOWN_TIME_MS);
  Serial.print("  GREEN_MAX_BRIGHTNESS    = "); Serial.println(GREEN_MAX_BRIGHTNESS);
  Serial.print("  PURPLE_MAX_BRIGHTNESS   = "); Serial.println(PURPLE_MAX_BRIGHTNESS);
  Serial.print("  FADE_STEPS              = "); Serial.println(FADE_STEPS);
  Serial.println();
}

// ============================================================
// SerialStage()
// Prints a timestamped stage message
// ============================================================
void SerialStage(const char* message) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

// ============================================================
// setBothFilaments()
// ============================================================
void setBothFilaments(int greenValue, int purpleValue) {
  greenValue  = constrain(greenValue,  0, 255);
  purpleValue = constrain(purpleValue, 0, 255);

  ledcWrite(GREEN_CHANNEL, greenValue);
  ledcWrite(PURPLE_CHANNEL, purpleValue);

  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] setBothFilaments -> Green=");
  Serial.print(greenValue);
  Serial.print("  Purple=");
  Serial.println(purpleValue);
}

// ============================================================
// darkPause()
// Turns both off, then waits while printing a 1-second countdown
// ============================================================
void darkPause(const char* label, unsigned long pauseMs) {
  SerialStage(label);
  setBothFilaments(0, 0);

  unsigned long startTime = millis();
  unsigned long lastPrintSecond = 999999;

  while (millis() - startTime < pauseMs) {
    unsigned long elapsed = millis() - startTime;
    unsigned long remainingMs = pauseMs - elapsed;
    unsigned long remainingSec = (remainingMs + 999) / 1000;  // round up

    if (remainingSec != lastPrintSecond) {
      Serial.print("[");
      Serial.print(millis());
      Serial.print(" ms] ");
      Serial.print(label);
      Serial.print(" ... ");
      Serial.print(remainingSec);
      Serial.println(" second(s) remaining");
      lastPrintSecond = remainingSec;
    }

    delay(50);
  }

  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.print(label);
  Serial.println(" complete");
}

// ============================================================
// holdWithCountdown()
// Holds the current state while printing once per second
// ============================================================
void holdWithCountdown(const char* label, unsigned long holdMs) {
  unsigned long startTime = millis();
  unsigned long lastPrintSecond = 999999;

  while (millis() - startTime < holdMs) {
    unsigned long elapsed = millis() - startTime;
    unsigned long remainingMs = holdMs - elapsed;
    unsigned long remainingSec = (remainingMs + 999) / 1000;  // round up

    if (remainingSec != lastPrintSecond) {
      Serial.print("[");
      Serial.print(millis());
      Serial.print(" ms] ");
      Serial.print(label);
      Serial.print(" ... ");
      Serial.print(remainingSec);
      Serial.println(" second(s) remaining");
      lastPrintSecond = remainingSec;
    }

    delay(50);
  }

  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.print(label);
  Serial.println(" complete");
}

// ============================================================
// fadeSingleFilament()
// ============================================================
void fadeSingleFilament(int channel, int startValue, int endValue, unsigned long fadeTimeMs, const char* label) {

  startValue = constrain(startValue, 0, 255);
  endValue   = constrain(endValue,   0, 255);

  float stepDelayMs = (float)fadeTimeMs / (float)FADE_STEPS;

  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.print(label);
  Serial.print(" fade starting: ");
  Serial.print(startValue);
  Serial.print(" -> ");
  Serial.print(endValue);
  Serial.print(" over ");
  Serial.print(fadeTimeMs);
  Serial.println(" ms");

  for (int step = 0; step <= FADE_STEPS; step++) {
    float progress = (float)step / (float)FADE_STEPS;
    int currentValue = startValue + (endValue - startValue) * progress;

    ledcWrite(channel, currentValue);

    if ((step % FADE_PRINT_INTERVAL == 0) || (step == FADE_STEPS)) {
      Serial.print("[");
      Serial.print(millis());
      Serial.print(" ms] ");
      Serial.print(label);
      Serial.print(" fade step ");
      Serial.print(step);
      Serial.print("/");
      Serial.print(FADE_STEPS);
      Serial.print("  PWM=");
      Serial.println(currentValue);
    }

    delay((unsigned long)stepDelayMs);
  }

  Serial.print("[");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.print(label);
  Serial.println(" fade complete");
}