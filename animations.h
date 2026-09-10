/**
 *  Idle (attract mode) lamp animations, with fades.
 *
 *  Each animation is a function of elapsed time that fills levels[] with a
 *  brightness 0..255 per lamp (index = CHANNELS row). The sketch plays them
 *  in order with a dark pause between, and loops. Works for any number of
 *  lamps; positions follow the CHANNELS row order (left to right).
 *
 *  The soft look is what tells the player "not in a game": in play the
 *  lamps only ever snap fully on and off.
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

// Triangle wave 0..255..0 over `period` ms.
static uint8_t tri(uint32_t t, uint16_t period) {
  const uint32_t p = t % period;
  const uint32_t half = period / 2;
  return p < half ? p * 255 / half : (period - p) * 255 / half;
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

// --- animations ------------------------------------------------------------

// KITT scanner: a glowing spot sweeps left to right and back with a tail.
static void animKitt(uint32_t t, uint8_t levels[]) {
  const uint16_t span = (NUM_CHANNELS - 1) * 256;            // lamp positions in 1/256
  const uint32_t pos = (uint32_t)tri(t, ANIM_KITT_SWEEP_MS) * span / 255;
  const uint16_t width = 300;                                 // glow radius
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const int32_t d = (int32_t)i * 256 - (int32_t)pos;
    const uint32_t ad = d < 0 ? -d : d;
    levels[i] = ad >= width ? 0 : gammaLevel((width - ad) * 255 / width);
  }
}

// Fill bar: a soft edge sweeps in from the left until all lamps glow,
// then sweeps out again.
static void animFill(uint32_t t, uint8_t levels[]) {
  const uint32_t span = (uint32_t)NUM_CHANNELS * 256 + 256;
  const uint32_t edge = (uint32_t)tri(t, ANIM_FILL_SWEEP_MS) * span / 255;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const int32_t v = (int32_t)edge - (int32_t)i * 256;      // how far past lamp i
    levels[i] = v <= 0 ? 0 : v >= 256 ? 255 : gammaLevel((uint8_t)(v - 1));
  }
}

// Outside-in pulse: outer lamps breathe, inner lamps follow half a beat later.
static void animOutsideIn(uint32_t t, uint8_t levels[]) {
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const uint8_t dist = i < NUM_CHANNELS - 1 - i ? i : NUM_CHANNELS - 1 - i;
    const uint32_t shifted = t + (uint32_t)dist * (ANIM_PULSE_PERIOD_MS / 2);
    levels[i] = gammaLevel(ease(tri(shifted, ANIM_PULSE_PERIOD_MS)));
  }
}

// Sparkle: every step each lamp picks a new random target and fades to it.
static void animSparkle(uint32_t t, uint8_t levels[]) {
  static uint32_t lastStep = 0xFFFFFFFF;
  static uint8_t from[8], to[8];
  const uint32_t step = t / ANIM_SPARKLE_STEP_MS;
  if (step != lastStep) {
    const bool restart = step < lastStep;                     // new run of the animation
    lastStep = step;
    for (uint8_t i = 0; i < NUM_CHANNELS && i < 8; i++) {
      from[i] = restart ? 0 : to[i];
      to[i] = random(0, 6) == 0 ? 255 : random(0, 90);        // mostly dim, some bright
    }
  }
  const uint32_t frac = (t % ANIM_SPARKLE_STEP_MS) * 255 / ANIM_SPARKLE_STEP_MS;
  for (uint8_t i = 0; i < NUM_CHANNELS && i < 8; i++) {
    const int32_t v = from[i] + ((int32_t)to[i] - from[i]) * (int32_t)ease(frac) / 255;
    levels[i] = gammaLevel((uint8_t)v);
  }
}

// Heartbeat: two quick flashes that decay, then rest.
static void animHeartbeat(uint32_t t, uint8_t levels[]) {
  const uint32_t p = t % 1200;
  uint8_t v = 0;
  if (p < 220) v = 255 - p * 255 / 220;
  if (p >= 180 && p < 420) {
    const uint8_t v2 = 255 - (p - 180) * 255 / 240;
    if (v2 > v) v = v2;
  }
  const uint8_t g = gammaLevel(v);
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = g;
}

// Breathe: everything slowly rises and falls together.
static void animBreathe(uint32_t t, uint8_t levels[]) {
  const uint8_t g = gammaLevel(ease(tri(t, ANIM_BREATHE_PERIOD_MS)));
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) levels[i] = g;
}

static const Animation ANIMATIONS[] = {
  { animKitt,      6000 },
  { animBreathe,   4800 },
  { animFill,      5000 },
  { animOutsideIn, 4400 },
  { animSparkle,   4000 },
  { animHeartbeat, 3600 },
};
static const uint8_t NUM_ANIMATIONS = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);
