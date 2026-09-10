# Nopeustesti

The classic Finnish reaction game (*Speden Spelit* style) on an Arduino
Leonardo: four lights, four buttons, a 4-digit display and an optional
buzzer. Lights come on in random order, faster and faster. Press the buttons
in the order the lights came on. One wrong press, a press when nothing is
lit, or falling five lights behind ends the game. Score is the number of
correct presses. A good player tops out around 80–120.

No libraries to install. Open `nopeustesti.ino` in the Arduino IDE and
upload, or from the command line with arduino-cli installed:

```
./flash.sh          # compile, upload, open serial monitor
```

## Wiring

| colour  | light pin | button pin |
|---------|-----------|------------|
| GREEN   | 6         | 2          |
| BLUE    | 5         | 11         |
| YELLOW  | 13        | 1          |
| RED     | 7         | 0          |
| display | CLK 9, DIO 10 (TM1637 / Grove 4-Digit Display) | |
| buzzer  | none by default; set `BUZZER_PIN` to a pin to enable | |

Buttons go between the pin and GND; the internal pull-up is used. Lights go
pin → resistor → LED → GND, or through a transistor for real lamps.

### Wired it differently? Fix it in `config.h` only

The "Wiring" section at the top of `config.h` describes the real device.
Whatever way the wires ended up, adjust it there and nothing else changes:

| Symptom / situation                         | Change                              |
|---------------------------------------------|-------------------------------------|
| a button lights the wrong lamp              | edit the pins in the `CHANNELS` row |
| lamps sit in a different order on the panel | reorder the `CHANNELS` rows         |
| 3 or 5 lamps instead of 4                   | add or remove `CHANNELS` rows       |
| buttons wired to 5V with pull-downs         | `BUTTON_ACTIVE_LOW = false`, `BUTTON_USE_PULLUP = false` |
| lamps switch on with LOW (PNP, relay board) | `LIGHT_ACTIVE_HIGH = false`         |
| display on other pins                       | `DISPLAY_CLK_PIN`, `DISPLAY_DIO_PIN`|
| no buzzer / active buzzer module            | `BUZZER_PIN = -1` / `BUZZER_PASSIVE = false` |

Each `CHANNELS` row ties one lamp to the button under it: `{ "RED", 12, 3 }`
means the red lamp is on pin 12 and its button reads on pin 3. Then use the
wiring test mode below to confirm.

### Mapping a freshly wired device

`tools/` has a pin-test sketch and a serial helper for finding out which pin
drives which lamp and which pin each button reaches, one pin at a time. See
`tools/README.md`.

### Finding out which button is which

Three ways, pick any:

1. **Serial monitor** at 115200 baud prints the full pin map whenever the
   monitor is opened, plus every phase change and correct press.
2. **Boot lamp test**: each lamp lights in turn while the display shows its
   pin number (`L  6`, `L 13`, …).
3. **Wiring test mode**: hold any button while powering up. Now every button
   lights its own lamp while held and the display shows the button's pin
   (`b  2`). The serial monitor also prints `button pin 2 (RED) -> light pin 12`.
   Power-cycle to leave the mode.

## Playing

* **Attract**: the lamps run a show with soft fades: KITT scanner, cat creep, breathe,
  fill bar, outside-in pulse, sparkle, heartbeat, each followed by a one
  second dark pause (`animations.h`, easy to extend). The fades are also
  the tell that no game is running: in play lamps only snap on and off. The display alternates the last score with `bESt` and the
  stored high score. Press any button to start. After two idle minutes
  everything goes dark; a press wakes it and starts a game.
* **Start**: on the press every lamp rises from wherever the show left it
  to full, holds half a second, fades to dark, and after a random 0.5–2.5 s
  silence the first light comes.
* **Playing**: the display shows the running score. Each lamp lights for
  60 % of the current interval and then goes dark on its own; the press is
  still owed, so if you fall behind you play from memory. Pressing turns the
  lamp off immediately.
* **Game over**: on a wrong button, the lamp you should have pressed blinks
  for 1.5 s. On a too-early or too-slow loss all lamps blink. Then the score
  blinks. A new high score lights all lamps and pings on each blink. The
  high score survives power cycles (EEPROM).

## Tuning difficulty

In `config.h`:

* `START_INTERVAL_MS` (900): time between the first lights.
* `SPEEDUP_FACTOR` (0.985): each light multiplies the interval by this.
* `MIN_INTERVAL_MS` (120): the interval never goes below this.
* `MAX_PENDING` (5): how many unpressed lamps you may fall behind.
* `LIGHT_ON_PERCENT` (60): how long each lamp stays lit, as a share of the
  interval.

The comment above `SPEEDUP_FACTOR` has a table of simulated scores for
different player speeds.

## Files

* `nopeustesti.ino` – game state machine (boot, wiring test, attract,
  countdown, playing, game over).
* `config.h` – wiring (pins, polarities, buzzer type) and every tunable
  constant.
* `buttons.h` – polled, bounce-proof button. A press fires on the first
  couple of low samples; release needs 30 ms of stable high, so contact
  bounce never creates a phantom press. No interrupts, no shared lockout.
* `display.h` – 60-line TM1637 driver (digits, a small letter font,
  brightness, on/off).
* `animations.h` – idle lamp animations as pure time → lamp-level functions.
  Lamps on PWM pins fade in hardware, others with a software PWM.
* `flash.sh` – compile + upload + serial monitor via arduino-cli.
* `tools/` – pin-test sketch and serial helper for mapping a rewired device.
