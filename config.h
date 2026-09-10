/**
 *  NOPEUSTESTI - hardware pin map and game tuning.
 *
 *  This is the only file you should need to touch when wiring the game
 *  or adjusting how hard it is.
 *
 *  Board: Arduino Leonardo (any AVR board works, no interrupt pins needed).
 *
 *  ┌────────┬───────────┬────────────┐
 *  │ colour │ light pin │ button pin │
 *  ├────────┼───────────┼────────────┤
 *  │ GREEN  │ 6         │ 0          │
 *  │ YELLOW │ 13        │ 1          │
 *  │ RED    │ 12        │ 2          │
 *  │ BLUE   │ 7         │ 3          │
 *  ├────────┼───────────┴────────────┤
 *  │ display│ CLK 9, DIO 10 (TM1637) │
 *  │ buzzer │ 4 (optional)           │
 *  └────────┴────────────────────────┘
 *
 *  Buttons: one leg to the pin, other leg to GND (internal pull-up is used).
 *  Lights:  pin -> resistor -> LED -> GND (or a transistor for bigger lamps).
 *
 *  The same table is printed to the serial monitor (115200 baud) at boot,
 *  and the boot lamp test lights each lamp while the display shows its pin.
 *  Hold any button while powering up to enter WIRING TEST mode: every
 *  button lights its own lamp while held and the display shows the
 *  button's pin number.
 */
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pin map
// ---------------------------------------------------------------------------

struct Channel {
  const char *name;
  uint8_t lightPin;
  uint8_t buttonPin;
};

// Order here is only cosmetic (lamp test / attract chase run in this order).
static const Channel CHANNELS[] = {
  { "GREEN",  6,  0 },
  { "YELLOW", 13, 1 },
  { "RED",    12, 2 },
  { "BLUE",   7,  3 },
};
static const uint8_t NUM_CHANNELS = sizeof(CHANNELS) / sizeof(CHANNELS[0]);

static const uint8_t DISPLAY_CLK_PIN = 9;
static const uint8_t DISPLAY_DIO_PIN = 10;

// Set to -1 if you have no buzzer.
static const int8_t BUZZER_PIN = 4;

// Lights are active HIGH by default. Flip this if you drive the lamps
// through PNP transistors / relays that switch on with a LOW.
static const bool LIGHT_ACTIVE_HIGH = true;

// ---------------------------------------------------------------------------
// Button debounce
// ---------------------------------------------------------------------------

// A press is accepted after this many consecutive 1 ms samples read LOW.
// 2 samples = ~2 ms latency and immune to single-sample glitches.
static const uint8_t PRESS_CONFIRM_SAMPLES = 2;

// A button counts as released only after it has read HIGH continuously for
// this long. This swallows release bounce completely, so holding a button
// and letting go never produces a phantom press.
static const uint16_t RELEASE_STABLE_MS = 30;

// ---------------------------------------------------------------------------
// Game tuning
// ---------------------------------------------------------------------------

// Time between two lights coming on at the start of a game.
static const uint16_t START_INTERVAL_MS = 900;

// Every new light multiplies the interval by this. Together with the start
// interval this sets the difficulty. Approximate interval per light count:
//
//   light #   0    20   40   60   80   100  120
//   ms        900  665  491  363  268  198  146
//
// Simulated scores for a player who sustains a steady press cadence
// (median of 200 games, 180 ms reaction to a new light):
//
//   ms/press  200  230  260  300  350  400  500
//   score     119  110  103  93   83   75   60
//
// So a good player tops out around 80-120. Nobody sustains 150 ms/press.
static const float SPEEDUP_FACTOR = 0.985f;

// The interval never goes below this, whatever the score.
static const uint16_t MIN_INTERVAL_MS = 120;

// How many lit-but-unpressed lights the player may have queued. When a new
// light would exceed this, the player was too slow and the game ends.
static const uint8_t MAX_PENDING = 5;

// When the same colour comes again while its lamp is still lit, the lamp
// blinks off for this long so the repeat is visible.
static const uint16_t RETRIGGER_GAP_MS = 70;

// ---------------------------------------------------------------------------
// Presentation timing
// ---------------------------------------------------------------------------

static const uint16_t LAMP_TEST_STEP_MS = 350;   // boot lamp test, per lamp
static const uint16_t COUNTDOWN_STEP_MS = 450;   // 3-2-1 before a game
static const uint16_t GAME_OVER_LOCKOUT_MS = 1500; // ignore presses after loss
static const uint16_t GAME_OVER_BLINK_MS = 350;  // score blink period
static const uint8_t  GAME_OVER_BLINKS = 8;
static const uint16_t ATTRACT_CHASE_MS = 180;    // idle light chase step
static const uint16_t ATTRACT_SHOW_SCORE_MS = 3000;
static const uint16_t ATTRACT_SHOW_BEST_MS = 2000;
static const uint32_t ATTRACT_SLEEP_MS = 120000UL; // dark after 2 min idle

static const uint8_t DISPLAY_BRIGHTNESS = 4;     // 0..7

// Set to 1, flash once, set back to 0 to wipe the stored high score.
#define RESET_HIGH_SCORE_ON_BOOT 0
