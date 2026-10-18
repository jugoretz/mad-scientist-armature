# A brief history of Uncanny Eyes — and how it reached the Mad Scientist Armature

The animated eyes in the Mad Scientist Armature are part of a long-running
open-source collaboration. The software did not originate with this project:
it passed through several generations of microcontrollers, display drivers,
and Halloween experiments before arriving on our ESP32-S3 and round displays.

## 2015: Phil Burgess makes electronic eyes uncanny

In 2015, **Phillip Burgess** (*Paint Your Dragon*) at **Adafruit** published
[Animated Electronic Eyes](https://learn.adafruit.com/animated-electronic-eyes-using-teensy-3-1/overview)
and the accompanying [Uncanny Eyes source code](https://github.com/adafruit/Uncanny_Eyes).
The original implementation targeted the **Teensy 3.1**, with two small OLED
or TFT screens—not initially a Feather. It animated gaze, blinking, eyelids,
and iris size, using precomputed eye-image data in C header files. Adafruit
also supplied a [graphics conversion tool](https://github.com/adafruit/Uncanny_Eyes/tree/master/convert)
for creating additional eyes.

Burgess credited **David Boccabella (Marcwolf)** for inspiring the concept
through earlier servo/OLED animatronic-eye experiments, and **Paul Stoffregen**
for SPI FIFO insights. These are part of the project's history, not necessarily
independent authors of our current sketch.

## After 2015: more Adafruit boards and displays

The original code subsequently supported Adafruit M0 and M4 hardware as well
as Teensy. Adafruit's later Halloween eye projects—including the Hallowing
family and Monster M4SK—extended the idea to different processors and
higher-resolution screens. The [M4_Eyes example](https://github.com/adafruit/Adafruit_Learning_System_Guides/tree/main/M4_Eyes)
is an important parallel branch of the family, **not a demonstrated direct
ancestor of our sketch**.

## Bodmer: the TFT_eSPI branch

**Bodmer** developed the widely used
[TFT_eSPI library](https://github.com/Bodmer/TFT_eSPI), and adapted Uncanny
Eyes to work with it on ESP8266, ESP32, and other supported hardware. The
library's [Animated_Eyes examples](https://github.com/Bodmer/TFT_eSPI/tree/master/examples/Generic)
made the animation more accessible on inexpensive development boards.

There are several examples, including an ESP8266/NodeMCU-oriented version.
Our sketch's opening comment explicitly identifies
[Animated_Eyes_1](https://github.com/Bodmer/TFT_eSPI/blob/master/examples/Generic/Animated_Eyes_1/Animated_Eyes_1.ino)
as its major starting point. This is much stronger evidence than guessing from
the board or display type. The date of Bodmer's first eye adaptation has not
been established here from commit history.

## 2023: Florian Sternke and round displays

**Florian Sternke** (`dalori`) published
[ESP32 Uncanny Eyes Halloween Skull](https://github.com/dalori/ESP32-uncanny-eyes-halloween-skull),
a project with two round **240 × 240 GC9A01** TFTs and a moving skull jaw.
His work is especially relevant to the display format used here and includes
large-format eye graphics. The repository dates to at least 2023.

We used or consulted Sternke's large-eye work when building the Halloween
goggles, including `default_large.h`. This does **not** mean that the entire
present sketch is a fork of his skull project: our own source identifies
Bodmer's `Animated_Eyes_1` as its principal code base. The precise provenance
of every large eye design remains to be checked.

## 2025: Halloween goggles

Our 2025 goggles used an **ESP32-S3** and two **240 × 240 round GC9A01**
displays. Getting the hardware, TFT_eSPI configuration, chip-select behavior,
display rotations, and appropriately sized graphics to work together took
some experimentation. The result provided a working basis for later projects.

## 2026: the Mad Scientist Armature

For the [Mad Scientist Armature](https://github.com/jugoretz/mad-scientist-armature),
the eyes were adapted again. Instead of one controller driving a pair of eyes,
**each eye module has its own ESP32-S3 and one round display**. The current
`config.h` sets `TFT_COUNT` and `NUM_EYES` to 1. The project also integrates
its own hardware wiring, configuration choices, and NeoPixel lighting, and
collects a number of eye-design header files.

The result is a descendant of the original Uncanny Eyes—not a new eye
animation system written from scratch. Its history is a good example of how
permissively licensed hobby electronics projects evolve through shared code,
experimentation, and adaptation.

## Timeline at a glance

| Period | Milestone |
| --- | --- |
| Before 2015 | David Boccabella's animatronic-eye experiments inspire the idea. |
| 2015 | Phil Burgess publishes Uncanny Eyes for Teensy 3.1 through Adafruit. |
| Later 2010s | Adafruit expands the concept to other boards and larger displays. |
| Date not yet verified | Bodmer adapts Uncanny Eyes for TFT_eSPI and ESP-family boards. |
| By 2023 | Florian Sternke's ESP32 Halloween skull uses dual round 240 × 240 displays. |
| 2025 | Our ESP32-S3 Halloween goggles use two round GC9A01 displays. |
| 2026 | The Mad Scientist Armature uses independent single-eye modules. |

## Credits and licensing

Special thanks to **Phillip Burgess and Adafruit**, **Bodmer**, and
**Florian Sternke**, along with the earlier contributors they credited.
See [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for licensing and
artwork-provenance details. Some graphic-file origins remain unverified;
acknowledgment here is not a claim that every image has been cleared for
redistribution.
