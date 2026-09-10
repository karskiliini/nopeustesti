/**
 *  Idle (attract mode) lamp animations, with fades.
 *
 *  Each animation is a function of elapsed time that fills levels[] with a
 *  brightness 0..255 per lamp (index = CHANNELS row). The sketch plays them
 *  in order, fades each one out, keeps a dark pause, and loops for as long
 *  as the device waits for a game. Works for any number of lamps;
 *  positions follow the CHANNELS row order (left to right).
 *
 *  Nothing here is random: every animation is a smooth, repeatable motion.
 *  The soft look is what tells the player "not in a game": in play the
 *  lamps only ever snap fully on and off.
 *
 *  Conventions inside a frame function:
 *    - positions along the panel are in 1/256 lamp spacings: lamp i sits at
 *      i * LAMP, the panel runs from 0 to SPAN.
 *    - work in perceptual brightness and call gammaAll() last, so fades
 *      look linear to the eye.
 *
 *  Add an animation: write a frame function, add a row to ANIMATIONS.
 */
#pragma once

#include <Arduino.h>
#include "config.h"

typedef void (*AnimationFrame)(uint32_t t, uint8_t levels[]);

struct Animation {
  AnimationFrame frame;
  uint16_t durationMs;
};

// --- small integer helpers -------------------------------------------------

static const int32_t LAMP = 256;                                  // one lamp spacing
static const int32_t SPAN = (int32_t)(NUM_CHANNELS - 1) * LAMP;   // first to last lamp

// Triangle wave 0..255..0 over `period` ms.
static uint8_t tri(uint32_t t, uint16_t period) {
  const uint32_t p = t % period;
  const uint32_t half = period / 2;
  return p < half ? p * 255 / half : (period - p) * 255 / half;
}

// Sawtooth 0..255 over `period` ms.
static uint8_t saw(uint32_t t, uint16_t period) {
  return (t % period) * 255 / period;
}

// Smoothstep ease: turns a linear 0..255 ramp into a soft S-curve.
static uint8_t ease(uint8_t x) {
  const uint32_t xx = x;
  return (uint8_t)((3u * xx * xx * 255u - 2u * xx * xx * xx) / (255u * 255u));
}

// Perceptual correction: LEDs look linear only after squaring.
static uint8_t gammaLevel(uint8_t v) {
  return (uint8_t)(((uint16_t)v * v) / 255);
}

static uint8_t clamp255(int32_t v) {
  return v < 0 ? 0 : v > 255 ? 255 : (uint8_t)v;
}

// Eased 0..255 ramp over `len` ms starting at `start`; 0 before, 255 after.
static uint8_t ramp(int32_t t, int32_t start, int32_t len) {
  return ease(clamp255((t - start) * 255 / len));
}

static void clearLevels(uint8_t levels[]) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = 0;
}

static void gammaAll(uint8_t levels[]) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = gammaLevel(levels[i]);
}

// Glow around position `pos`: full at the spot, fading to dark `back` units
// to its left and `fwd` units to its right. `soft` uses an S-curve falloff
// instead of a straight one. Max-combined into levels[] so glows can overlap.
static void glow(uint8_t levels[], int32_t pos, uint16_t back, uint16_t fwd, bool soft = false) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const int32_t d = (int32_t)i * LAMP - pos;         // > 0: lamp is right of the spot
    const uint32_t ad = d < 0 ? -d : d;
    const uint16_t w = d < 0 ? back : fwd;
    if (ad >= w) continue;
    uint8_t v = (w - ad) * 255 / w;
    if (soft) v = ease(v);
    if (v > levels[i]) levels[i] = v;
  }
}

// Glow on a ring: the distance wraps, so a spot leaving the last lamp
// re-enters at the first.
static void ringGlow(uint8_t levels[], int32_t pos, uint16_t width) {
  const int32_t ring = (int32_t)NUM_CHANNELS * LAMP;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    int32_t d = ((int32_t)i * LAMP - pos) % ring;
    if (d < 0) d += ring;
    if (d > ring / 2) d = ring - d;
    if (d >= width) continue;
    const uint8_t v = (width - d) * 255 / width;
    if (v > levels[i]) levels[i] = v;
  }
}

// How full lamp i is behind a soft edge at `edge`: 0 until the edge reaches
// the lamp, 255 once it is a full lamp spacing past it.
static uint8_t fillAmount(int32_t edge, uint8_t i) {
  return ease(clamp255(edge - (int32_t)i * LAMP));
}

