/**
 *  NOPEUSTESTI / SPEEDTEST
 *
 *  The classic Finnish reaction game: four lights, four buttons, one
 *  4-digit display. Lights come on in random order, faster and faster.
 *  Each light stays on only briefly; press the buttons in the same order
 *  the lights came on, from memory if you fall behind. One wrong press,
 *  one press too early, or falling more than MAX_PENDING lights behind
 *  ends the game. Score = number of correct presses.
 *
 *  (C) Tero Maaranen 2022-2026. GPL-3.0, see LICENSE.
 *
 *  Board: Arduino Leonardo. No external libraries needed.
 *  Wiring, pin map and tuning live in config.h.
 *
 *  Phases:
 *    LAMP TEST   boot: each lamp lights while the display shows its pin
 *    WIRING TEST hold any button at power-up: buttons echo to their lamps
 *    ATTRACT     fading lamp animations with pauses, last and best score
 *    COUNTDOWN   after a press: all lamps flash, fade out, random silence
 *    PLAYING     the game
 *    GAME OVER   fast soft pulses, show score, back to ATTRACT
 */

#include <EEPROM.h>
#include "config.h"
#include "buttons.h"
#include "display.h"
#include "animations.h"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------

static Button buttons[NUM_CHANNELS];
static TM1637Display display(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);

// Lamp levels 0..255 after the per-lamp brightness from config.h. Pins
// with hardware PWM use analogWrite; a lamp on a plain digital pin is
// simply on when the level is at least half, off below that.
static uint8_t lampLevel[NUM_CHANNELS];     // after brightness scaling
static uint8_t lampRequest[NUM_CHANNELS];   // as asked, 0..255

static void writeLampPin(uint8_t ch, bool on) {
  digitalWrite(CHANNELS[ch].lightPin, (on == LIGHT_ACTIVE_HIGH) ? HIGH : LOW);
}

static void setLampLevel(uint8_t ch, uint8_t level) {
  const uint8_t pin = CHANNELS[ch].lightPin;
  lampRequest[ch] = level;
  level = (uint16_t)level * CHANNELS[ch].brightness / 255;
  lampLevel[ch] = level;
  if (level == 0 || level == 255) {
    writeLampPin(ch, level == 255);
  } else if (digitalPinHasPWM(pin)) {
    analogWrite(pin, LIGHT_ACTIVE_HIGH ? level : 255 - level);
  } else {
    writeLampPin(ch, level >= 128);
  }
}

static void setLight(uint8_t ch, bool on) { setLampLevel(ch, on ? 255 : 0); }

static void setAllLights(bool on) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) setLight(i, on);
}

// Passive buzzer: play the tone. Active buzzer: switch on for the duration
// (pitch is ignored); tone() is still used so it turns itself off.
static void beep(uint16_t hz, uint16_t ms) {
  if (BUZZER_PIN < 0) return;
  tone(BUZZER_PIN, BUZZER_PASSIVE ? hz : 4000, ms);
}

// ---------------------------------------------------------------------------
// High score (EEPROM)
// ---------------------------------------------------------------------------

static const int EEPROM_MAGIC_ADDR = 0;
static const int EEPROM_BEST_ADDR = 1;
static const uint8_t EEPROM_MAGIC = 0x5E;

static uint16_t loadBest() {
  if (EEPROM.read(EEPROM_MAGIC_ADDR) != EEPROM_MAGIC) return 0;
  uint16_t best = 0;
  EEPROM.get(EEPROM_BEST_ADDR, best);
  return best > 9999 ? 0 : best;
}

static void saveBest(uint16_t best) {
  EEPROM.update(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);
  EEPROM.put(EEPROM_BEST_ADDR, best);
}

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------

enum class Phase : uint8_t { WiringTest, Attract, Countdown, Playing, GameOver };
enum class LossReason : uint8_t { WrongButton, TooEarly, TooSlow };

// Explicit prototypes: the Arduino builder's generated ones would land
// above the enums and fail to compile.
static void enterPhase(Phase p, uint32_t now);
static const char *phaseName(Phase p);
static void loseGame(LossReason reason, uint8_t pressed, uint8_t expected, uint32_t now);

static Phase phase = Phase::Attract;
static uint32_t phaseStart = 0;   // millis() when the current phase began
static bool phaseFresh = true;    // true on the first loop() of a new phase

