/**
 *  Minimal TM1637 4-digit 7-segment driver (Grove 4-Digit Display and the
 *  common bare modules). No library needed.
 *
 *  Bit-banged two-wire protocol. Lines are driven open-drain style: LOW by
 *  pulling the pin to GND, HIGH by releasing it to the pull-up.
 *
 *  Segment bits:  a=0x01 b=0x02 c=0x04 d=0x08 e=0x10 f=0x20 g=0x40 dp=0x80
 *
 *       a
 *      ---
 *   f |   | b
 *      -g-
 *   e |   | c
 *      ---  . dp
 *       d
 */
#pragma once

#include <Arduino.h>

class TM1637Display {
 public:
  static const uint8_t SEG_BLANK = 0x00;
  static const uint8_t SEG_DASH  = 0x40;

  TM1637Display(uint8_t clkPin, uint8_t dioPin) : clk_(clkPin), dio_(dioPin) {}

  void begin(uint8_t brightness) {
    release(clk_);
    release(dio_);
    setBrightness(brightness, true);
    clear();
  }

  // brightness 0..7, on=false blanks the display without losing data.
  void setBrightness(uint8_t brightness, bool on = true) {
    ctrl_ = 0x80 | (on ? 0x08 : 0x00) | (brightness & 0x07);
    start();
    writeByte(ctrl_);
    stop();
  }

  void on()  { setBrightness(ctrl_ & 0x07, true); }
  void off() { setBrightness(ctrl_ & 0x07, false); }

  void clear() {
    const uint8_t blank[4] = { 0, 0, 0, 0 };
    showRaw(blank);
  }

  // Writes four raw segment bytes, leftmost digit first.
  void showRaw(const uint8_t segs[4]) {
    start();
    writeByte(0x40);          // data command: auto-increment address
    stop();
    start();
    writeByte(0xC0);          // address command: first digit
    for (uint8_t i = 0; i < 4; i++) writeByte(segs[i]);
    stop();
    start();
    writeByte(ctrl_);
    stop();
  }

  // Right-aligned decimal number 0..9999, leading zeros blanked.
  void showNumber(uint16_t value) {
    uint8_t segs[4];
    if (value > 9999) value = 9999;
    for (int8_t i = 3; i >= 0; i--) {
      segs[i] = digit(value % 10);
      value /= 10;
      if (value == 0 && i > 0) {
        while (--i >= 0) segs[i] = SEG_BLANK;
        break;
      }
    }
    showRaw(segs);
  }

  // Up to four characters from the small font below; unknown -> blank.
  void showText(const char *text) {
    uint8_t segs[4] = { 0, 0, 0, 0 };
    for (uint8_t i = 0; i < 4 && text[i]; i++) segs[i] = glyph(text[i]);
    showRaw(segs);
  }

  static uint8_t digit(uint8_t d) {
    static const uint8_t table[10] = {
      0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    return d < 10 ? table[d] : SEG_BLANK;
  }

  static uint8_t glyph(char c) {
    if (c >= '0' && c <= '9') return digit(c - '0');
    switch (c) {
      case 'A': case 'a': return 0x77;
      case 'b': case 'B': return 0x7C;
      case 'C':           return 0x39;
      case 'c':           return 0x58;
      case 'd': case 'D': return 0x5E;
      case 'E': case 'e': return 0x79;
      case 'F': case 'f': return 0x71;
      case 'G': case 'g': return 0x3D;
      case 'H':           return 0x76;
      case 'h':           return 0x74;
      case 'I': case 'i': return 0x30;
      case 'J': case 'j': return 0x1E;
      case 'L': case 'l': return 0x38;
      case 'n': case 'N': return 0x54;
      case 'o': case 'O': return 0x5C;
      case 'P': case 'p': return 0x73;
      case 'r': case 'R': return 0x50;
      case 'S': case 's': return 0x6D;
      case 't': case 'T': return 0x78;
      case 'U': case 'u': return 0x3E;
      case 'y': case 'Y': return 0x6E;
      case '-':           return SEG_DASH;
      case '_':           return 0x08;
      default:            return SEG_BLANK;
    }
  }

 private:
  // Open-drain emulation: LOW drives, HIGH releases to the pull-up.
  void drive(uint8_t pin)   { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  void release(uint8_t pin) { pinMode(pin, INPUT_PULLUP); }
  void tick() { delayMicroseconds(5); }

  void start() {          // DIO falls while CLK is high
    release(clk_); release(dio_); tick();
    drive(dio_); tick();
  }

  void stop() {           // DIO rises while CLK is high
    drive(clk_); tick();
    drive(dio_); tick();
    release(clk_); tick();
    release(dio_); tick();
  }

  void writeByte(uint8_t b) {   // LSB first, then one ACK clock
    for (uint8_t i = 0; i < 8; i++) {
      drive(clk_); tick();
      if (b & 0x01) release(dio_); else drive(dio_);
      tick();
      release(clk_); tick();
      b >>= 1;
    }
    drive(clk_);
    release(dio_);          // let the chip pull DIO low for ACK
    tick();
    release(clk_); tick();  // ACK is sampled here; we don't need its value
    drive(clk_); tick();
  }

  uint8_t clk_;
  uint8_t dio_;
  uint8_t ctrl_ = 0x88;
};
