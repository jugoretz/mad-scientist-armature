/*
  Halloween Ultrasonic + MP3 Playlist Prototype
  ==============================================

  Hardware:
    - ESP32-C3
    - HC-SR04 ultrasonic distance sensor
    - MicroSD card module
    - MAX98357A I2S amplifier
    - 3-watt, 8-ohm speaker (GRS 3FR-4 Full-Range 3" Speaker Driver 4 Ohm)

  Software:
    - Arduino-ESP32 3.x
    - Current ESP8266Audio library

  Wifi tuning mode: enable or disable below.  When it's on, we can see
      * Latest measured distance
      * Whether that distance is inside the active zone
      * Current state
      * Dwell progress
      * Clear-to-rearm progress
      * Active playlist and track
      * Next playlist
      * Uptime
    Using this info:
        Network: MadScientist-Tuning
        Password: raven2026
        Page: http://192.168.4.1

  Behavior:
    1. Measure distance five times per second.
    2. Detect a target between 2 cm and 100 cm.
    3. Require the target to remain in the viewing zone for five seconds.
    4. Play the next sound playlist.
    5. Play every sound in that playlist in order.
    6. Do not take ultrasonic measurements during playlist playback.
    7. After playback, wait until the viewing zone has remained
       clear for two seconds.
    8. Re-arm for the next visitor.
    9. Advance sequentially through all the playlists, 
        in the order defined by const char* const* const playlists[],
        then start over again.  ( be sure that the order in 
        const char* const* const playlists[] exactly matches the order in
        const uint8_t playlistLengths[] )


  ------------------------------------------------------------
  WIRING
  ------------------------------------------------------------

  HC-SR04:

    VCC   -> 5V NOTE: 5v is what we want to be for this, not 3v3
    GND   -> GND
    TRIG  -> GPIO 4
    ECHO  -> GPIO 5 through voltage divider

  ECHO voltage divider:

    HC-SR04 ECHO
         |
       1k resistor
         |
         +---------- GPIO 5
         |
       2k resistor
         |
        GND


  MAX98357A:

    VIN   -> 5V
    GND   -> GND
    BCLK  -> GPIO 0
    LRC   -> GPIO 1
    DIN   -> GPIO 3
    SD    -> 3.3V
    GAIN  -> unconnected

    Speaker connects between SPK+ and SPK-.
    Neither speaker output connects to ground.


  MicroSD module:

    SCK   -> GPIO 6
    MISO  -> GPIO 7
    MOSI  -> GPIO 8
    CS    -> GPIO 10
    GND   -> GND

    Power:
      Use 3.3V for the 3.3V SD module.

  IMPORTANT:
    ESP32-C3, ultrasonic sensor, SD module, and amplifier must
    all share a common ground.
*/

// ===========================================================================
// Wi-Fi tuning mode
// ===========================================================================

/*
  Set to 1 for  field tuning.

  Set to 0 for the final production firmware. When disabled, the Wi-Fi
  code and web server are omitted completely.
*/
#define ENABLE_WIFI_TUNING 1

// ===========================================================================
// Includes
// ===========================================================================

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

#if ENABLE_WIFI_TUNING
#include <WiFi.h>
#include <WebServer.h>
#endif

// ===========================================================================
// Pin assignments
// ===========================================================================

// HC-SR04
const int ULTRASONIC_TRIG_PIN = 4;
const int ULTRASONIC_ECHO_PIN = 5;

// MicroSD SPI
const int SD_SCK_PIN  = 6;
const int SD_MISO_PIN = 7;
const int SD_MOSI_PIN = 8;
const int SD_CS_PIN   = 10;

// MAX98357A I2S
const int I2S_BCLK_PIN = 0;
const int I2S_LRC_PIN  = 1;
const int I2S_DATA_PIN = 3;

// ===========================================================================
// Wi-Fi tuning-page settings
// ===========================================================================

#if ENABLE_WIFI_TUNING

/*
  The ESP32 creates this private local network.

  The password must contain at least eight characters.
*/
const char* const TUNING_WIFI_NAME =
    "YOUR WIFI NAME HERE";

const char* const TUNING_WIFI_PASSWORD =
    "YOUR PASSWORD HERE";

/*
  Standard HTTP server on port 80.
*/
WebServer tuningServer(80);

#endif

// ===========================================================================
// Latest sensor status for the tuning page
// ===========================================================================

/*
  These duplicate the latest completed sensor reading so the webpage
  can display it without initiating extra ultrasonic measurements.
*/
float latestDistanceCm = NAN;
bool latestInViewingZone = false;
unsigned long latestDistanceReadingAt = 0;

// ===========================================================================
// Detection settings
// ===========================================================================

/*
  Practical near limit of the HC-SR04.
*/
const float MIN_VIEWING_DISTANCE_CM = 2.0;

/*
  Initial outdoor viewing-zone limit.

  This can be adjusted during testing at the actual installation site.
*/
const float MAX_VIEWING_DISTANCE_CM = 100.0;

/*
  A target must remain associated with the viewing zone for five
  seconds before it counts as a viewer.
*/
const unsigned long VIEWER_DWELL_TIME_MS = 5000;

/*
  Permit a few isolated poor readings without immediately canceling
  a possible viewer.

  At five readings per second, 600 ms is roughly three sample periods.
*/
const unsigned long ALLOWED_READING_GAP_MS = 600;