static uint16_t score = 0;
static uint16_t best = 0;
static bool played = false;       // has a game been played since boot
static bool newBest = false;

// Queue of lights that are lit and not yet pressed, oldest first.
static uint8_t queue[MAX_PENDING];
static uint8_t qHead = 0;
static uint8_t qCount = 0;
static uint8_t pending[NUM_CHANNELS];     // per colour count in the queue
static uint32_t litUntil[NUM_CHANNELS];   // lamp goes dark at this time
static float interval = START_INTERVAL_MS;
static uint32_t nextLightAt = 0;

static LossReason lossReason;
static uint8_t lossPressed = 0;
static uint8_t lossExpected = 0;

static inline uint32_t sincePhase(uint32_t now) { return now - phaseStart; }
static inline bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

static const char *phaseName(Phase p) {
  switch (p) {
    case Phase::WiringTest: return "WIRING TEST";
    case Phase::Attract:    return "ATTRACT";
    case Phase::Countdown:  return "COUNTDOWN";
    case Phase::Playing:    return "PLAYING";
    case Phase::GameOver:   return "GAME OVER";
  }
  return "?";
}

static void enterPhase(Phase p, uint32_t now) {
  phase = p;
  phaseStart = now;
  phaseFresh = true;
  Serial.print(F("phase: "));
  Serial.println(phaseName(p));
}

// Returns true exactly once after each enterPhase().
static bool freshPhase() {
  const bool f = phaseFresh;
  phaseFresh = false;
  return f;
}

// ---------------------------------------------------------------------------
// Serial helpers
// ---------------------------------------------------------------------------

static void printPinMap() {
  Serial.println(F("NOPEUSTESTI pin map"));
  Serial.println(F("  colour   light  button"));
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    Serial.print(F("  "));
    Serial.print(CHANNELS[i].name);
    for (uint8_t s = strlen(CHANNELS[i].name); s < 9; s++) Serial.print(' ');
    Serial.print(CHANNELS[i].lightPin);
    Serial.print(F("     "));
    Serial.println(CHANNELS[i].buttonPin);
  }
  Serial.print(F("  display  CLK "));
  Serial.print(DISPLAY_CLK_PIN);
  Serial.print(F(", DIO "));
  Serial.println(DISPLAY_DIO_PIN);
  Serial.print(F("  buzzer   "));
  if (BUZZER_PIN >= 0) {
    Serial.print(BUZZER_PIN);
    Serial.println(BUZZER_PASSIVE ? F(" (passive)") : F(" (active)"));
  } else {
    Serial.println(F("none"));
  }
  Serial.print(F("  buttons  pressed = "));
  Serial.print(BUTTON_ACTIVE_LOW ? F("LOW") : F("HIGH"));
  Serial.println(BUTTON_USE_PULLUP ? F(", internal pull-up") : F(", no pull-up"));
  Serial.print(F("  lamps    on = "));
  Serial.println(LIGHT_ACTIVE_HIGH ? F("HIGH") : F("LOW"));
  Serial.print(F("  best score "));
  Serial.println(best);
  Serial.println(F("Hold any button at power-up for wiring test mode."));
}

// ---------------------------------------------------------------------------
// Boot
// ---------------------------------------------------------------------------

static void lampTest() {
  char text[5];
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    setAllLights(false);
    setLight(i, true);
    snprintf(text, sizeof(text), "L%3d", CHANNELS[i].lightPin);
    display.showText(text);
    delay(LAMP_TEST_STEP_MS);
  }
  setAllLights(true);
  display.showText("8888");
  delay(LAMP_TEST_STEP_MS);
  setAllLights(false);
  display.clear();
}

void setup() {
  Serial.begin(115200);   // USB CDC on Leonardo: never wait for it

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    pinMode(CHANNELS[i].lightPin, OUTPUT);
    setLight(i, false);
    buttons[i].begin(CHANNELS[i].buttonPin);
  }
  if (BUZZER_PIN >= 0) pinMode(BUZZER_PIN, OUTPUT);

  display.begin(DISPLAY_BRIGHTNESS);

#if RESET_HIGH_SCORE_ON_BOOT
  saveBest(0);
#endif
  best = loadBest();
  printPinMap();

  bool anyHeld = false;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) anyHeld |= buttons[i].rawDown();

  lampTest();

  const uint32_t now = millis();
  if (anyHeld) {
    Serial.println(F("WIRING TEST mode. Press a button: its lamp lights, display shows the button pin."));
    display.showText("tESt");
    enterPhase(Phase::WiringTest, now);
  } else {
    enterPhase(Phase::Attract, now);
  }
}

