/**
 *  Polled, bounce-proof push button (active LOW, internal pull-up).
 *
 *  No interrupts, no shared lockout between buttons, no library.
 *
 *  - A press is reported on the first PRESS_CONFIRM_SAMPLES consecutive
 *    LOW samples (1 ms apart), so latency is a couple of milliseconds.
 *  - The button is considered held until it has read HIGH for
 *    RELEASE_STABLE_MS in a row. Every LOW sample while "held" just
 *    refreshes that timer, so contact bounce on press or release can
 *    never generate a second press.
 *
 *  Call poll() from loop() as often as possible; it samples at most once
 *  per millisecond and returns true exactly once per physical press.
 */
#pragma once

#include <Arduino.h>
#include "config.h"

class Button {
 public:
  void begin(uint8_t pin) {
    pin_ = pin;
    pinMode(pin_, INPUT_PULLUP);
    held_ = false;
    lowSamples_ = 0;
    lastSampleMs_ = millis();
    lastLowMs_ = lastSampleMs_;
  }

  // Returns true once for each new press.
  bool poll(uint32_t now) {
    if (now == lastSampleMs_) return false;   // sample at most every 1 ms
    lastSampleMs_ = now;

    const bool low = digitalRead(pin_) == LOW;

    if (held_) {
      if (low) {
        lastLowMs_ = now;
      } else if ((uint32_t)(now - lastLowMs_) >= RELEASE_STABLE_MS) {
        held_ = false;
        lowSamples_ = 0;
      }
      return false;
    }

    if (!low) {
      lowSamples_ = 0;
      return false;
    }

    if (++lowSamples_ < PRESS_CONFIRM_SAMPLES) return false;

    held_ = true;
    lastLowMs_ = now;
    return true;
  }

  bool isHeld() const { return held_; }

  // Raw, un-debounced read. Only for the boot-time "is anything held" check.
  bool rawDown() const { return digitalRead(pin_) == LOW; }

 private:
  uint8_t pin_ = 0;
  bool held_ = false;
  uint8_t lowSamples_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastLowMs_ = 0;
};