/*
  After the playlist finishes, require the viewing zone to remain
  clear for two seconds before rearming.

  This prevents the same person from repeatedly triggering the display.
*/
const unsigned long CLEAR_TO_REARM_TIME_MS = 2000;

/*
  Five ultrasonic readings per second.
*/
const unsigned long SENSOR_INTERVAL_MS = 200;


// ===========================================================================
// Ultrasonic timeout (distance limiting)
// ===========================================================================

/*
  We care only about targets within 100 cm.
  but we do want to see just how far targets are if they're out of range.
  Echo timeout is an easy way to let us see what we want to see.  
  Roughly, a 10,000 ms timeout is good out to 130 cm
  25,0000 should be roughly 400 cm
  and 30,000 should be good out to maybe 500 cm 
  we do want to avoid an unnecessary 
  long wait when no useful echo returns.
*/
const unsigned long ECHO_TIMEOUT_US = 30000;


// ===========================================================================
// Audio settings
// ===========================================================================

/*
  Software gain range is nominally 0.0 to 1.0.

  0.60 is the current test setting. The final value can be adjusted
  after testing the speaker outdoors and inside its enclosure.
*/
const float AUDIO_GAIN = 0.60;


// ===========================================================================
// Sound playlists
// ===========================================================================

/*
  Each playlist is one complete performance.

  Filenames begin with "/" because the files are stored in the
  root directory of the SD card.

  To add a playlist:
    1. Copy the MP3 files onto the SD card.
    2. Create a new
         const char* const playlistX[]
    3. Add playlistX to playlists[]
    4. Add its sizeof(...) entry to playlistLengths[]
    NUM_PLAYLISTS updates automatically.
*/

const char* const playlist0[] = {
  "/raven_2.mp3",
  "/music_1.mp3",
  "/laugh_1.mp3",
  "/raven_3.mp3"
};

const char* const playlist1[] = {
  "/raven_3.mp3",
  "/laugh_2.mp3",
  "/music_1.mp3",
  "/raven_2.mp3"
};

const char* const playlist2[] = {
  "/raven_2.mp3",
  "/laugh_2.mp3",
  "/music_1.mp3",
  "/raven_3.mp3"
};

const char* const playlist3[] = {
  "/raven_3.mp3",
  "/laugh_1.mp3",
  "/laugh_2.mp3",
  "/raven_2.mp3"
};

const char* const playlist4[] = {
  "/raven_2.mp3",
  "/raven_1.mp3",
  "/music_1.mp3",
  "/laugh_2.mp3",
  "/raven_1.mp3"
};

const char* const playlist5[] = {
  "/raven_2.mp3",
  "/monstermash1.mp3",
  "/raven_1.mp3"
};

const char* const playlist6[] = {
  "/raven_3.mp3",
  "/monstermash2.mp3",
  "/raven_2.mp3"
};

const char* const playlist7[] = {
  "/raven_2.mp3",
  "/monstermash3.mp3",
  "/raven_3.mp3"
};

const char* const playlist8[] = {
  "/raven_3.mp3",
  "/monstermash4.mp3",
  "/raven_2.mp3"
};

/*
  Table containing pointers to all the playlists. this is the order through
  which we will proceed in playing playlists
*/
const char* const* const playlists[] = {
  playlist0,
  playlist5,
  playlist1,
  playlist6,
  playlist2,
  playlist7,
  playlist3,
  playlist8,
  playlist4
};


/*
  Number of tracks in each playlist. Make sure the order here exactly matches
  the order above.
*/
const uint8_t playlistLengths[] = {
  sizeof(playlist0) / sizeof(playlist0[0]),
  sizeof(playlist5) / sizeof(playlist5[0]),
  sizeof(playlist1) / sizeof(playlist1[0]),
  sizeof(playlist6) / sizeof(playlist6[0]),
  sizeof(playlist2) / sizeof(playlist2[0]),
  sizeof(playlist7) / sizeof(playlist7[0]),
  sizeof(playlist3) / sizeof(playlist3[0]),
  sizeof(playlist8) / sizeof(playlist8[0]),
  sizeof(playlist4) / sizeof(playlist4[0])
};


/*
  Total number of playlists.
*/
const uint8_t NUM_PLAYLISTS =
    sizeof(playlists) / sizeof(playlists[0]);

/*
  Pause inserted between successive MP3 files within a playlist.
*/

const unsigned long TRACK_PAUSE_MS = 500;

bool waitingBetweenTracks = false;

unsigned long trackPauseStartedAt = 0;
// ===========================================================================
// Playlist progress
// ===========================================================================

/*
  Playlist that will be used by the next confirmed visitor.

  This begins at zero internally, which corresponds to displayed
  playlist number 1.
*/
uint8_t nextPlaylist = 0;

/*
  Playlist currently playing.
*/
uint8_t activePlaylist = 0;

/*
  Track currently playing within the active playlist.
*/
uint8_t currentTrack = 0;


// ===========================================================================
// Main operating states
// ===========================================================================

enum SystemState {
  WAITING_FOR_VISITOR,
  POSSIBLE_VISITOR,
  PLAYING_SOUND,
  WAITING_FOR_CLEAR
};

SystemState systemState = WAITING_FOR_VISITOR;


// ===========================================================================
// Timing variables
// ===========================================================================

unsigned long lastSensorReadingAt = 0;

/*
  Time at which the possible visitor first entered the zone.
*/
unsigned long visitorCandidateStartedAt = 0;

/*
  Most recent reading that placed the visitor inside the zone.
*/
unsigned long lastInZoneAt = 0;