// ---------------------------------------------------------------------------
// Wiring test
// ---------------------------------------------------------------------------

static void runWiringTest(uint32_t now) {
  static int8_t shown = -1;
  freshPhase();
  int8_t held = -1;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (buttons[i].poll(now)) {
      Serial.print(F("button pin "));
      Serial.print(CHANNELS[i].buttonPin);
      Serial.print(F(" ("));
      Serial.print(CHANNELS[i].name);
      Serial.print(F(") -> light pin "));
      Serial.println(CHANNELS[i].lightPin);
    }
    setLight(i, buttons[i].isHeld());
    if (buttons[i].isHeld()) held = i;
  }
  if (held != shown) {
    shown = held;
    if (held < 0) {
      display.showText("tESt");
    } else {
      char text[5];
      snprintf(text, sizeof(text), "b%3d", CHANNELS[held].buttonPin);
      display.showText(text);
    }
  }
}

// ---------------------------------------------------------------------------
// Attract
// ---------------------------------------------------------------------------

static void showLevels(const uint8_t levels[]) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (levels[i] != lampRequest[i]) setLampLevel(i, levels[i]);
  }
}

// ATTRACT_SLEEP_MS 0 = never sleep. (Written as "idle + 1 > limit" rather
// than "idle >= limit" only to keep -Wtype-limits quiet when limit is 0.)
static bool sleepDue(uint32_t idle) {
  return ATTRACT_SLEEP_MS != 0 && idle + 1 > ATTRACT_SLEEP_MS;
}

static void runAttract(uint32_t now, bool anyPress) {
  static uint8_t animIndex = 0;
  static uint32_t animStart = 0;
  static uint32_t lastFrame = 0;
  static bool pausing = false;
  static bool fading = false;     // animation over, fading the lamps to dark
  static uint8_t fadeFrom[NUM_CHANNELS];
  static int8_t shownScreen = -1;
  static bool asleep = false;

  if (freshPhase()) {
    animIndex = 0;
    animStart = now;
    lastFrame = 0;
    pausing = true;               // short dark pause before the first show
    fading = false;
    shownScreen = -1;
    asleep = false;
    setAllLights(false);
    display.on();
  }

  if (anyPress) {
    enterPhase(Phase::Countdown, now);
    return;
  }

  if (!asleep && sleepDue(sincePhase(now))) {
    asleep = true;
    setAllLights(false);
    display.off();
    Serial.println(F("idle: sleeping"));
    return;
  }
  if (asleep) return;

  // Animation show: one animation, fade out, a dark pause, the next, ...
  // and loop for as long as nobody presses a button. The first frames are
  // faded in and the last levels faded out so every animation starts and
  // ends softly whatever it was doing.
  const uint32_t at = now - animStart;
  if (pausing) {
    if (at >= ATTRACT_PAUSE_MS) {
      pausing = false;
      animStart = now;
    }
  } else if (fading) {
    if (at >= ATTRACT_FADE_MS) {
      fading = false;
      pausing = true;
      animStart = now;
      setAllLights(false);
    } else if (now - lastFrame >= 8) {
      lastFrame = now;
      const uint8_t f = 255 - ease(at * 255 / ATTRACT_FADE_MS);
      uint8_t levels[NUM_CHANNELS];
      for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = (uint16_t)fadeFrom[i] * f / 255;
      showLevels(levels);
    }
  } else if (at >= ANIMATIONS[animIndex].durationMs) {
    fading = true;
    animStart = now;
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) fadeFrom[i] = lampRequest[i];
    animIndex = (animIndex + 1) % NUM_ANIMATIONS;
  } else if (now - lastFrame >= 8) {   // ~120 frames/s is plenty
    lastFrame = now;
    uint8_t levels[NUM_CHANNELS];
    ANIMATIONS[animIndex].frame(at, levels);
    if (at < ATTRACT_FADE_MS) {
      const uint8_t f = ease(at * 255 / ATTRACT_FADE_MS);
      for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = (uint16_t)levels[i] * f / 255;
    }
    showLevels(levels);
  }

  // Display cycle: last score -> "bESt" -> best score -> ...
  const uint32_t cycle = ATTRACT_SHOW_SCORE_MS + ATTRACT_SHOW_BEST_MS * 2;
  const uint32_t t = sincePhase(now) % cycle;
  int8_t screen;
  if (best == 0 || t < ATTRACT_SHOW_SCORE_MS) screen = 0;
  else if (t < ATTRACT_SHOW_SCORE_MS + ATTRACT_SHOW_BEST_MS) screen = 1;
  else screen = 2;

  if (screen != shownScreen) {
    shownScreen = screen;
    switch (screen) {
      case 0: if (played) display.showNumber(score); else display.showText("PLAY"); break;
      case 1: display.showText("bESt"); break;
      default: display.showNumber(best); break;
    }
  }
}

