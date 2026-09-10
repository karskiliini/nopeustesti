# Wiring tools

Use these when the device has been (re)wired and you need to find out which
pin drives which lamp and which pin each button reaches. No pyserial or IDE
needed, only arduino-cli.

## 1. Flash the pin test sketch

```
arduino-cli compile --fqbn arduino:avr:leonardo tools/pintest
arduino-cli upload  --fqbn arduino:avr:leonardo --port /dev/cu.usbmodemXXXX tools/pintest
```

(`arduino-cli board list` shows the port.)

## 2. Talk to it

`tools/sercmd.py COMMAND [SECONDS]` sends one line and prints replies for
SECONDS. Commands understood by pintest:

| command | effect |
|---------|--------|
| `6`     | blink pin 6 at 2 Hz until told otherwise (any pin number works, A0 = 18 … A5 = 23) |
| `s`     | stop blinking |
| `a`     | sweep: flash every candidate pin three times in turn |
| `p`     | print the current level (H/L) of every candidate pin, with internal pull-ups on |
| `q`     | probe: for each pin report `UP` if something external pulls it high, `DOWN` if pulled low, `-` if floating. Detects buttons wired to GND *and* to +5 V. |
| (empty) | just listen; pintest reports every pin that changes level |

### Map the lamps

```
tools/sercmd.py 6 1      # "which lamp blinks?"  -> note colour + position
tools/sercmd.py 7 1
...
tools/sercmd.py s 1
```

Lamps on pins that are *not* being blinked may glow faintly: that is the
internal pull-up leaking through the LED, not a wiring fault.

### Map the buttons

Hold one button and run `tools/sercmd.py q 1.5`. The pin that changes from
`-` to `DOWN` (button to GND) or `UP` (button to +5 V) is that button's pin.
Alternatively run `tools/sercmd.py "" 15` and press buttons; every edge is
printed as `pin N -> LOW` / `-> HIGH`.

Pin 13 always shows `DOWN` because of the Leonardo's on-board LED.

## 3. Write the result into `config.h`

Fill the `CHANNELS` table (one row per lamp: name, lamp pin, button pin, in
left-to-right panel order) and set `BUTTON_ACTIVE_LOW` / `LIGHT_ACTIVE_HIGH`
to match. Then `./flash.sh` and, to double-check, hold any button while
powering up: the game's own wiring test mode lights each button's lamp and
shows its pin on the display.