/*
  Beginning of the sustained clear period after playback.
*/
unsigned long clearPeriodStartedAt = 0;

/*
  True only while the clear-period timer is actively running.
*/
bool clearTimerRunning = false;


// ===========================================================================
// Audio objects
// ===========================================================================

/*
  Keep the I2S output object alive for the entire sketch.

  The MP3 decoder and SD file-source objects are created and destroyed
  separately for each track.
*/
AudioOutputI2S* audioOutput = nullptr;
AudioFileSourceSD* audioFile = nullptr;
AudioGeneratorMP3* mp3 = nullptr;


// ===========================================================================
// Function declarations
// ===========================================================================

float measureDistanceCm();

bool readingIsInViewingZone(float distanceCm);

void processWaitingForVisitor(
    unsigned long now,
    bool inViewingZone
);

void processPossibleVisitor(
    unsigned long now,
    bool inViewingZone
);

void processAudioPlayback();

void processWaitingForClear(
    unsigned long now,
    bool inViewingZone
);

bool startNextPlaylist();
 
bool startCurrentTrack();

bool startSound(const char* filename);

void finishPlaylist(unsigned long now);

void handlePlaylistFailure(unsigned long now);

void stopAndReleaseSound();

bool verifyPlaylistFiles();

void printDistanceReading(
    unsigned long now,
    float distanceCm,
    bool inViewingZone
);

void printEvent(
    unsigned long now,
    const char* message
);

const char* systemStateName(); //this is for the wifi tuning if it's enabled

#if ENABLE_WIFI_TUNING

void startWifiTuning();

void handleTuningHomePage();

void handleTuningStatus();

#endif

// ===========================================================================
// Setup
// ===========================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  latestDistanceReadingAt = millis();
  Serial.println();
  Serial.println("Halloween ultrasonic + MP3 playlist prototype");
  Serial.println("----------------------------------------------");

  // -------------------------------------------------------------------------
  // Configure HC-SR04 pins
  // -------------------------------------------------------------------------

  pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
  pinMode(ULTRASONIC_ECHO_PIN, INPUT);

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);


  // -------------------------------------------------------------------------
  // Start the SD-card SPI bus
  // -------------------------------------------------------------------------

  SPI.begin(
    SD_SCK_PIN,
    SD_MISO_PIN,
    SD_MOSI_PIN,
    SD_CS_PIN
  );

  Serial.print("Mounting SD card... ");

  /*
    trying 20 mhz to get better sound. if we have trouble mounting SD,
    we can step it down a bit.
  */
  if (!SD.begin(SD_CS_PIN, SPI, 20000000)) {
    Serial.println("FAILED");
    Serial.println();
    Serial.println("Check SD power, SPI wiring, card formatting,");
    Serial.println("chip-select pin, and common ground.");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("success.");


  // -------------------------------------------------------------------------
  // Verify every sound used by the playlists
  // -------------------------------------------------------------------------

  Serial.println();
  Serial.println("Checking playlist files...");

  if (!verifyPlaylistFiles()) {
    Serial.println();
    Serial.println("One or more playlist files are missing.");
    Serial.println("Correct the SD-card contents and restart.");

    while (true) {
      delay(1000);
    }
  }

  Serial.println("All playlist files found.");


  // -------------------------------------------------------------------------
  // Configure permanent I2S output
  // -------------------------------------------------------------------------

  audioOutput = new AudioOutputI2S();

  if (audioOutput == nullptr) {
    Serial.println("ERROR: Could not allocate I2S output.");

    while (true) {
      delay(1000);
    }
  }

  /*
    SetPinout order:
      BCLK
      LRC / word select
      data output
  */
  if (!audioOutput->SetPinout(
        I2S_BCLK_PIN,
        I2S_LRC_PIN,
        I2S_DATA_PIN
      )) {

    Serial.println("ERROR: Could not configure I2S pins.");

    while (true) {
      delay(1000);
    }
  }

  audioOutput->SetGain(AUDIO_GAIN);

  // -------------------------------------------------------------------------
  // Show active configuration
  // -------------------------------------------------------------------------

  Serial.println();
  Serial.println("Configuration:");

  Serial.print("  Viewing zone:       ");
  Serial.print(MIN_VIEWING_DISTANCE_CM, 1);
  Serial.print(" to ");
  Serial.print(MAX_VIEWING_DISTANCE_CM, 1);
  Serial.println(" cm");

  Serial.print("  Viewer dwell time:  ");
  Serial.print(VIEWER_DWELL_TIME_MS / 1000.0, 1);
  Serial.println(" seconds");

  Serial.print("  Clear-to-rearm:     ");
  Serial.print(CLEAR_TO_REARM_TIME_MS / 1000.0, 1);
  Serial.println(" seconds");

  Serial.print("  Sensor rate:        ");
  Serial.print(1000 / SENSOR_INTERVAL_MS);
  Serial.println(" readings per second");

  Serial.print("  Audio gain:         ");
  Serial.println(AUDIO_GAIN, 2);

  Serial.print("  Number of playlists: ");
  Serial.println(NUM_PLAYLISTS);

  Serial.println();
  Serial.println("Next trigger will play playlist 1.");
  Serial.println("System armed.");
  Serial.println();

  #if ENABLE_WIFI_TUNING
  startWifiTuning();
  #endif
}

// ===========================================================================
// Main loop
// ===========================================================================

