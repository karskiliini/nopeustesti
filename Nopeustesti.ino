/**
 *  (C) Tero Maaranen 2022
 *
 *  NOPEUSTESTI / SPEEDTEST
 *  classic speed testing game with four lights and a buzzer sound.
 *
 *  Programmed and tested with Arduino Leonardo on Wed 19.10.2022.
 *
 */

const float PRESS_INTERVAL_MS = 200;
const int TIME_AFTER_ERROR = 2500;

float delay_ms;
const int BUF_LEN = 10;
int buffer[BUF_LEN];
int buffer_r;
int buffer_w;
int enabled = 0;
int quit = 0;
float press_guard = 0;

volatile int pressed = 0;
float target_time = 0;

// lights
const int GREEN_LIGHT_PIN = 6;
const int YELLOW_LIGHT_PIN = 13;
const int RED_LIGHT_PIN = 12;
const int BLUE_LIGHT_PIN = 7;

// button isr pins
const int GREEN_BUTTON_PIN = 0;
const int YELLOW_BUTTON_PIN = 1;
const int BLUE_BUTTON_PIN = 3;
const int RED_BUTTON_PIN = 2;

void isr_green();
void isr_yellow();
void isr_red();
void isr_blue();

int index_to_color[4];

void light_on(int color) {
    digitalWrite(color, HIGH);
}

void light_off(int color) {
    digitalWrite(color, LOW);
}

void all_lights_on()
{
  light_on(RED_LIGHT_PIN);
  light_on(GREEN_LIGHT_PIN);
  light_on(YELLOW_LIGHT_PIN);
  light_on(BLUE_LIGHT_PIN);
}

void all_lights_off()
{
  light_off(RED_LIGHT_PIN);
  light_off(GREEN_LIGHT_PIN);
  light_off(YELLOW_LIGHT_PIN);
  light_off(BLUE_LIGHT_PIN);
}

void reset()
{
  delay_ms = 2000.0f;
  buffer_r = 0;
  buffer_w = 0;
  enabled = 0;
  quit = 0;
  pressed = 0;
  press_guard = millis() + PRESS_INTERVAL_MS;

  all_lights_off();
  
  target_time = millis() + delay_ms;
}

void setup() {
  
  reset();
  
  index_to_color[0] = GREEN_LIGHT_PIN;
  index_to_color[1] = YELLOW_LIGHT_PIN;
  index_to_color[2] = BLUE_LIGHT_PIN;
  index_to_color[3] = RED_LIGHT_PIN;

  pinMode(GREEN_LIGHT_PIN, OUTPUT);
  pinMode(GREEN_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(GREEN_BUTTON_PIN), isr_green, FALLING);

  pinMode(YELLOW_LIGHT_PIN, OUTPUT);
  pinMode(YELLOW_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(YELLOW_BUTTON_PIN), isr_yellow, FALLING);

  pinMode(BLUE_LIGHT_PIN, OUTPUT);
  pinMode(BLUE_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BLUE_BUTTON_PIN), isr_red, FALLING);

  pinMode(RED_LIGHT_PIN, OUTPUT);
  pinMode(RED_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RED_BUTTON_PIN), isr_blue, FALLING);

  memset(buffer, 0, sizeof(int) * BUF_LEN);

  all_lights_off();
}

int register_press(int color)
{
    press_guard = millis() + PRESS_INTERVAL_MS;

    // nothing in queue, error
    if (buffer_w == buffer_r)
    {
      Serial.print("game state: register_press error queue empty -1.\n");
      Serial.print(buffer_r);
      Serial.print("\n");
      return -1;
    }

    Serial.print("game state: register_press done: index ");
    Serial.print(buffer_r);
    Serial.print("\n");

    int c = buffer[buffer_r];
    ++buffer_r;

    if (buffer_r >= BUF_LEN) {
      buffer_r = 0;
    }

    // wrong color pressed
    if (c != color)
    {
      Serial.print("game state: register_press error, wrong color.\n");

      return -1;
    }

    return 0;
}

