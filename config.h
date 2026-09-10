/**
 *  NOPEUSTESTI - wiring and game tuning. The only file you need to edit.
 *
 *  ==========================================================================
 *  WIRING: everything about how the real device is connected is in the
 *  "Wiring" section right below. Whatever way the wires ended up, fix it
 *  here and nowhere else:
 *
 *    - a button lights the wrong lamp      -> edit the CHANNELS table
 *    - one colour glares, another is dim   -> brightness column in CHANNELS
 *    - lamps are in a different order      -> reorder the CHANNELS rows
 *    - buttons wired to 5V instead of GND  -> BUTTON_ACTIVE_LOW / pull-up
 *    - lamps switch on with LOW            -> LIGHT_ACTIVE_HIGH
 *    - display or buzzer on other pins     -> DISPLAY_*_PIN, BUZZER_PIN
 *    - 3 or 5 lamps instead of 4           -> add/remove CHANNELS rows
 *
 *  To check the result without a serial monitor: hold any button while
 *  powering up. In that WIRING TEST mode every button lights its own lamp
 *  while held and the display shows the button's pin number. The boot lamp
 *  test also lights each lamp while showing its pin, and the serial
 *  monitor (115200 baud) prints the whole table at boot.
 *  ==========================================================================
 *
 *  Board: Arduino Leonardo (any AVR board works, no interrupt pins needed).
 */
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Wiring
// ---------------------------------------------------------------------------

struct Channel {
  const char *name;    // shown on the serial monitor only
  uint8_t lightPin;    // Arduino pin driving this lamp
  uint8_t buttonPin;   // Arduino pin reading this lamp's button
  uint8_t brightness;  // 0..255, only on PWM pins (Leonardo: 3 5 6 9 10 11 13)
};

// One row per lamp+button pair. A row ties a lamp to the button that is
// physically under it, so if lamp "RED" is on pin 12 and the button below it
// reads on pin 3, the row is { "RED", 12, 3 }. Row order is the physical
// left-to-right order on the panel; it is used for the lamp test, the
// attract chase and the countdown, never for the game logic.
//
//   ┌────────┬───────────┬────────────┐
//   │ name   │ light pin │ button pin │
//   ├────────┼───────────┼────────────┤
// Lamp and button pins measured 10.9.2026 with tools/pintest.
//
// Brightness balances the colours: 255 = full. Green and yellow LEDs look
// much brighter than red and blue at the same current, so dim those two.
// A lamp on a non-PWM pin (red on 7) is always full on. Note: dimming the
// lamp on pin 5 does not work together with a buzzer, both use Timer 3.
//
//   ┌────────┬───────────┬────────────┬────────────┐
//   │ name   │ light pin │ button pin │ brightness │
//   ├────────┼───────────┼────────────┼────────────┤
static const Channel CHANNELS[] = {
    { "GREEN",  6,          2,           110 },
    { "BLUE",   5,          11,          255 },
    { "YELLOW", 13,         1,           170 },
    { "RED",    7,          0,           255 },
};
//   └────────┴───────────┴────────────┴────────────┘
static const uint8_t NUM_CHANNELS = sizeof(CHANNELS) / sizeof(CHANNELS[0]);

// Buttons. Default: one leg on the pin, other leg on GND, internal pull-up.
//   BUTTON_ACTIVE_LOW  true  = pressed reads LOW  (wired to GND)
//                      false = pressed reads HIGH (wired to 5V, needs an
//                              external pull-down resistor to GND)
//   BUTTON_USE_PULLUP  enable the chip's internal pull-up. Leave true for
//                      GND-wired buttons; set false if you use your own
//                      external resistors.
static const bool BUTTON_ACTIVE_LOW = true;
static const bool BUTTON_USE_PULLUP = true;

// Lamps. Default: pin -> resistor -> LED -> GND, i.e. HIGH = on.
// Set false if a lamp lights when the pin goes LOW (PNP transistor,
// low-active relay board, LED wired to 5V).
static const bool LIGHT_ACTIVE_HIGH = true;

// TM1637 4-digit display (Grove 4-Digit Display or bare module).
static const uint8_t DISPLAY_CLK_PIN = 9;
static const uint8_t DISPLAY_DIO_PIN = 10;

// Buzzer. Set BUZZER_PIN to -1 if there is none.
//   BUZZER_PASSIVE true  = passive piezo/speaker, driven with tones
//                  false = active buzzer module that just needs power
//                          (one fixed pitch, driven on/off)
static const int8_t BUZZER_PIN = 4;
static const bool BUZZER_PASSIVE = true;

// ---------------------------------------------------------------------------
// Button debounce
// ---------------------------------------------------------------------------

// A press is accepted after this many consecutive 1 ms samples read "pressed".
// 2 samples = ~2 ms latency and immune to single-sample glitches.
static const uint8_t PRESS_CONFIRM_SAMPLES = 2;

// A button counts as released only after it has read "released" continuously for
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
static const uint16_t ATTRACT_PAUSE_MS = 2500;   // dark gap between animations
static const uint16_t ANIM_KITT_STEP_MS = 110;   // KITT scanner, per lamp
static const uint16_t ANIM_FILL_STEP_MS = 160;   // fill bar, per lamp
static const uint16_t ANIM_PULSE_STEP_MS = 220;  // outside-in pulse, per ring
static const uint16_t ANIM_SPARKLE_STEP_MS = 90; // random sparkle, per pattern
static const uint16_t ATTRACT_SHOW_SCORE_MS = 3000;
static const uint16_t ATTRACT_SHOW_BEST_MS = 2000;
static const uint32_t ATTRACT_SLEEP_MS = 120000UL; // dark after 2 min idle

static const uint8_t DISPLAY_BRIGHTNESS = 4;     // 0..7

// Set to 1, flash once, set back to 0 to wipe the stored high score.
#define RESET_HIGH_SCORE_ON_BOOT 0
