# Mad Scientist Armature
### Hardware Overview & System Architecture
**Version: October 2026**

---

[![Watch the video]([https://youtube.com](https://youtu.be/LL1f9C8RyxI?si=5dwZkYu1-de1xpDs))]([https://youtube.com](https://youtu.be/LL1f9C8RyxI?si=5dwZkYu1-de1xpDs))


## 1. Project Overview

The **Mad Scientist Armature** is a modular, interactive Halloween decoration designed to operate outdoors throughout the month of October.

The system combines animated eyes, illuminated gauges, sound effects, and decorative lighting into a single weatherproof structure that responds to visitors.

### Design Goals

- **Weatherproof:** Designed for continuous outdoor operation throughout October.
- **Modular:** Individual components can be added, removed, replaced, or rearranged.
- **Interactive:** Detects visitors and responds with sound effects.
- **Self-contained:** Each functional module has its own microcontroller and local power regulation.
- **Stable:** The structure maintains a low center of gravity and limited horizontal spread to resist wind.
- **Adaptable:** The armature can accommodate new or modified modules in future versions.

### Physical Construction

The supporting structure is constructed from **½-inch black iron pipe**.

The pipe serves two purposes:

1. Provides a rigid framework for mounting the modules.
2. Conceals and protects the power wiring routed through its interior.

The iron pipe may develop surface rust over time, which is acceptable and consistent with the intended aesthetic.

Each functional module is housed in a **weatherproof plastic enclosure**, with waterproof cable glands for electrical connections. The enclosures are painted and decorated to complement the overall design.

---

## 2. Module Overview

As of October 2026, the armature consists of **five modules**, including one power distribution module and four functional modules.

| Module | Primary Function | Controller |
|---|---|---|
| **Power Distribution** | Receives and distributes 12 V DC power | None |
| **Eye A** | Animated eyeball and decorative lighting | ESP32-S3 |
| **Eye B** | Second animated eyeball and decorative lighting | ESP32-S3 |
| **Gauge & Sound** | Animated danger gauge, visitor detection, and audio playback | ESP32-S3 + ESP32-C3 |
| **Raven** | Animated purple/green light bulb | ESP32-C3 |

### Visual and Interactive Features

**Raven**

Mounted at the highest point of the armature, the raven faces outward and holds a small vintage-style light bulb dangling from its beak.

The bulb contains two LED filaments, one purple and one green. The filaments alternately fade in and out, creating a continuous color-changing effect.

**Animated Eyes**

Two independent eye modules contain round TFT displays running the *Uncanny Eyes* software.

Each display sits behind a clear, weatherproof acrylic shield. The eyeball designs can be customized independently in software.

**Danger Gauge**

A conventional analog voltmeter has been modified with a custom **Danger Level** display card.

The gauge advances steadily through progressively higher danger levels, accompanied by changing NeoPixel illumination. After reaching its maximum level, the gauge returns to a safe reading and begins the cycle again.

**Interactive Sound**

The sound system is housed in the same enclosure as the danger gauge.

An ultrasonic sensor detects visitors standing in front of the armature. After a visitor remains in the detection zone for five seconds, the system plays a short audio playlist.

The system rotates through **nine different playlists**, stored on an SD card.

Once the detection zone has been clear for two seconds, the system resets and is ready to detect another visitor.

---

# 3. System Architecture

## 3.1 General Architecture

The armature uses a distributed control architecture.

Each functional module operates independently, with its own microcontroller and local power regulation.

A central power distribution module supplies **12 V DC** to all four functional modules.

Each functional module contains a buck converter that reduces the incoming 12 V supply to the regulated **5 V** required by its electronics.

### Overall Power Distribution

```text
                12 V DC Power Supply
                         │
                         ▼
              ┌─────────────────────┐
              │ Power Distribution  │
              │       Module        │
              └──────────┬──────────┘
                         │
            12 V DC Distribution
                         │
         ┌───────────────┼───────────────┐
         │               │               │
         ▼               ▼               ▼
     Eye Module A    Eye Module B    Gauge Module
         │               │               │
    Buck Converter  Buck Converter  Buck Converter
         │               │               │
        5 V             5 V             5 V

                         │
                         ▼
                    Raven Module
                         │
                   Buck Converter
                         │
                        5 V
```

All modules share the common 12 V power supply and ground distribution.

---

# 4. Power Distribution Module

## Purpose

The Power Distribution Module serves as the central electrical connection point for the armature.

It performs **no processing or control functions**.

Its sole responsibility is to receive the incoming 12 V DC supply and distribute power and ground to the functional modules.

## Components

| Component | Function |
|---|---|
| 12 V DC input | Receives power from the external supply |
| Weatherproof power switch | Main system on/off control |
| WAGO lever connectors | Distribute power and ground |
| Weatherproof enclosure | Protects electrical connections |

## Electrical Connections

**Inputs**
- 12 V DC
- Ground

**Outputs**
- Eye Module A — 12 V / GND
- Eye Module B — 12 V / GND
- Gauge Module — 12 V / GND
- Raven Module — 12 V / GND

Power wiring runs through the black iron pipe wherever possible, protecting it from direct exposure to the weather.

---

# 5. Eye Modules

The armature contains two independently controlled eye modules.

Both use identical hardware, but each can display a different eye design.

## 5.1 Eye Module A

### Components

| Component | Function |
|---|---|
| Buck converter | Converts 12 V to 5 V |
| ESP32-S3 | Main controller |
| Round TFT display | Displays animated eyeball |
| NeoPixels | Decorative illumination |
| Clear acrylic shield | Protects display from weather |

### Power Architecture

```text
12 V DC Input
      │
      ▼
Buck Converter
   (12 V → 5 V)
      │
      ▼
   ESP32-S3
```

The local buck converter provides regulated 5 V power for the module electronics.

### Controller Responsibilities

The ESP32-S3:

- Runs the *Uncanny Eyes* animation software.
- Controls the round TFT display.
- Controls the NeoPixel lighting.

### Display

The TFT display is mounted inside the weatherproof enclosure, behind a clear acrylic shield.

The eye design and animation characteristics can be modified in software.

---

## 5.2 Eye Module B

Eye Module B is **electrically and functionally identical to Eye Module A**.

It contains:

- Buck converter (12 V → 5 V)
- ESP32-S3
- Round TFT display
- NeoPixels
- Clear acrylic weatherproof shield

The primary difference is the eye design displayed by the software.

Because the modules operate independently, each eye can use different visual characteristics and animations.

---

# 6. Gauge & Sound Module

The Gauge Module combines two independent functions in a single weatherproof enclosure:

1. **Animated Danger Gauge**
2. **Interactive Sound System**

Each subsystem has its own microcontroller.

The ESP32-S3 handles the visual effects, while the ESP32-C3 manages visitor detection and audio playback.

## 6.1 Components

| Component | Function |
|---|---|
| Analog voltmeter | Physical danger gauge |
| Custom Danger Level card | Replaces the original voltmeter scale |
| NeoPixel | Gauge illumination |
| ESP32-S3 | Gauge and lighting controller |
| ESP32-C3 | Visitor detection and audio controller |
| Ultrasonic sensor | Detects visitor presence |
| SD card module | Stores audio playlists |
| I²S audio amplifier | Converts digital audio to speaker output |
| Rear-mounted speaker | Plays sound effects |
| Buck converter | Converts 12 V to 5 V |

---

## 6.2 Power Architecture

The module receives 12 V DC from the central power distribution module.

A local buck converter provides regulated 5 V power.

```text
             12 V DC Input
                   │
                   ▼
             Buck Converter
              (12 V → 5 V)
                   │
          ┌────────┼────────┐
          │        │        │
          ▼        ▼        ▼
       ESP32-S3 ESP32-C3  5 V Devices
                   │
                   ▼
              3.3 V Output
                   │
                   ▼
              SD Card Module
```

### Power Distribution

**5 V supply:**
- ESP32-S3
- ESP32-C3
- Ultrasonic sensor
- Audio amplifier

**3.3 V supply from ESP32-C3:**
- SD card module

---

## 6.3 Danger Gauge Subsystem

### Controller

**ESP32-S3**

### Responsibilities

The ESP32-S3 controls:

- The analog gauge.
- The gauge illumination NeoPixel.
- The timing and progression of the danger-level animation.

### Behavior

The gauge operates continuously through a repeating animation cycle.

1. Begins at a safe danger level.
2. Advances upward in discrete steps.
3. Changes NeoPixel illumination to match the current danger level.
4. Reaches its maximum danger level.
5. Returns to a safe reading.
6. Repeats the sequence.

The gauge operates independently of the visitor detection and sound system.

---

## 6.4 Interactive Sound Subsystem

### Controller

**ESP32-C3**

### Responsibilities

The ESP32-C3:

- Monitors the ultrasonic sensor.
- Determines whether a visitor is present.
- Implements the detection and reset timing.
- Selects audio playlists.
- Reads audio files from the SD card.
- Sends digital audio to the amplifier using I²S.

### Audio Signal Path

```text
          Ultrasonic Sensor
                  │
                  ▼
              ESP32-C3
                  │
                  ├──── SD Card
                  │     (Audio Files)
                  │
                  ▼
            I²S Audio Output
                  │
                  ▼
            Audio Amplifier
                  │
                  ▼
                Speaker
```

The SD card stores the audio files, which are read by the ESP32-C3 and transmitted as digital audio over I²S to the amplifier.

The amplifier drives the rear-mounted speaker.

---

## 6.5 Visitor Detection Logic

The sound system uses a timed detection sequence to avoid triggering when someone merely walks past the armature.

### Detection Sequence

**1. Idle**

The ultrasonic sensor continuously monitors the area in front of the armature.

**2. Presence Detected**

When a visitor enters the detection zone, a five-second timer begins.

**3. Trigger**

If the visitor remains continuously within the detection zone for five seconds, the ESP32-C3 initiates audio playback.

**4. Playback**

The system plays a short playlist selected from nine available playlists stored on the SD card.

The playlists rotate sequentially between activations.

**5. Reset**

After the detection zone has remained clear for two seconds, the system resets.

**6. Ready**

The system returns to idle and is ready for another five-second detection cycle.

### State Sequence

```text
           ┌──────────────┐
           │     IDLE     │
           └──────┬───────┘
                  │
          Visitor detected
                  │
                  ▼
           ┌──────────────┐
           │  5-Second    │
           │   Presence   │
           │    Timer     │
           └──────┬───────┘
                  │
       Continuous presence
           for 5 seconds
                  │
                  ▼
           ┌──────────────┐
           │    PLAY      │
           │   Playlist   │
           └──────┬───────┘
                  │
                  ▼
           ┌──────────────┐
           │  Wait for    │
           │  Clear Zone  │
           └──────┬───────┘
                  │
       Zone clear for 2 sec
                  │
                  ▼
                IDLE
```

If the visitor leaves before the initial five-second threshold, the detection timer resets without triggering playback.

---

# 7. Raven Module

The Raven Module is mounted at the highest point of the armature.

The raven sculpture faces outward and holds a small vintage-style light bulb suspended from its beak.

The bulb contains two LED filaments:

- **Purple**
- **Green**

The two filaments are independently controlled to create a smooth alternating fade.

## 7.1 Components

| Component | Function |
|---|---|
| Raven sculpture | Decorative housing and visual element |
| Vintage-style light bulb | Contains LED filaments |
| Purple LED filament | First illumination channel |
| Green LED filament | Second illumination channel |
| ESP32-C3 | Lighting controller |
| Buck converter | Converts 12 V to 5 V |

## 7.2 Power Architecture

```text
          12 V DC Input
                │
                ▼
          Buck Converter
           (12 V → 5 V)
                │
                ▼
             ESP32-C3
                │
          ┌─────┴─────┐
          │           │
          ▼           ▼
      Purple LED   Green LED
       Filament     Filament
```

## 7.3 Behavior

The ESP32-C3 generates a continuous, smooth fading animation.

As one LED filament fades out, the other fades in.

The resulting effect is a gradual transition between purple and green illumination.

The animation operates independently of the other armature modules.

---

# 8. Controller Responsibilities

The system uses two types of ESP32 microcontrollers, with responsibilities divided between visual effects and interactive functions.

## ESP32-S3 — Visual Controllers

The ESP32-S3 controllers handle the primary visual effects.

| Module | Responsibilities |
|---|---|
| Eye A | TFT animation, NeoPixels |
| Eye B | TFT animation, NeoPixels |
| Gauge | Analog gauge control, NeoPixel illumination |

### Primary Functions

- Animated TFT displays
- Gauge movement
- NeoPixel lighting
- Visual animation timing

---

## ESP32-C3 — Interactive and Lighting Controllers

The ESP32-C3 controllers handle visitor interaction, audio playback, and the raven's lighting animation.

| Module | Responsibilities |
|---|---|
| Gauge & Sound | Ultrasonic sensing, SD card access, I²S audio |
| Raven | Purple/green LED filament animation |

### Primary Functions

- Visitor presence detection
- Detection and reset timing
- SD card audio playback
- I²S digital audio output
- LED filament fading

---

# 9. System Summary

### Hardware Inventory

| Item | Quantity |
|---|---:|
| Weatherproof module enclosures | 5 |
| ESP32-S3 microcontrollers | 3 |
| ESP32-C3 microcontrollers | 2 |
| 12 V → 5 V buck converters | 4 |
| Round TFT eye displays | 2 |
| Analog danger gauge | 1 |
| Ultrasonic sensors | 1 |
| SD card modules | 1 |
| I²S audio amplifiers | 1 |
| Speakers | 1 |
| LED filaments | 2 |
| Central power distribution modules | 1 |

*Additional components include NeoPixels, wiring, cable glands, WAGO connectors, and the supporting black iron pipe structure.*

### Design Characteristics

| Characteristic | Implementation |
|---|---|
| **Primary power** | 12 V DC |
| **Local power regulation** | Individual buck converter per functional module |
| **Control architecture** | Distributed, independent microcontrollers |
| **Visual controllers** | ESP32-S3 |
| **Interactive/audio controllers** | ESP32-C3 |
| **Audio storage** | SD card |
| **Audio interface** | I²S |
| **Visitor detection** | Ultrasonic sensor |
| **Detection threshold** | 5 seconds continuous presence |
| **Reset threshold** | 2 seconds clear |
| **Audio content** | 9 rotating playlists |
| **Outdoor protection** | Weatherproof enclosures and cable glands |
| **Structural framework** | ½-inch black iron pipe |

---

## 10. Design Philosophy

The Mad Scientist Armature is designed around **modularity, independence, and simplicity**.

Each functional module receives the same 12 V power supply, regulates its own operating voltage, and runs its own control software.

This architecture offers several advantages:

- **Independent operation:** Each module functions without requiring communication with the others.
- **Simplified wiring:** Only power and ground need to be distributed through the armature.
- **Ease of maintenance:** Individual modules can be removed or serviced without redesigning the system.
- **Flexibility:** New visual effects, sensors, or interactive features can be introduced by adding or modifying modules.
- **Reliability:** A software failure in one functional module does not inherently prevent the others from operating.

The result is a flexible platform for outdoor Halloween effects, combining a distinctive industrial aesthetic with independently controlled lighting, animation, and sound.