void loop() {
  /*
    During ordinary sensor operation, service requests from the
    Wi-Fi tuning page.
    
    During MP3 playback we deliberately stop servicing the webpage.
    This preserves the audio-first behavior that has already proven
    reliable. The browser automatically reconnects after playback.
  */

#if ENABLE_WIFI_TUNING
  if (systemState != PLAYING_SOUND) {
    tuningServer.handleClient();
  }
#endif
  /*
    Audio playback receives absolute priority.
    ESP8266Audio requires mp3->loop() to be called repeatedly and
    quickly while decoding. Ultrasonic readings are therefore paused
    for the entire playlist.
  */
  if (systemState == PLAYING_SOUND) {
    processAudioPlayback();
    return;
  }

  const unsigned long now = millis();

  /*
    Take one ultrasonic reading every 200 ms.

    No delay() is used for the sample interval.
  */
  if (now - lastSensorReadingAt < SENSOR_INTERVAL_MS) {
    return;
  }

  lastSensorReadingAt = now;

const float distanceCm = measureDistanceCm();

const bool inViewingZone =
    readingIsInViewingZone(distanceCm);

/*
  Preserve this completed measurement for the tuning webpage.
*/
latestDistanceCm = distanceCm;
latestInViewingZone = inViewingZone;
latestDistanceReadingAt = now;

printDistanceReading(
  now,
  distanceCm,
  inViewingZone
);

  /*
    Process the current operating state.
  */
  switch (systemState) {

    case WAITING_FOR_VISITOR:
      processWaitingForVisitor(
        now,
        inViewingZone
      );
      break;


    case POSSIBLE_VISITOR:
      processPossibleVisitor(
        now,
        inViewingZone
      );
      break;


    case WAITING_FOR_CLEAR:
      processWaitingForClear(
        now,
        inViewingZone
      );
      break;


    case PLAYING_SOUND:
      /*
        PLAYING_SOUND is handled at the beginning of loop().
      */
      break;
  }
}


// ===========================================================================
// Measure distance with HC-SR04
// ===========================================================================

float measureDistanceCm() {
  /*
    Generate the required 10-microsecond trigger pulse.
  */
  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
  delayMicroseconds(10);

  digitalWrite(ULTRASONIC_TRIG_PIN, LOW);


  /*
    Measure the returned echo pulse.

    pulseIn() is technically blocking, but only until the echo
    completes or the short timeout expires.
  */
  const unsigned long echoDurationUs =
      pulseIn(
        ULTRASONIC_ECHO_PIN,
        HIGH,
        ECHO_TIMEOUT_US
      );

  if (echoDurationUs == 0) {
    return NAN;
  }

  /*
    Convert round-trip travel time to one-way distance in centimeters.
  */
  return echoDurationUs / 58.0;
}


// ===========================================================================
// Determine whether a reading occupies the viewing zone
// ===========================================================================

bool readingIsInViewingZone(const float distanceCm) {
  if (isnan(distanceCm)) {
    return false;
  }

  return
      distanceCm >= MIN_VIEWING_DISTANCE_CM &&
      distanceCm <= MAX_VIEWING_DISTANCE_CM;
}


// ===========================================================================
// State: waiting for someone to enter
// ===========================================================================

void processWaitingForVisitor(
    const unsigned long now,
    const bool inViewingZone) {

  if (!inViewingZone) {
    return;
  }

  visitorCandidateStartedAt = now;
  lastInZoneAt = now;

  systemState = POSSIBLE_VISITOR;

  printEvent(
    now,
    "TARGET ENTERED VIEWING ZONE"
  );

  printEvent(
    now,
    "FIVE-SECOND DWELL TIMER STARTED"
  );
}


// ===========================================================================
// State: possible visitor, dwell timer running
// ===========================================================================

void processPossibleVisitor(
    const unsigned long now,
    const bool inViewingZone) {

  if (inViewingZone) {
    /*
      Record the most recent good in-zone reading.
    */
    lastInZoneAt = now;

    const unsigned long elapsedDwellTime =
        now - visitorCandidateStartedAt;

    if (elapsedDwellTime >= VIEWER_DWELL_TIME_MS) {
      printEvent(
        now,
        "VIEWER CONFIRMED"
      );

      printEvent(
        now,
        "STARTING PLAYLIST"
      );

      if (startNextPlaylist()) {
        systemState = PLAYING_SOUND;
      } else {
        /*
          If playback fails, do not immediately retrigger.
          Require the zone to clear before trying again.
        */
        handlePlaylistFailure(now);
      }
    }

    return;
  }


  /*
    The current reading is outside the zone.

    Allow a short gap before deciding that the person genuinely left.
  */
  const unsigned long timeSinceLastInZone =
      now - lastInZoneAt;

  if (timeSinceLastInZone > ALLOWED_READING_GAP_MS) {
    systemState = WAITING_FOR_VISITOR;

    printEvent(
      now,
      "TARGET LEFT BEFORE FIVE-SECOND CONFIRMATION"
    );

    printEvent(
      now,
      "SYSTEM READY FOR ANOTHER CANDIDATE"
    );
  }
}


// ===========================================================================
// Begin the next playlist in sequence
// ===========================================================================