int queue_color(int color)
{
    // queue full, player too slow: error
    if ((buffer_w == (buffer_r - 1)) ||
       ((buffer_w == (BUF_LEN - 1)) && (buffer_r == 0)))
    {
      Serial.print("queue_color err: buffer full, too slow. \n");
      return -1;
    }

    Serial.print("game state: queue_color done: index ");
    Serial.print(buffer_w);
    Serial.print("\n");

    buffer[buffer_w] = color;
    ++buffer_w;

    switch(color)
    {
      case RED_LIGHT_PIN:
    Serial.print("queue_color added color RED\n");
        break;
      case BLUE_LIGHT_PIN:
    Serial.print("queue_color added color BLUE\n");
        break;
      case GREEN_LIGHT_PIN:
    Serial.print("queue_color added color GREEN\n");
        break;
      case YELLOW_LIGHT_PIN:
    Serial.print("queue_color added color YELLOW\n");
        break;
    }

    if (buffer_w >= BUF_LEN) {
      buffer_w = 0; 
    }

    return 0;
}

void isr_green()
{
  if (press_guard > millis())
    return;

  press_guard = millis() + PRESS_INTERVAL_MS;

  Serial.print("isr green\n");
  pressed = GREEN_LIGHT_PIN;
}

void isr_yellow()
{
  if (press_guard > millis())
    return;

  press_guard = millis() + PRESS_INTERVAL_MS;

  Serial.print("isr yellow\n");
  pressed = YELLOW_LIGHT_PIN;
}

void isr_blue()
{
  if (press_guard > millis())
    return;

  press_guard = millis() + PRESS_INTERVAL_MS;

  Serial.print("isr blue\n");
  pressed = BLUE_LIGHT_PIN;
}

void isr_red()
{
  if (press_guard > millis())
    return;

  press_guard = millis() + PRESS_INTERVAL_MS;

  Serial.print("isr red\n");
  pressed = RED_LIGHT_PIN;
}

void handleQuitState()
{
    if (target_time < millis())
    {
      target_time = millis() + 400;
      if (enabled)
      {
        all_lights_off();
        enabled = 0;
      } else {
        all_lights_on();
        enabled = 1;
      }
    }
    if (pressed)
    {
      Serial.print("quit state: press registered, start game.\n");
      reset();
    }
}

void loop() {
  if (quit) {
    handleQuitState();
    return;
  }

  int err = 0;

  float delay_now = ((float)(rand() % 100) / 100.0f) * delay_ms + delay_ms;

  if (pressed)
  {
        Serial.print("game state: press detected.\n");
    err = register_press(pressed);
    if (err)
      quit = 1;
    
    light_off(pressed);
    pressed = 0;
  }

  if (target_time < millis())
  {
    if (enabled)
    {
      // Serial.print("game state: turn off lights.\n");

      delay_ms = 0.95f * delay_ms;

      light_off(GREEN_LIGHT_PIN);
      light_off(YELLOW_LIGHT_PIN);
      light_off(BLUE_LIGHT_PIN);
      light_off(RED_LIGHT_PIN);
      enabled = 0;
      target_time = millis() + delay_ms;
    } else {
      // Serial.print("game state: turn on light.\n");

      int color = rand() % 4;
      color = index_to_color[color];

      err = queue_color(color);
      if (err)
        quit = 1;

      light_on(color);
      enabled = 1;

      target_time = millis() + delay_ms;
    }
  }

  if (quit) {
    Serial.print("game state: error -> quit state.\n");
  
    // ignore all keypresses until a buzzer has sounded
    press_guard = millis() + TIME_AFTER_ERROR;

    target_time = millis() + TIME_AFTER_ERROR;
    light_on(GREEN_LIGHT_PIN);
    light_on(YELLOW_LIGHT_PIN);
    light_on(BLUE_LIGHT_PIN);
    light_on(RED_LIGHT_PIN);
    return;
  }

  float now = millis();
  float shutoff = now + delay_ms;
}