// ---------------------------------------------------------------------------
// Countdown
// ---------------------------------------------------------------------------

static void startGame(uint32_t now) {
  randomSeed(micros() ^ ((uint32_t)analogRead(A0) << 16) ^ now);
  score = 0;
  qHead = 0;
  qCount = 0;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    pending[i] = 0;
    litUntil[i] = now;
  }
  interval = START_INTERVAL_MS;
  nextLightAt = now;            // the start sequence already made us wait
  newBest = false;
  played = true;
  setAllLights(false);
  display.showNumber(0);
  Serial.println(F("GO"));
  enterPhase(Phase::Playing, now);
}

// Start sequence: every lamp rises from wherever the attract show left it
// to full, holds, fades to dark, then a random silence so the first light
// cannot be anticipated.
static void runCountdown(uint32_t now) {
  static uint16_t waitMs = 0;
  static uint8_t from[NUM_CHANNELS];
  if (freshPhase()) {
    display.on();               // may have been asleep
    display.showNumber(0);
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) from[i] = lampRequest[i];
    waitMs = random(START_WAIT_MIN_MS, START_WAIT_MAX_MS + 1);
    beep(660, 60);
  }

  const uint32_t t = sincePhase(now);
  uint8_t levels[NUM_CHANNELS];
  if (t < START_RISE_MS) {
    const uint8_t f = ease(t * 255 / START_RISE_MS);
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
      levels[i] = from[i] + (uint16_t)(255 - from[i]) * f / 255;
    }
  } else if (t < START_RISE_MS + START_FLASH_MS) {
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = 255;
  } else if (t < START_RISE_MS + START_FLASH_MS + START_FADE_MS) {
    const uint32_t f = t - START_RISE_MS - START_FLASH_MS;
    const uint8_t lin = 255 - f * 255 / START_FADE_MS;
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = gammaLevel(lin);
  } else if (t < START_RISE_MS + START_FLASH_MS + START_FADE_MS + waitMs) {
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = 0;
  } else {
    startGame(now);
    return;
  }
  showLevels(levels);
}

// ---------------------------------------------------------------------------
// Playing
// ---------------------------------------------------------------------------

static void loseGame(LossReason reason, uint8_t pressed, uint8_t expected, uint32_t now) {
  lossReason = reason;
  lossPressed = pressed;
  lossExpected = expected;

  if (score > best) {
    best = score;
    newBest = true;
    saveBest(best);
  }

  Serial.print(F("GAME OVER: "));
  switch (reason) {
    case LossReason::WrongButton:
      Serial.print(F("wrong button (pressed "));
      Serial.print(CHANNELS[pressed].name);
      Serial.print(F(", expected "));
      Serial.print(CHANNELS[expected].name);
      Serial.print(')');
      break;
    case LossReason::TooEarly:
      Serial.print(F("pressed "));
      Serial.print(CHANNELS[pressed].name);
      Serial.print(F(" with no light on"));
      break;
    case LossReason::TooSlow:
      Serial.print(F("too slow, "));
      Serial.print(MAX_PENDING);
      Serial.print(F(" lights waiting"));
      break;
  }
  Serial.print(F(". score="));
  Serial.print(score);
  Serial.print(F(" best="));
  Serial.print(best);
  if (newBest) Serial.print(F(" NEW BEST"));
  Serial.println();

  beep(110, 700);
  display.showNumber(score);
  enterPhase(Phase::GameOver, now);
}

static void refreshLamps(uint32_t now) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    setLight(i, pending[i] > 0 && !reached(now, litUntil[i]));
  }
}