bool startNextPlaylist() {
  /*
    Clear any leftover inter-track pause state before beginning
    a new playlist.
  */
  waitingBetweenTracks = false;

  trackPauseStartedAt = 0;

  /*
    Select the playlist assigned to this visitor.
  */
  activePlaylist = nextPlaylist;
  currentTrack = 0;
  

  /*
    Advance the selection now so the next confirmed visitor receives
    the following playlist.

    After the final playlist, this wraps back to the first playlist.
  */
  nextPlaylist++;

  if (nextPlaylist >= NUM_PLAYLISTS) {
    nextPlaylist = 0;
  }

  Serial.println();
  Serial.println("========================================");

  Serial.print("Starting playlist ");
  Serial.print(activePlaylist + 1);
  Serial.print(" of ");
  Serial.println(NUM_PLAYLISTS);

  Serial.print("Tracks in this playlist: ");
  Serial.println(playlistLengths[activePlaylist]);

  Serial.println("========================================");

  return startCurrentTrack();
}


// ===========================================================================
// Start the current track within the active playlist
// ===========================================================================

bool startCurrentTrack() {
  if (activePlaylist >= NUM_PLAYLISTS) {
    Serial.println("ERROR: Invalid active playlist number.");
    return false;
  }

  if (currentTrack >= playlistLengths[activePlaylist]) {
    Serial.println("ERROR: Invalid playlist track number.");
    return false;
  }

  const char* filename =
      playlists[activePlaylist][currentTrack];

  Serial.println();

  Serial.print("Playlist ");
  Serial.print(activePlaylist + 1);

  Serial.print(" — track ");
  Serial.print(currentTrack + 1);

  Serial.print(" of ");
  Serial.print(playlistLengths[activePlaylist]);

  Serial.print(": ");
  Serial.println(filename);

  return startSound(filename);
}


// ===========================================================================
// State: playlist playback
// ===========================================================================

void processAudioPlayback() {
  const unsigned long now = millis();


  // -------------------------------------------------------------------------
  // Wait between tracks without using delay()
  // -------------------------------------------------------------------------

  /*
    The system remains in PLAYING_SOUND during this pause, so ultrasonic
    measurements remain suspended for the entire playlist.
  */
  if (waitingBetweenTracks) {
    if (now - trackPauseStartedAt < TRACK_PAUSE_MS) {
      return;
    }

    waitingBetweenTracks = false;

    Serial.println();
    Serial.println("Inter-track pause complete.");

    /*
      Start the track that currentTrack now points to.
    */
    if (!startCurrentTrack()) {
      handlePlaylistFailure(now);
    }

    return;
  }


  // -------------------------------------------------------------------------
  // Service the MP3 decoder while the current track is playing
  // -------------------------------------------------------------------------

  if (mp3 != nullptr && mp3->isRunning()) {
    if (mp3->loop()) {
      return;
    }
  }


  // -------------------------------------------------------------------------
  // The current track has ended or stopped
  // -------------------------------------------------------------------------

  stopAndReleaseSound();

  Serial.print("Finished track ");
  Serial.print(currentTrack + 1);
  Serial.print(" of playlist ");
  Serial.println(activePlaylist + 1);


  /*
    Advance to the next track.
  */
  currentTrack++;


  // -------------------------------------------------------------------------
  // More tracks remain
  // -------------------------------------------------------------------------

  if (currentTrack < playlistLengths[activePlaylist]) {
    waitingBetweenTracks = true;
    trackPauseStartedAt = now;

    Serial.print("Pausing ");
    Serial.print(TRACK_PAUSE_MS);
    Serial.println(" ms before the next track.");

    return;
  }


  // -------------------------------------------------------------------------
  // No tracks remain
  // -------------------------------------------------------------------------

  finishPlaylist(now);
}

// ===========================================================================
// Finish the active playlist
// ===========================================================================

void finishPlaylist(const unsigned long now) {
  stopAndReleaseSound();
  waitingBetweenTracks = false;
  Serial.println();
  Serial.println("========================================");

  Serial.print("Playlist ");
  Serial.print(activePlaylist + 1);
  Serial.println(" complete.");

  Serial.println("========================================");
  Serial.println();

  systemState = WAITING_FOR_CLEAR;
  clearTimerRunning = false;

  /*
    Reset the sensor interval so the first post-playback reading
    happens immediately.
  */
  lastSensorReadingAt = 0;

  printEvent(
    now,
    "PLAYLIST FINISHED"
  );

  printEvent(
    now,
    "WAITING FOR VIEWING ZONE TO CLEAR"
  );

  Serial.print("The next visitor will receive playlist ");
  Serial.print(nextPlaylist + 1);
  Serial.println(".");
  Serial.println();
}


// ===========================================================================
// Handle an error while starting or advancing a playlist
// ===========================================================================

void handlePlaylistFailure(const unsigned long now) {
  stopAndReleaseSound();
  waitingBetweenTracks = false;
  systemState = WAITING_FOR_CLEAR;
  clearTimerRunning = false;

  /*
    Resume sensor readings immediately so the system can determine
    when the visitor has left.
  */
  lastSensorReadingAt = 0;

  printEvent(
    now,
    "PLAYLIST FAILED — WAITING FOR CLEAR"
  );
}


// ===========================================================================
// State: playback finished; wait for visitor to leave
// ===========================================================================