// Phosphor tail: each lamp holds the brightest value it has seen and decays
// from it linearly over `tailMs`. State survives between frames; a jump
// back in time (the animation restarted) clears it.
struct Persist {
  uint32_t lastT = 0xFFFFFFFF;
  uint8_t held[NUM_CHANNELS];
  void apply(uint32_t t, uint8_t levels[], uint16_t tailMs) {
    uint32_t dt = 0;
    if (t < lastT) { for (uint8_t i = 0; i < NUM_CHANNELS; i++) held[i] = 0; }
    else dt = t - lastT;
    lastT = t;
    const int32_t drop = (int32_t)dt * 255 / tailMs;
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
      const int32_t decayed = (int32_t)held[i] - drop;
      held[i] = levels[i] > decayed ? levels[i] : (uint8_t)decayed;
      levels[i] = held[i];
    }
  }
};

// --- scanners: one bright spot on the move ---------------------------------

// KITT scanner: a glowing spot sweeps left to right and back, neighbours
// glow as it approaches.
static void animKitt(uint32_t t, uint8_t levels[]) {
  clearLevels(levels);
  glow(levels, (int32_t)tri(t, ANIM_KITT_SWEEP_MS) * SPAN / 255, 300, 300);
  gammaAll(levels);
}

// Wide scanner: same sweep, but the glow spans most of the panel, so
// several lamps are lit at once and the brightest one leads the way.
static void animKittWide(uint32_t t, uint8_t levels[]) {
  clearLevels(levels);
  glow(levels, (int32_t)tri(t, ANIM_KITT_SWEEP_MS) * SPAN / 255, 640, 640);
  gammaAll(levels);
}

// Pendulum: the spot swings like a weight, slow at the ends and quick
// through the middle.
static void animPendulum(uint32_t t, uint8_t levels[]) {
  clearLevels(levels);
  glow(levels, (int32_t)ease(tri(t, ANIM_PENDULUM_PERIOD_MS)) * SPAN / 255, 300, 300);
  gammaAll(levels);
}

// Comet: a spot sweeps back and forth and leaves a fading tail behind it.
// The tail is time-based, so it turns around as smoothly as the head.
static void animComet(uint32_t t, uint8_t levels[]) {
  static Persist tail;
  clearLevels(levels);
  glow(levels, (int32_t)tri(t, ANIM_COMET_SWEEP_MS) * SPAN / 255, 200, 200);
  tail.apply(t, levels, ANIM_COMET_TAIL_MS);
  gammaAll(levels);
}

// Chase: a comet that always runs left to right. It enters from beyond the
// first lamp and leaves past the last while its tail is still dying out.
static void animChase(uint32_t t, uint8_t levels[]) {
  static Persist tail;
  const int32_t travel = SPAN + 2 * 200;
  const int32_t pos = -200 + (int32_t)saw(t, ANIM_CHASE_PASS_MS) * travel / 255;
  clearLevels(levels);
  glow(levels, pos, 200, 200);
  tail.apply(t, levels, ANIM_COMET_TAIL_MS);
  gammaAll(levels);
}

// Headlights: the spot glows well ahead of itself and hardly at all behind,
// so lamps brighten as it comes towards them and drop away once passed.
// The beam shortens to nothing at the ends of the sweep, so turning around
// does not make it flip.
static void animHeadlights(uint32_t t, uint8_t levels[]) {
  const uint32_t p = t % ANIM_COMET_SWEEP_MS;
  const bool rightward = p < ANIM_COMET_SWEEP_MS / 2;
  const int32_t pos = (int32_t)tri(t, ANIM_COMET_SWEEP_MS) * SPAN / 255;
  const uint16_t beam = 160 + (uint16_t)tri(t, ANIM_COMET_SWEEP_MS / 2) * (700 - 160) / 255;
  clearLevels(levels);
  glow(levels, pos, rightward ? 160 : beam, rightward ? beam : 160, true);
  gammaAll(levels);
}

// Crossing: two spots start at opposite ends, meet in the middle, pass
// through each other and return.
static void animCrossing(uint32_t t, uint8_t levels[]) {
  const int32_t pos = (int32_t)tri(t, ANIM_CROSSING_PERIOD_MS) * SPAN / 255;
  clearLevels(levels);
  glow(levels, pos, 300, 300);
  glow(levels, SPAN - pos, 300, 300);
  gammaAll(levels);
}

// Ring: the spot runs left to right and wraps straight from the last lamp
// back to the first, like a lap around a circle.
static void animRing(uint32_t t, uint8_t levels[]) {
  const int32_t pos = (int32_t)saw(t, ANIM_RING_LAP_MS) * (int32_t)NUM_CHANNELS * LAMP / 255;
  clearLevels(levels);
  ringGlow(levels, pos, 320);
  gammaAll(levels);
}