static void runPlaying(uint32_t now) {
  freshPhase();   // no per-phase local state to reset

  // Button presses
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (!buttons[i].poll(now)) continue;

    if (qCount == 0) {
      loseGame(LossReason::TooEarly, i, 0, now);
      return;
    }
    const uint8_t expected = queue[qHead];
    if (expected != i) {
      loseGame(LossReason::WrongButton, i, expected, now);
      return;
    }
    qHead = (qHead + 1) % MAX_PENDING;
    qCount--;
    if (--pending[i] == 0) litUntil[i] = now;   // lamp off on the press
    score++;
    display.showNumber(score);
    Serial.print(F("ok "));
    Serial.println(CHANNELS[i].name);
  }

  // Next light
  if (reached(now, nextLightAt)) {
    if (qCount == MAX_PENDING) {
      loseGame(LossReason::TooSlow, 0, queue[qHead], now);
      return;
    }
    const uint8_t colour = random(NUM_CHANNELS);
    queue[(qHead + qCount) % MAX_PENDING] = colour;
    qCount++;
    pending[colour]++;
    interval *= SPEEDUP_FACTOR;
    if (interval < MIN_INTERVAL_MS) interval = MIN_INTERVAL_MS;
    nextLightAt = now + (uint32_t)interval;
    uint32_t onMs = (uint32_t)interval * LIGHT_ON_PERCENT / 100;
    if (onMs < LIGHT_ON_MIN_MS) onMs = LIGHT_ON_MIN_MS;
    litUntil[colour] = now + onMs;
  }

  refreshLamps(now);
}

// ---------------------------------------------------------------------------
// Game over
// ---------------------------------------------------------------------------

static void runGameOver(uint32_t now, bool anyPress) {
  static int8_t shownBreath = -1;
  if (freshPhase()) shownBreath = -1;

  const uint32_t t = sincePhase(now);

  if (t < GAME_OVER_LOCKOUT_MS) {
    // Fast soft pulses: on a wrong button, the lamp you should have hit
    // pulses alone; otherwise everything pulses. In play the lamps only
    // ever snap on and off, so a fading lamp says "game over" at once.
    // Presses are ignored.
    const uint8_t v = gammaLevel(ease(tri(t, GAME_OVER_PULSE_MS)));
    uint8_t levels[NUM_CHANNELS];
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
      levels[i] = (lossReason != LossReason::WrongButton || i == lossExpected) ? v : 0;
    }
    showLevels(levels);
    return;
  }

  // Show the score while every lamp breathes up and down a few times; a
  // new best gets a rising ping at the start of each breath. Then hand
  // over to the attract show, whose own dark pause comes first.
  const uint32_t bt = t - GAME_OVER_LOCKOUT_MS;
  const int8_t breath = bt / GAME_OVER_BREATHE_MS;
  if (breath != shownBreath) {
    shownBreath = breath;
    if (breath == 0) display.showNumber(score);
    if (newBest) beep(1319, 40);
  }

  if (anyPress) {
    enterPhase(Phase::Countdown, now);
    return;
  }
  if (breath >= GAME_OVER_BREATHS) {
    enterPhase(Phase::Attract, now);
    return;
  }
  uint8_t levels[NUM_CHANNELS];
  const uint8_t v = gammaLevel(ease(tri(bt, GAME_OVER_BREATHE_MS)));
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = v;
  showLevels(levels);
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

// Leonardo's USB serial has no boot-time output for a monitor opened later,
// so repeat the pin map whenever a monitor connects.
static void serialGreeting() {
  static bool wasOpen = false;
  const bool open = (bool)Serial;
  if (open && !wasOpen) {
    printPinMap();
    Serial.print(F("phase: "));
    Serial.println(phaseName(phase));
  }
  wasOpen = open;
}

void loop() {
  const uint32_t now = millis();
  serialGreeting();

  if (phase == Phase::WiringTest) { runWiringTest(now); return; }
  if (phase == Phase::Playing)    { runPlaying(now);    return; }

  // All other phases only care whether any button was pressed.
  bool anyPress = false;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    if (buttons[i].poll(now)) {
      anyPress = true;
      Serial.print(F("press "));
      Serial.print(CHANNELS[i].name);
      Serial.print(F(" (pin "));
      Serial.print(CHANNELS[i].buttonPin);
      Serial.println(')');
    }
  }

  switch (phase) {
    case Phase::Attract:   runAttract(now, anyPress);  break;
    case Phase::Countdown: runCountdown(now);          break;
    case Phase::GameOver:  runGameOver(now, anyPress); break;
    default: break;
  }
}
