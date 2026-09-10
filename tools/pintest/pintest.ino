// Wiring mapper. Serial 115200. Send "6" to blink pin 6, "s" to stop,
// "a" to sweep all candidate pins once. Any candidate pin pulled LOW
// (a button press) is reported.
const uint8_t PINS[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,A0,A1,A2,A3,A4,A5};
const uint8_t N = sizeof(PINS);
int8_t blinkPin = -1;
uint32_t blinkMask = 0;   // bit n = pin n blinks (multi-pin mode)
bool wasLow[N];
uint32_t lastReport[N];

void allInputs() {
  for (uint8_t i = 0; i < N; i++) { pinMode(PINS[i], INPUT_PULLUP); wasLow[i] = false; }
}
void setup() {
  Serial.begin(115200);
  allInputs();
}
void setBlink(int pin) {
  allInputs();
  blinkMask = 0;
  blinkPin = pin;
  if (pin >= 0) { pinMode(pin, OUTPUT); Serial.print(F("blinking pin ")); Serial.println(pin); }
  else Serial.println(F("stopped"));
}
void loop() {
  static bool greeted = false;
  if (Serial && !greeted) { greeted = true; Serial.println(F("pintest ready: send pin number, s=stop, a=sweep")); }
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); line.trim();
    if (line == "s") setBlink(-1);
    else if (line == "q") {
      // Classify each pin: drive LOW then float; if it reads HIGH something
      // external pulls it up. Drive HIGH then float; if LOW something pulls
      // it down. Otherwise it floats.
      Serial.print(F("probe:"));
      for (uint8_t i = 0; i < N; i++) {
        uint8_t pin = PINS[i];
        pinMode(pin, OUTPUT); digitalWrite(pin, LOW); delayMicroseconds(100);
        pinMode(pin, INPUT); delayMicroseconds(30);
        bool up = digitalRead(pin) == HIGH;
        pinMode(pin, OUTPUT); digitalWrite(pin, HIGH); delayMicroseconds(100);
        pinMode(pin, INPUT); delayMicroseconds(30);
        bool down = digitalRead(pin) == LOW;
        pinMode(pin, INPUT_PULLUP);
        Serial.print(' '); Serial.print(pin); Serial.print('=');
        Serial.print(up && down ? "?" : up ? "UP" : down ? "DOWN" : "-");
      }
      Serial.println();
    }
    else if (line == "p") {
      Serial.print(F("levels:"));
      for (uint8_t i = 0; i < N; i++) {
        Serial.print(' '); Serial.print(PINS[i]); Serial.print('='); Serial.print(digitalRead(PINS[i]) ? 'H' : 'L');
      }
      Serial.println();
    }
    else if (line == "a") {
      allInputs();
      for (uint8_t i = 0; i < N; i++) {
        pinMode(PINS[i], OUTPUT);
        Serial.print(F("sweep pin ")); Serial.println(PINS[i]);
        for (uint8_t k = 0; k < 3; k++) { digitalWrite(PINS[i], HIGH); delay(150); digitalWrite(PINS[i], LOW); delay(150); }
        pinMode(PINS[i], INPUT_PULLUP);
      }
      Serial.println(F("sweep done"));
    }
    else if (line.indexOf(',') >= 0) {      // "5,6,7,13": blink several pins
      allInputs(); blinkPin = -1; blinkMask = 0;
      int start = 0;
      while (start < (int)line.length()) {
        int comma = line.indexOf(',', start); if (comma < 0) comma = line.length();
        int pin = line.substring(start, comma).toInt();
        if (pin >= 0 && pin < 32) { blinkMask |= (1UL << pin); pinMode(pin, OUTPUT); }
        start = comma + 1;
      }
      Serial.print(F("blinking pins ")); Serial.println(line);
    }
    else if (line.length()) setBlink(line.toInt());
  }
  if (blinkPin >= 0) digitalWrite(blinkPin, (millis() / 250) % 2);
  for (uint8_t p = 0; p < 32; p++) if (blinkMask & (1UL << p)) digitalWrite(p, (millis() / 250) % 2);
  for (uint8_t i = 0; i < N; i++) {
    if (PINS[i] == blinkPin || (blinkMask & (1UL << PINS[i]))) continue;
    bool low = digitalRead(PINS[i]) == LOW;
    if (low != wasLow[i]) {
      if (millis() - lastReport[i] > 30) {
        Serial.print(F("pin ")); Serial.print(PINS[i]);
        Serial.println(low ? F(" -> LOW (pressed to GND?)") : F(" -> HIGH"));
      }
      lastReport[i] = millis();
      wasLow[i] = low;
    }
  }
}