// Cat creep: one lamp slowly brightens; as it starts to fade the next one
// begins to glow, and so on across the board. One direction, then again.
static void animCreep(uint32_t t, uint8_t levels[]) {
  // Position runs from one lamp before the first to one past the last,
  // so the first lamp fades in from dark and the last fades out to dark.
  const int32_t travel = SPAN + 2 * LAMP;
  const int32_t pos = -LAMP + (int32_t)((t % ANIM_CREEP_SWEEP_MS) * travel / ANIM_CREEP_SWEEP_MS);
  clearLevels(levels);
  glow(levels, pos, LAMP, LAMP, true);
  gammaAll(levels);
}

// --- waves and pulses: every lamp moves, phase-shifted -----------------------

// Wave: a slow swell travels across the panel; each lamp is a quarter turn
// behind its neighbour, so all of them are always somewhere on the curve.
static void animWave(uint32_t t, uint8_t levels[]) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const uint32_t shifted = t + (uint32_t)(NUM_CHANNELS - 1 - i) * (ANIM_WAVE_PERIOD_MS / NUM_CHANNELS);
    levels[i] = ease(tri(shifted, ANIM_WAVE_PERIOD_MS));
  }
  gammaAll(levels);
}

// Outside-in pulse: outer lamps breathe, inner lamps follow half a beat later.
static void animOutsideIn(uint32_t t, uint8_t levels[]) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const uint8_t dist = i < NUM_CHANNELS - 1 - i ? i : NUM_CHANNELS - 1 - i;
    const uint32_t shifted = t + (uint32_t)dist * (ANIM_PULSE_PERIOD_MS / 2);
    levels[i] = ease(tri(shifted, ANIM_PULSE_PERIOD_MS));
  }
  gammaAll(levels);
}

// Inside-out pulse: the middle breathes first, the edges follow.
static void animInsideOut(uint32_t t, uint8_t levels[]) {
  const uint8_t maxDist = (NUM_CHANNELS - 1) / 2;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const uint8_t dist = i < NUM_CHANNELS - 1 - i ? i : NUM_CHANNELS - 1 - i;
    const uint32_t shifted = t + (uint32_t)(maxDist - dist) * (ANIM_PULSE_PERIOD_MS / 2);
    levels[i] = ease(tri(shifted, ANIM_PULSE_PERIOD_MS));
  }
  gammaAll(levels);
}

// Breathe: everything slowly rises and falls together.
static void animBreathe(uint32_t t, uint8_t levels[]) {
  const uint8_t v = ease(tri(t, ANIM_BREATHE_PERIOD_MS));
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = v;
  gammaAll(levels);
}

// Heartbeat: two beats, each a quick rise and a slower decay, then rest.
static uint8_t beat(int32_t p, int32_t start, int32_t rise, int32_t fall) {
  if (p < start) return 0;
  if (p < start + rise) return ramp(p, start, rise);
  return 255 - ramp(p, start + rise, fall);
}
static void animHeartbeat(uint32_t t, uint8_t levels[]) {
  const int32_t p = t % 1200;
  const uint8_t a = beat(p, 0, 80, 220);
  const uint8_t b = beat(p, 240, 80, 300);
  const uint8_t v = a > b ? a : b;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = v;
  gammaAll(levels);
}

// --- crossfades: two groups trade places -----------------------------------

// Group crossfade: group A rises first, then A fades as B rises, and so on.
// `inA(i)` says which lamps belong to A.
static void crossfade(uint32_t t, uint8_t levels[], bool (*inA)(uint8_t)) {
  const uint16_t P = ANIM_CROSSFADE_PERIOD_MS;
  const uint8_t a = ease(tri(t, P));
  const uint8_t b = t < P / 2 ? 0 : ease(tri(t - P / 2, P));
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = inA(i) ? a : b;
  gammaAll(levels);
}

static bool leftHalf(uint8_t i) { return i < NUM_CHANNELS / 2; }
static bool evenLamp(uint8_t i) { return (i & 1) == 0; }

// Halves: the left half and the right half take turns.
static void animHalves(uint32_t t, uint8_t levels[]) { crossfade(t, levels, leftHalf); }

// Odd-even: every other lamp, then the ones in between.
static void animOddEven(uint32_t t, uint8_t levels[]) { crossfade(t, levels, evenLamp); }

// --- fills and bars: a soft edge moves --------------------------------------

// Fill bar: a soft edge sweeps in from the left until all lamps glow,
// then sweeps out again.
static void animFill(uint32_t t, uint8_t levels[]) {
  const int32_t edge = (int32_t)tri(t, ANIM_FILL_SWEEP_MS) * (SPAN + LAMP) / 255;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = fillAmount(edge, i);
  gammaAll(levels);
}

// Fill from the centre: the middle lights first and the glow grows out to
// both ends, then shrinks back.
static void animFillCenter(uint32_t t, uint8_t levels[]) {
  const int32_t centre = SPAN / 2;
  const int32_t edge = (int32_t)tri(t, ANIM_FILL_SWEEP_MS) * (centre + LAMP) / 255;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const int32_t d = (int32_t)i * LAMP - centre;
    levels[i] = ease(clamp255(edge - (d < 0 ? -d : d)));
  }
  gammaAll(levels);
}

