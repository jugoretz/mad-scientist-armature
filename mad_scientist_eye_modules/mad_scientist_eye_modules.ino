/*
Based heavily (REALLY heavily) on the great Bodmer's Animated_Eyes
https://github.com/Bodmer/TFT_eSPI/blob/master/examples/Generic/Animated_Eyes_1/Animated_Eyes_1.ino
this is what runs the eye modules on Mad Scientist Armature.
We use 240x240 round displays. We can choose different eye designs in config.h
*/

// An adaptation of the "UncannyEyes" sketch (see eye_functions tab)
// for the TFT_eSPI library. As written the sketch is for driving
// two TFT displays.

// The size of the displayed eye is determined by the screen size and
// resolution. The eye image is 128 pixels wide. In humans the palpebral
// fissure (open eye) width is about 30mm so a low resolution, large
// pixel size display works best to show a scale eye image. Note that
// display manufacturers usually quote the diagonal measurement, so a
// 128 x 128 1.7" display or 128 x 160 2" display is about right.

// The number of displays and chip selects used are defined in the
// config.h tab. The display count can be set to 1. If using one
// TFT and the chip select for that display is already defined in
// the TFT_eSPI library then change the chip select pins to -1 in the
// "config.h" tab.

// The wiring for 2 TFT displays to an ESP32 is described in the
// "wiring" tab of this sketch.

// Configuration settings for the eye, eye style, display count,
// chip selects and x offsets can be defined in the sketch "config.h" tab.

// Performance (frames per second = fps) can be improved by using
// DMA (for SPI displays only) on ESP32 and STM32 processors. Use
// as high a SPI clock rate as is supported by the display. 27MHz
// minimum, some displays can be operated at higher clock rates in
// the range 40-80MHz.

// Single defaultEye performance for different processors
//                                  No DMA   With DMA
// ESP8266 (160MHz CPU) 40MHz SPI   36 fps
// ESP32 27MHz SPI                  53 fps     85 fps
// ESP32 40MHz SPI                  67 fps    102 fps
// ESP32 80MHz SPI                  82 fps    116 fps // Note: Few displays work reliably at 80MHz
// STM32F401 55MHz SPI              44 fps     90 fps
// STM32F446 55MHz SPI              83 fps    155 fps
// STM32F767 55MHz SPI             136 fps    197 fps

// DMA can be used with RP2040, STM32 and ESP32 processors when the interface
// is SPI, uncomment the next line:
//#define USE_DMA

// Load TFT driver library
#include <SPI.h>
#include <TFT_eSPI.h>
TFT_eSPI tft;           // A single instance is used for 1 or 2 displays

//add a neopixel indicator light
#include <Adafruit_NeoPixel.h>
#include <math.h>

// NeoPixel setup
#define NEOPIXEL_PIN 10     // adjust to your pin
#define NUMPIXELS    2      // adjust to as many as we need
Adafruit_NeoPixel pixel(NUMPIXELS, NEOPIXEL_PIN, NEO_GRBW + NEO_KHZ800);

// A pixel buffer is used during eye rendering
#define BUFFER_SIZE 1024 // 128 to 1024 seems optimum

#ifdef USE_DMA
  #define BUFFERS 2      // 2 toggle buffers with DMA
#else
  #define BUFFERS 1      // 1 buffer for no DMA
#endif

uint16_t pbuffer[BUFFERS][BUFFER_SIZE]; // Pixel rendering buffer
bool     dmaBuf   = 0;                  // DMA buffer selection
// Actual display resolution for GC9A01
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240
// This struct is populated in config.h
typedef struct {        // Struct is defined before including config.h --
  int8_t  select;       // pin numbers for each eye's screen select line
  int8_t  wink;         // and wink button (or -1 if none) specified there,
  uint8_t rotation;     // also display rotation and the x offset
  int16_t xposition;    // position of eye on the screen
  int16_t yposition;   // new vertical offset
} eyeInfo_t;

#include "config.h"     // ****** CONFIGURATION IS DONE IN HERE ******

extern void user_setup(void); // Functions in the user*.cpp files
extern void user_loop(void);

#define SCREEN_X_START 0
#define SCREEN_X_END   SCREEN_WIDTH   // Badly named, actually the *eye* width!
#define SCREEN_Y_START 0
#define SCREEN_Y_END   SCREEN_HEIGHT  // Actually *eye* height

// A simple state machine is used to control eye blinks/winks:
#define NOBLINK 0       // Not currently engaged in a blink
#define ENBLINK 1       // Eyelid is currently closing
#define DEBLINK 2       // Eyelid is currently opening
typedef struct {
  uint8_t  state;       // NOBLINK/ENBLINK/DEBLINK
  uint32_t duration;    // Duration of blink state (micros)
  uint32_t startTime;   // Time (micros) of last state change
} eyeBlink;

struct {                // One-per-eye structure
  int16_t   tft_cs;     // Chip select pin for each display
  eyeBlink  blink;      // Current blink/wink state
  int16_t   xposition;  // x position of eye image
} eye[NUM_EYES];

uint32_t startTime;  // For FPS indicator


unsigned long neoStartTime = 0;
int colorIndex = 0;