void processWaitingForClear(
    const unsigned long now,
    const bool inViewingZone) {

  if (inViewingZone) {
    /*
      Someone is still present or has returned.

      Any previous clear-period timing is canceled.
    */
    if (clearTimerRunning) {
      printEvent(
        now,
        "TARGET RETURNED — CLEAR TIMER CANCELED"
      );
    }

    clearTimerRunning = false;
    return;
  }


  /*
    The zone is currently clear.
  */
  if (!clearTimerRunning) {
    clearTimerRunning = true;
    clearPeriodStartedAt = now;

    printEvent(
      now,
      "VIEWING ZONE IS CLEAR — REARM TIMER STARTED"
    );

    return;
  }


  const unsigned long clearTime =
      now - clearPeriodStartedAt;

  if (clearTime >= CLEAR_TO_REARM_TIME_MS) {
    clearTimerRunning = false;
    systemState = WAITING_FOR_VISITOR;

    printEvent(
      now,
      "SYSTEM REARMED"
    );

    Serial.print("Next trigger will play playlist ");
    Serial.print(nextPlaylist + 1);
    Serial.println(".");
    Serial.println();
  }
}


// ===========================================================================
// Start one MP3 file
// ===========================================================================

bool startSound(const char* filename) {
  /*
    Remove any unexpected remains from the previous track.
  */
  stopAndReleaseSound();

  Serial.print("Opening sound: ");
  Serial.println(filename);

  if (!SD.exists(filename)) {
    Serial.println("ERROR: Sound file does not exist.");
    return false;
  }


  // -------------------------------------------------------------------------
  // Open the MP3 file on the SD card
  // -------------------------------------------------------------------------

  audioFile = new AudioFileSourceSD(filename);

  if (audioFile == nullptr) {
    Serial.println("ERROR: Could not allocate SD file source.");
    return false;
  }


  // -------------------------------------------------------------------------
  // Create the MP3 decoder
  // -------------------------------------------------------------------------

  mp3 = new AudioGeneratorMP3();

  if (mp3 == nullptr) {
    Serial.println("ERROR: Could not allocate MP3 decoder.");

    delete audioFile;
    audioFile = nullptr;

    return false;
  }


  // -------------------------------------------------------------------------
  // Begin decoding directly from the SD-card source
  // -------------------------------------------------------------------------

  if (!mp3->begin(audioFile, audioOutput)) {
    Serial.println("ERROR: MP3 decoder could not start.");

    stopAndReleaseSound();
    return false;
  }

  Serial.println("Playback started.");

  return true;
}


// ===========================================================================
// Stop and release current audio objects
// ===========================================================================

void stopAndReleaseSound() {
  if (mp3 != nullptr) {
    if (mp3->isRunning()) {
      mp3->stop();
    }

    delete mp3;
    mp3 = nullptr;
  }

  if (audioFile != nullptr) {
    delete audioFile;
    audioFile = nullptr;
  }
}


// ===========================================================================
// Verify all playlist files during startup
// ===========================================================================

bool verifyPlaylistFiles() {
  bool allFilesFound = true;

  for (uint8_t playlistIndex = 0;
       playlistIndex < NUM_PLAYLISTS;
       playlistIndex++) {

    Serial.print("  Playlist ");
    Serial.print(playlistIndex + 1);
    Serial.println(":");

    for (uint8_t trackIndex = 0;
         trackIndex < playlistLengths[playlistIndex];
         trackIndex++) {

      const char* filename =
          playlists[playlistIndex][trackIndex];

      Serial.print("    ");
      Serial.print(filename);
      Serial.print(" ... ");

      if (SD.exists(filename)) {
        Serial.println("found");
      } else {
        Serial.println("MISSING");
        allFilesFound = false;
      }
    }
  }

  return allFilesFound;
}

// ===========================================================================
// Return a readable name for the current operating state
// ===========================================================================

const char* systemStateName() {
  switch (systemState) {

    case WAITING_FOR_VISITOR:
      return "WAITING";

    case POSSIBLE_VISITOR:
      return "DWELL";

    case PLAYING_SOUND:
      return "PLAYING";

    case WAITING_FOR_CLEAR:
      return "CLEARING";

    default:
      return "UNKNOWN";
  }
}
#if ENABLE_WIFI_TUNING

// ===========================================================================
// Start the ESP32's private tuning network and web server
// ===========================================================================

void startWifiTuning() {
  Serial.println();
  Serial.println("Starting Wi-Fi tuning mode...");

  /*
    Use access-point-only mode. The ESP32 does not attempt to join
    the home network and does not require internet access.
  */
  WiFi.mode(WIFI_AP);

  const bool accessPointStarted =
      WiFi.softAP(
        TUNING_WIFI_NAME,
        TUNING_WIFI_PASSWORD
      );

  if (!accessPointStarted) {
    Serial.println("ERROR: Wi-Fi access point failed to start.");
    Serial.println("The main prop will continue without Wi-Fi.");
    return;
  }

  /*
    Register the two browser routes:

      /         Human-readable tuning dashboard
      /status   Current machine-readable JSON status
  */
  tuningServer.on(
    "/",
    HTTP_GET,
    handleTuningHomePage
  );

  tuningServer.on(
    "/status",
    HTTP_GET,
    handleTuningStatus
  );

  /*
    Redirect unknown paths to the dashboard. This is useful when a
    phone performs a connectivity check after joining the network.
  */
  tuningServer.onNotFound([]() {
    tuningServer.sendHeader("Location", "/");
    tuningServer.send(302, "text/plain", "");
  });

  tuningServer.begin();

  Serial.println("Wi-Fi tuning page ready.");
  Serial.print("  Network:  ");
  Serial.println(TUNING_WIFI_NAME);

  Serial.print("  Password: ");
  Serial.println(TUNING_WIFI_PASSWORD);

  Serial.print("  Address:  http://");
  Serial.println(WiFi.softAPIP());

  Serial.println();
}