// Fill from both ends: the outer lamps light first and the glow closes in
// on the middle, then opens up again.
static void animFillEnds(uint32_t t, uint8_t levels[]) {
  const int32_t centre = SPAN / 2;
  const int32_t edge = (int32_t)tri(t, ANIM_FILL_SWEEP_MS) * (centre + LAMP) / 255;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const int32_t d = (int32_t)i * LAMP - centre;
    const int32_t fromEnd = centre - (d < 0 ? -d : d);
    levels[i] = ease(clamp255(edge - fromEnd));
  }
  gammaAll(levels);
}

// Worm: a bar grows in from the left, crawls across at a fixed length and
// shrinks out at the right.
static void animWorm(uint32_t t, uint8_t levels[]) {
  const int32_t length = 2 * LAMP;
  const int32_t travel = SPAN + LAMP + length;
  const int32_t head = (int32_t)((t % ANIM_WORM_PASS_MS) * travel / ANIM_WORM_PASS_MS);
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    levels[i] = fillAmount(head, i) - fillAmount(head - length, i);
  }
  gammaAll(levels);
}

// Stack: lamps come on one after another from the left and stay, until
// the row is full; then everything fades out together.
static void animStack(uint32_t t, uint8_t levels[]) {
  const int32_t S = ANIM_STACK_STEP_MS;
  const int32_t period = (NUM_CHANNELS + 3) * S;
  const int32_t p = t % period;
  const uint8_t out = 255 - ramp(p, (NUM_CHANNELS + 1) * S, 2 * S);
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    levels[i] = (uint16_t)ramp(p, i * S, S) * out / 255;
  }
  gammaAll(levels);
}

// Cascade: the whole row lights, then goes out lamp by lamp from the left
// with overlapping fades, then comes back the same way.
static void animCascade(uint32_t t, uint8_t levels[]) {
  const int32_t S = ANIM_CASCADE_STEP_MS;
  const int32_t sweep = (NUM_CHANNELS + 1) * S;
  const int32_t period = 2 * sweep + 2 * S;
  const int32_t p = t % period;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    uint8_t v;
    if (p < S)                       v = ramp(p, 0, S);                            // all rise
    else if (p < S + sweep)          v = 255 - ramp(p - S, i * S, 2 * S);          // out, left to right
    else if (p < S + 2 * sweep)      v = ramp(p - S - sweep, i * S, 2 * S);        // in, left to right
    else                             v = 255 - ramp(p - S - 2 * sweep, 0, S);      // all fall
    levels[i] = v;
  }
  gammaAll(levels);
}

// --- the show ----------------------------------------------------------------

static const Animation ANIMATIONS[] = {
  { animKitt,       4 * ANIM_KITT_SWEEP_MS },
  { animWave,       3 * ANIM_WAVE_PERIOD_MS },
  { animComet,      3 * ANIM_COMET_SWEEP_MS },
  { animFill,       2 * ANIM_FILL_SWEEP_MS },
  { animCreep,      2 * ANIM_CREEP_SWEEP_MS },
  { animCrossing,   3 * ANIM_CROSSING_PERIOD_MS },
  { animBreathe,    2 * ANIM_BREATHE_PERIOD_MS },
  { animChase,      5 * ANIM_CHASE_PASS_MS },
  { animFillCenter, 2 * ANIM_FILL_SWEEP_MS },
  { animKittWide,   4 * ANIM_KITT_SWEEP_MS },
  { animOutsideIn,  4 * ANIM_PULSE_PERIOD_MS },
  { animWorm,       3 * ANIM_WORM_PASS_MS },
  { animHalves,     3 * ANIM_CROSSFADE_PERIOD_MS },
  { animHeadlights, 3 * ANIM_COMET_SWEEP_MS },
  { animStack,      2 * (NUM_CHANNELS + 3) * ANIM_STACK_STEP_MS },
  { animPendulum,   3 * ANIM_PENDULUM_PERIOD_MS },
  { animFillEnds,   2 * ANIM_FILL_SWEEP_MS },
  { animRing,       5 * ANIM_RING_LAP_MS },
  { animInsideOut,  4 * ANIM_PULSE_PERIOD_MS },
  { animCascade,    2 * (2 * (NUM_CHANNELS + 1) + 2) * ANIM_CASCADE_STEP_MS },
  { animOddEven,    3 * ANIM_CROSSFADE_PERIOD_MS },
  { animHeartbeat,  3 * 1200 },
};
static const uint8_t NUM_ANIMATIONS = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);