void updateNeoPixel() {
  const unsigned long FADE_DURATION = 30000;  // 30 seconds to fade between colors
  const uint8_t PALETTE[][3] = {  //that [] means that we will automatically grab the number of colors from how many are listed below.  Specify there instead if that doesn't work
  {255,   0,   0},   // red
  {9, 255, 0},    //eerie green 
   {180,   0, 255},   // purple
  {255,  50,   0},   // orange not bad
  {0, 0, 255},    //blue
  {255, 120,   0},   // amber a good yellow
    {  0, 255,   0},   // green
  {255, 255,   0},   // yellow but rather greenish
    {144, 0, 255},  //a different deep purple that is better and deeper
     {255, 0, 0},    //red again but the web calls it bloody red 
  {  0, 100, 255},   // blue-ish
    {128, 0, 128}
  };
  const int N = sizeof(PALETTE) / sizeof(PALETTE[0]);  // automatically matches however many colors we define above

  unsigned long now = millis();
  unsigned long elapsed = now - neoStartTime;

  // Check if fade is complete; advance to next color
  if (elapsed >= FADE_DURATION) {
    neoStartTime = now;
    colorIndex = (colorIndex + 1) % N;
    elapsed = 0;  // reset elapsed for smooth transition
    Serial.printf("finsihed switch to color %d: RGB(%d,%d,%d)\n",
                  colorIndex,
                  PALETTE[colorIndex][0],
                  PALETTE[colorIndex][1],
                  PALETTE[colorIndex][2]);
  }

  // Calculate fade progress as a fraction from 0.0 to 1.0
  float progress = (float)elapsed / FADE_DURATION;

  // Get the next color index (wraps around)
  int nextColorIndex = (colorIndex + 1) % N;

  // Linear interpolation: blend current color toward next color
  uint8_t r = (uint8_t)(PALETTE[colorIndex][0] + 
                        (PALETTE[nextColorIndex][0] - PALETTE[colorIndex][0]) * progress);
  uint8_t g = (uint8_t)(PALETTE[colorIndex][1] + 
                        (PALETTE[nextColorIndex][1] - PALETTE[colorIndex][1]) * progress);
  uint8_t b = (uint8_t)(PALETTE[colorIndex][2] + 
                        (PALETTE[nextColorIndex][2] - PALETTE[colorIndex][2]) * progress);

  for (int i = 0; i < NUMPIXELS; i++) {
  pixel.setPixelColor(i, pixel.Color(r, g, b));
}
pixel.show();
}
void neoTask(void *parameter) {
  while (true) {
    updateNeoPixel();
    vTaskDelay(pdMS_TO_TICKS(10));  // about 50 updates/sec
  }
}
// INITIALIZATION -- runs once at startup ----------------------------------
void setup(void) {

  Serial.begin(115200);
  //while (!Serial);
  Serial.println("Starting");

#if defined(DISPLAY_BACKLIGHT) && (DISPLAY_BACKLIGHT >= 0)
  // Enable backlight pin, initially off
  Serial.println("Backlight turned off");
  pinMode(DISPLAY_BACKLIGHT, OUTPUT);
  digitalWrite(DISPLAY_BACKLIGHT, LOW);
#endif

  // User call for additional features
  user_setup();

  // Initialise the eye(s), this will set all chip selects low for the tft.init()
  initEyes();

  // Initialise TFT
  Serial.println("Initialising displays");
  tft.init();

  
#ifdef USE_DMA
  tft.initDMA();
#endif

  // Raise chip select(s) so that displays can be individually configured
  digitalWrite(eye[0].tft_cs, HIGH);
  if (NUM_EYES > 1) digitalWrite(eye[1].tft_cs, HIGH);

  for (uint8_t e = 0; e < NUM_EYES; e++) {
    digitalWrite(eye[e].tft_cs, LOW);
    tft.setRotation(eyeInfo[e].rotation);
    tft.fillScreen(TFT_BLACK);
    digitalWrite(eye[e].tft_cs, HIGH);
  }
// Center the 200x200 eye texture inside the 240x240 round display
//tft.setViewport((SCREEN_WIDTH - 200) / 2, (SCREEN_HEIGHT - 200) / 2, 200, 200);
//Serial.println("Viewport set for 200x200 eye on 240x240 display");
#if defined(DISPLAY_BACKLIGHT) && (DISPLAY_BACKLIGHT >= 0)
  Serial.println("Backlight now on!");
  analogWrite(DISPLAY_BACKLIGHT, BACKLIGHT_MAX);
#endif

  startTime = millis(); // For frame-rate calculation
  pixel.begin();
pixel.setBrightness(40);
pixel.show();
Serial.println("NeoPixel fade effect initialized");
neoStartTime = millis();

  // Start NeoPixel task on core 0
  xTaskCreatePinnedToCore(
    neoTask,          // task function
    "NeoTask",        // name
    4096,             // stack size
    NULL,             // parameter
    1,                // priority
    NULL,             // task handle
    0                 // core
  );
}

// MAIN LOOP -- runs continuously after setup() ----------------------------
void loop() {
  updateEye();      // your main eye animation
  
}