// ===========================================================================
// Serve the tuning dashboard
// ===========================================================================

void handleTuningHomePage() {
  /*
    The browser fetches /status twice per second and updates the
    stationary values on the page. No external fonts, scripts,
    internet connection, or files are required.
  */

  static const char PAGE_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta
    name="viewport"
    content="width=device-width, initial-scale=1"
  >
  <title>Mad Scientist Tuning</title>

  <style>
    :root {
      color-scheme: dark;
      font-family:
        -apple-system,
        BlinkMacSystemFont,
        "Segoe UI",
        sans-serif;
    }

    body {
      margin: 0;
      padding: 18px;
      background: #111;
      color: #eee;
    }

    main {
      width: min(680px, 100%);
      margin: 0 auto;
    }

    h1 {
      margin: 0 0 4px;
      font-size: 1.55rem;
    }

    .subtitle {
      margin: 0 0 18px;
      color: #aaa;
    }

    .grid {
      display: grid;
      grid-template-columns:
        repeat(auto-fit, minmax(145px, 1fr));
      gap: 10px;
    }

    .card {
      padding: 14px;
      border: 1px solid #444;
      border-radius: 10px;
      background: #1b1b1b;
    }

    .label {
      margin-bottom: 6px;
      color: #aaa;
      font-size: 0.85rem;
    }

    .value {
      font-size: 1.35rem;
      font-weight: 650;
      overflow-wrap: anywhere;
    }

    .wide {
      grid-column: 1 / -1;
    }

    .bar {
      height: 12px;
      margin-top: 10px;
      overflow: hidden;
      border: 1px solid #555;
      border-radius: 999px;
      background: #090909;
    }

    .bar-fill {
      width: 0%;
      height: 100%;
      background: #ddd;
      transition: width 0.15s linear;
    }

    .yes {
      color: #8ee58e;
    }

    .no {
      color: #eee;
    }

    .warning {
      color: #ffcf75;
    }

    #connection {
      margin-top: 14px;
      color: #aaa;
      font-size: 0.9rem;
    }
  </style>
</head>

<body>
  <main>
    <h1>Mad Scientist Armature</h1>
    <p class="subtitle">Raven module tuning page</p>

    <section class="grid">
      <div class="card">
        <div class="label">Distance</div>
        <div class="value" id="distance">—</div>
      </div>

      <div class="card">
        <div class="label">In viewing zone</div>
        <div class="value" id="in-zone">—</div>
      </div>

      <div class="card">
        <div class="label">Current state</div>
        <div class="value" id="state">—</div>
      </div>

      <div class="card">
        <div class="label">Next playlist</div>
        <div class="value" id="next-playlist">—</div>
      </div>

      <div class="card">
        <div class="label">Active playlist</div>
        <div class="value" id="active-playlist">—</div>
      </div>

      <div class="card">
        <div class="label">Current track</div>
        <div class="value" id="current-track">—</div>
      </div>

      <div class="card">
        <div class="label">Current filename</div>
        <div class="value" id="current-filename">—</div>
      </div>

      <div class="card">
        <div class="label">Uptime</div>
        <div class="value" id="uptime">—</div>
      </div>

      <div class="card">
        <div class="label">Last sensor reading</div>
        <div class="value" id="reading-age">—</div>
      </div>

      <div class="card">
        <div class="label">Track pause</div>
        <div class="value" id="track-pause">—</div>
      </div>

      <div class="card wide">
        <div class="label" id="timer-label">
          No timer active
        </div>

        <div class="value" id="timer-value">—</div>

        <div class="bar">
          <div class="bar-fill" id="timer-bar"></div>
        </div>
      </div>
    </section>

    <div id="connection">
      Connecting…
    </div>
  </main>

  <script>
    const byId = (id) => document.getElementById(id);

    function secondsText(ms) {
      return (ms / 1000).toFixed(1) + " s";
    }

    async function updateStatus() {
      try {
        const response = await fetch(
          "/status",
          { cache: "no-store" }
        );

        if (!response.ok) {
          throw new Error("HTTP " + response.status);
        }

        const data = await response.json();

        byId("distance").textContent =
          data.distance_cm === null
            ? "NO ECHO"
            : data.distance_cm.toFixed(1) + " cm";

        const inZone = byId("in-zone");
        inZone.textContent =
          data.in_zone ? "YES" : "NO";

        inZone.className =
          "value " + (data.in_zone ? "yes" : "no");

        byId("state").textContent = data.state;

        byId("next-playlist").textContent =
          data.next_playlist;

        byId("active-playlist").textContent =
          data.active_playlist === 0
            ? "—"
            : data.active_playlist;

        byId("current-track").textContent =
          data.current_track === 0
            ? "—"
            : data.current_track +
              " of " +
              data.tracks_in_playlist;

        byId("current-filename").textContent =
          data.current_filename === null
            ? "—"
            : data.current_filename;

        byId("uptime").textContent =
          secondsText(data.uptime_ms);

        byId("reading-age").textContent =
          secondsText(data.reading_age_ms);

        byId("track-pause").textContent =
          data.track_pause_ms + " ms";

        let label = "No timer active";
        let timerText = "—";
        let percent = 0;

        if (data.state === "DWELL") {
          label = "Viewer dwell progress";
          timerText =
            secondsText(data.dwell_elapsed_ms) +
            " / " +
            secondsText(data.dwell_required_ms);

          percent =
            100 *
            data.dwell_elapsed_ms /
            data.dwell_required_ms;
        }

        if (
          data.state === "CLEARING" &&
          data.clear_timer_running
        ) {
          label = "Clear-to-rearm progress";
          timerText =
            secondsText(data.clear_elapsed_ms) +
            " / " +
            secondsText(data.clear_required_ms);

          percent =
            100 *
            data.clear_elapsed_ms /
            data.clear_required_ms;
        }

        percent = Math.max(0, Math.min(100, percent));

        byId("timer-label").textContent = label;
        byId("timer-value").textContent = timerText;
        byId("timer-bar").style.width = percent + "%";

        byId("connection").textContent =
          "Live — updated " +
          new Date().toLocaleTimeString();

        byId("connection").className = "";
      }
      catch (error) {
        /*
          The ESP32 intentionally pauses its web server while playing
          audio. Keep retrying until playback finishes.
        */
        byId("connection").textContent =
          "Waiting for ESP32 — audio may be playing";

        byId("connection").className = "warning";
      }
    }

    updateStatus();
    setInterval(updateStatus, 500);
  </script>
