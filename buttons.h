/**
 *  Polled, bounce-proof push button. Polarity and pull-up come from
 *  config.h (BUTTON_ACTIVE_LOW, BUTTON_USE_PULLUP).
 *
 *  No interrupts, no shared lockout between buttons, no library.
 *
 *  - A press is reported on the first PRESS_CONFIRM_SAMPLES consecutive
 *    "pressed" samples (1 ms apart), so latency is a couple of ms.
 *  - The button is considered held until it has read "released" for
 *    RELEASE_STABLE_MS in a row. Every "pressed" sample while held just
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
    pinMode(pin_, BUTTON_USE_PULLUP ? INPUT_PULLUP : INPUT);
    held_ = false;
    downSamples_ = 0;
    lastSampleMs_ = millis();
    lastDownMs_ = lastSampleMs_;
  }

  // Returns true once for each new press.
  bool poll(uint32_t now) {
    if (now == lastSampleMs_) return false;   // sample at most every 1 ms
    lastSampleMs_ = now;

    const bool down = rawDown();

    if (held_) {
      if (down) {
        lastDownMs_ = now;
      } else if ((uint32_t)(now - lastDownMs_) >= RELEASE_STABLE_MS) {
        held_ = false;
        downSamples_ = 0;
      }
      return false;
    }

    if (!down) {
      downSamples_ = 0;
      return false;
    }

    if (++downSamples_ < PRESS_CONFIRM_SAMPLES) return false;

    held_ = true;
    lastDownMs_ = now;
    return true;
  }

  bool isHeld() const { return held_; }

  // Raw, un-debounced read in the configured polarity.
  bool rawDown() const {
    return (digitalRead(pin_) == LOW) == BUTTON_ACTIVE_LOW;
  }

 private:
  uint8_t pin_ = 0;
  bool held_ = false;
  uint8_t downSamples_ = 0;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastDownMs_ = 0;
};
