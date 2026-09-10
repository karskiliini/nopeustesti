/**
 *  NOPEUSTESTI / SPEEDTEST
 *
 *  The classic Finnish reaction game: four lights, four buttons, one
 *  4-digit display. Lights come on in random order, faster and faster.
 *  Press the buttons in the same order the lights came on. One wrong
 *  press, one press too early, or falling more than MAX_PENDING lights
 *  behind ends the game. Score = number of correct presses.
 *
 *  (C) Tero Maaranen 2022-2026. GPL-3.0, see LICENSE.
 *
 *  Board: Arduino Leonardo. No external libraries needed.
 *  Wiring, pin map and tuning live in config.h.
 *
 *  Phases:
 *    LAMP TEST   boot: each lamp lights while the display shows its pin
 *    WIRING TEST hold any button at power-up: buttons echo to their lamps
 *    ATTRACT     idle chase, shows last score and best score, sleeps later
 *    COUNTDOWN   3-2-1 after any button press
 *    PLAYING     the game
 *    GAME OVER   flash, show score, back to ATTRACT
 */

#include <EEPROM.h>
#include "config.h"
#include "buttons.h"
#include "display.h"

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------

static Button buttons[NUM_CHANNELS];
static TM1637Display display(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);

static void setLight(uint8_t ch, bool on) {
  digitalWrite(CHANNELS[ch].lightPin, (on == LIGHT_ACTIVE_HIGH) ? HIGH : LOW);
}

static void setAllLights(bool on) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) setLight(i, on);
}

static void beep(uint16_t hz, uint16_t ms) {
  if (BUZZER_PIN >= 0) tone(BUZZER_PIN, hz, ms);
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
static uint32_t lampOffUntil[NUM_CHANNELS]; // retrigger blink
static float interval = START_INTERVAL_MS;
static uint32_t nextLightAt = 0;

static LossReason lossReason;
static uint8_t lossPressed = 0;
static uint8_t lossExpected = 0;

static inline uint32_t sincePhase(uint32_t now) { return now - phaseStart; }
static inline bool reached(uint32_t now, uint32_t t) { return (int32_t)(now - t) >= 0; }

static void enterPhase(Phase p, uint32_t now) {
  phase = p;
  phaseStart = now;
  phaseFresh = true;
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
  if (BUZZER_PIN >= 0) Serial.println(BUZZER_PIN); else Serial.println(F("none"));
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

static void runAttract(uint32_t now, bool anyPress) {
  static uint8_t chaseIndex = 0;
  static uint32_t chaseAt = 0;
  static int8_t shownScreen = -1;
  static bool asleep = false;

  if (freshPhase()) {
    chaseAt = now;
    shownScreen = -1;
    asleep = false;
    display.on();
  }

  if (anyPress) {
    enterPhase(Phase::Countdown, now);
    return;
  }

  if (!asleep && sincePhase(now) >= ATTRACT_SLEEP_MS) {
    asleep = true;
    setAllLights(false);
    display.off();
    Serial.println(F("idle: sleeping"));
    return;
  }
  if (asleep) return;

  // Slow chase around the lamps.
  if (reached(now, chaseAt)) {
    chaseAt = now + ATTRACT_CHASE_MS;
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) setLight(i, i == chaseIndex);
    chaseIndex = (chaseIndex + 1) % NUM_CHANNELS;
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
    lampOffUntil[i] = now;
  }
  interval = START_INTERVAL_MS;
  nextLightAt = now + START_INTERVAL_MS;
  newBest = false;
  played = true;
  setAllLights(false);
  display.showNumber(0);
  Serial.println(F("GO"));
  enterPhase(Phase::Playing, now);
}

static void runCountdown(uint32_t now) {
  static int8_t shownStep = -1;
  if (freshPhase()) {
    shownStep = -1;
    display.on();   // may have been asleep
  }

  const uint8_t step = sincePhase(now) / COUNTDOWN_STEP_MS;   // 0,1,2 -> 3,2,1
  if (step >= 3) {
    beep(880, 120);
    startGame(now);
    return;
  }
  if (step != shownStep) {
    shownStep = step;
    char text[5];
    snprintf(text, sizeof(text), "  %d ", 3 - step);
    display.showText(text);
    // Lamps count down too: 3 lit, then 2, then 1.
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) setLight(i, i < (uint8_t)(3 - step));
    beep(660, 60);
  }
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
    setLight(i, pending[i] > 0 && reached(now, lampOffUntil[i]));
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
    pending[i]--;
    score++;
    display.showNumber(score);
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
    if (pending[colour]++ > 0) {
      lampOffUntil[colour] = now + RETRIGGER_GAP_MS;   // blink to show repeat
    }
    interval *= SPEEDUP_FACTOR;
    if (interval < MIN_INTERVAL_MS) interval = MIN_INTERVAL_MS;
    nextLightAt = now + (uint32_t)interval;
  }

  refreshLamps(now);
}

// ---------------------------------------------------------------------------
// Game over
// ---------------------------------------------------------------------------

static void runGameOver(uint32_t now, bool anyPress) {
  static int8_t shownBlink = -1;
  static bool lampsOn = false;
  if (freshPhase()) { shownBlink = -1; lampsOn = false; }

  const uint32_t t = sincePhase(now);

  if (t < GAME_OVER_LOCKOUT_MS) {
    // Fast flash: on a wrong button, the lamp you should have hit blinks
    // alone; otherwise everything blinks. Presses are ignored.
    const bool on = (t / 100) % 2 == 0;
    if (on != lampsOn) {
      lampsOn = on;
      if (lossReason == LossReason::WrongButton) {
        setAllLights(false);
        setLight(lossExpected, on);
      } else {
        setAllLights(on);
      }
    }
    return;
  }

  // Blink the score; a new best gets a rising ping on every blink.
  const int8_t blink = (t - GAME_OVER_LOCKOUT_MS) / GAME_OVER_BLINK_MS;
  if (blink != shownBlink) {
    shownBlink = blink;
    const bool visible = blink % 2 == 0;
    if (visible) display.showNumber(score); else display.clear();
    setAllLights(visible && newBest);
    if (visible && newBest) beep(1319, 40);
  }

  if (anyPress) {
    enterPhase(Phase::Countdown, now);
    return;
  }
  if (blink >= GAME_OVER_BLINKS * 2) {
    display.showNumber(score);
    enterPhase(Phase::Attract, now);
  }
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void loop() {
  const uint32_t now = millis();

  if (phase == Phase::WiringTest) { runWiringTest(now); return; }
  if (phase == Phase::Playing)    { runPlaying(now);    return; }

  // All other phases only care whether any button was pressed.
  bool anyPress = false;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) anyPress |= buttons[i].poll(now);

  switch (phase) {
    case Phase::Attract:   runAttract(now, anyPress);  break;
    case Phase::Countdown: runCountdown(now);          break;
    case Phase::GameOver:  runGameOver(now, anyPress); break;
    default: break;
  }
}