</body>
</html>
)HTML";

  tuningServer.send_P(
    200,
    "text/html",
    PAGE_HTML
  );
}


// ===========================================================================
// Return the current status as JSON
// ===========================================================================

void handleTuningStatus() {
  const unsigned long now = millis();

  unsigned long dwellElapsedMs = 0;

  if (systemState == POSSIBLE_VISITOR) {
    dwellElapsedMs =
        now - visitorCandidateStartedAt;

    if (dwellElapsedMs > VIEWER_DWELL_TIME_MS) {
      dwellElapsedMs = VIEWER_DWELL_TIME_MS;
    }
  }


  unsigned long clearElapsedMs = 0;

  if (
      systemState == WAITING_FOR_CLEAR &&
      clearTimerRunning
  ) {
    clearElapsedMs =
        now - clearPeriodStartedAt;

    if (clearElapsedMs > CLEAR_TO_REARM_TIME_MS) {
      clearElapsedMs = CLEAR_TO_REARM_TIME_MS;
    }
  }


  const bool playlistIsActive =
      systemState == PLAYING_SOUND;


  String json;
  json.reserve(512);

  json += "{";

  json += "\"distance_cm\":";

  if (isnan(latestDistanceCm)) {
    json += "null";
  } else {
    json += String(latestDistanceCm, 1);
  }

  json += ",\"in_zone\":";
  json += latestInViewingZone ? "true" : "false";

  json += ",\"state\":\"";
  json += systemStateName();
  json += "\"";

  json += ",\"uptime_ms\":";
  json += String(now);

  json += ",\"reading_age_ms\":";
  json += String(now - latestDistanceReadingAt);

  json += ",\"dwell_elapsed_ms\":";
  json += String(dwellElapsedMs);

  json += ",\"dwell_required_ms\":";
  json += String(VIEWER_DWELL_TIME_MS);

  json += ",\"clear_timer_running\":";
  json += clearTimerRunning ? "true" : "false";

  json += ",\"clear_elapsed_ms\":";
  json += String(clearElapsedMs);

  json += ",\"clear_required_ms\":";
  json += String(CLEAR_TO_REARM_TIME_MS);

  json += ",\"next_playlist\":";
  json += String(nextPlaylist + 1);

  json += ",\"active_playlist\":";
  json += playlistIsActive
      ? String(activePlaylist + 1)
      : "0";

  json += ",\"current_track\":";
  json += playlistIsActive
      ? String(currentTrack + 1)
      : "0";

  json += ",\"tracks_in_playlist\":";
  json += playlistIsActive
      ? String(playlistLengths[activePlaylist])
      : "0";

  json += ",\"current_filename\":";

if (playlistIsActive) {
  json += "\"";
  json += playlists[activePlaylist][currentTrack];
  json += "\"";
} else {
  json += "null";
}

  json += ",\"track_pause_ms\":";
  json += String(TRACK_PAUSE_MS);

  json += "}";

  tuningServer.send(
    200,
    "application/json",
    json
  );
}

#endif

// ===========================================================================
// Continuous distance logging
// ===========================================================================

void printDistanceReading(
    const unsigned long now,
    const float distanceCm,
    const bool inViewingZone) {

  Serial.print("Elapsed_s:");
  Serial.print(now / 1000.0, 1);

  Serial.print('\t');

  Serial.print("Distance_cm:");

  if (isnan(distanceCm)) {
    Serial.print("NO ECHO");
  } else {
    Serial.print(distanceCm, 1);
  }

  Serial.print('\t');

  Serial.print("In_zone:");
  Serial.print(inViewingZone ? 1 : 0);

  Serial.print('\t');

  Serial.print("State:");

  switch (systemState) {

    case WAITING_FOR_VISITOR:
      Serial.println("WAITING");
      break;

    case POSSIBLE_VISITOR:
      Serial.println("DWELL");
      break;

    case PLAYING_SOUND:
      Serial.println("PLAYING");
      break;

    case WAITING_FOR_CLEAR:
      Serial.println("CLEARING");
      break;
  }
}


// ===========================================================================
// Event logging
// ===========================================================================

void printEvent(
    const unsigned long now,
    const char* message) {

  Serial.println();
  Serial.print("EVENT at ");
  Serial.print(now / 1000.0, 1);
  Serial.print(" s: ");
  Serial.println(message);
  Serial.println();
}