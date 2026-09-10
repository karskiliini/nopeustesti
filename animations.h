/**
 *  Idle (attract mode) lamp animations.
 *
 *  Each animation is a pure function of elapsed time: it returns a bitmask
 *  of lamps that should be lit (bit i = CHANNELS[i]). The sketch plays them
 *  in order, with a dark pause between, and loops. Works for any number of
 *  lamps; positions follow the CHANNELS row order (left to right).
 *
 *  Add an animation: write a frame function, add a row to ANIMATIONS.
 */
#pragma once

#include <Arduino.h>
#include "config.h"

typedef uint8_t (*AnimationFrame)(uint32_t t);

struct Animation {
  AnimationFrame frame;
  uint16_t durationMs;
};

static const uint8_t ALL_LAMPS = (uint8_t)((1u << NUM_CHANNELS) - 1);

// KITT scanner: one lamp sweeps left to right and back.
static uint8_t animKitt(uint32_t t) {
  const uint8_t period = NUM_CHANNELS * 2 - 2;
  const uint8_t step = (t / ANIM_KITT_STEP_MS) % period;
  const uint8_t pos = step < NUM_CHANNELS ? step : period - step;
  return 1u << pos;
}

// Fill bar: lamps light one by one from the left, then drain from the left.
static uint8_t animFill(uint32_t t) {
  const uint8_t step = (t / ANIM_FILL_STEP_MS) % (NUM_CHANNELS * 2);
  uint8_t mask = 0;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const bool on = step < NUM_CHANNELS ? (i <= step) : (i > step - NUM_CHANNELS);
    if (on) mask |= 1u << i;
  }
  return mask;
}

// Outside-in pulse: outer lamps, then the next pair inward, alternating.
static uint8_t animOutsideIn(uint32_t t) {
  const uint8_t rings = (NUM_CHANNELS + 1) / 2;
  const uint8_t ring = (t / ANIM_PULSE_STEP_MS) % rings;
  uint8_t mask = 0;
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    const uint8_t dist = i < NUM_CHANNELS - 1 - i ? i : NUM_CHANNELS - 1 - i;
    if (dist == ring) mask |= 1u << i;
  }
  return mask;
}

// Sparkle: a fresh random pattern every step, never fully dark.
static uint8_t animSparkle(uint32_t t) {
  static uint32_t lastStep = 0xFFFFFFFF;
  static uint8_t mask = 1;
  const uint32_t step = t / ANIM_SPARKLE_STEP_MS;
  if (step != lastStep) {
    lastStep = step;
    mask = random(1, ALL_LAMPS + 1);
  }
  return mask;
}

// Heartbeat: two quick flashes of everything, then rest.
static uint8_t animHeartbeat(uint32_t t) {
  const uint16_t p = t % 1200;
  return (p < 90 || (p >= 220 && p < 310)) ? ALL_LAMPS : 0;
}

static const Animation ANIMATIONS[] = {
  { animKitt,      5000 },
  { animFill,      4000 },
  { animOutsideIn, 3000 },
  { animSparkle,   3500 },
  { animHeartbeat, 3600 },
};
static const uint8_t NUM_ANIMATIONS = sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);
