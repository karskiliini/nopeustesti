/**
 *  (C) Tero Maaranen 2022
 *
 *  NOPEUSTESTI / SPEEDTEST
 *  classic speed testing game with four lights and a buzzer sound.
 *
 *  Programmed and tested with Arduino Leonardo on Wed 19.10.2022.
 *
 */

// install Grove 4-digit display by Seeed library
// https://github.com/Seeed-Studio/Grove_4Digital_Display/blob/master/TM1637.h
#include <TM1637.h>

int CLK = 9;
int DIO = 10;

TM1637 tm(CLK,DIO);
int8_t dispData[] = { 0, 0, 0, 0 };
int score = 0;

void displayScore()
{
  dispData[3] = score % 10;
  dispData[2] = (score / 10) % 10;
  dispData[1] = (score / 100) % 10;
  dispData[0] = (score / 1000) % 10;

  tm.display(dispData);
}

const float PRESS_INTERVAL_MS = 200;
const int TIME_AFTER_ERROR = 2500;

// values updated in isr
volatile float press_guard = 0;
volatile int pressed = 0;

float delay_ms;
const int BUF_LEN = 10;
int buffer[BUF_LEN];
int buffer_r;
int buffer_w;
int enabled = 0;
int quit = 0;
int quit_blink = 25;
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
  quit_blink = 25;
  pressed = 0;
  press_guard = millis() + PRESS_INTERVAL_MS;

  all_lights_off();
  
  target_time = millis() + delay_ms;
}

void setup() {

  tm.init();
  // set brightness; 0-7
  tm.set(2);
  tm.display(dispData);

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
      Serial.print("game state: register_press error queue empty, index: ");
      Serial.print(buffer_r);
      Serial.print("\n");
      return -1;
    }

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

    ++score;

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

    buffer[buffer_w] = color;
    ++buffer_w;

    if (buffer_w >= BUF_LEN) {
      buffer_w = 0; 
    }

    return 0;
}

void isr_green()
{
  unsigned long ms = millis();
  if (press_guard > ms)
    return;

  press_guard = ms + PRESS_INTERVAL_MS;
  pressed = GREEN_LIGHT_PIN;
}

void isr_yellow()
{
  unsigned long ms = millis();
  if (press_guard > ms)
    return;

  press_guard = ms + PRESS_INTERVAL_MS;
  pressed = YELLOW_LIGHT_PIN;
}

void isr_blue()
{
  unsigned long ms = millis();
  if (press_guard > ms)
    return;

  press_guard = ms + PRESS_INTERVAL_MS;
  pressed = BLUE_LIGHT_PIN;
}

void isr_red()
{
  unsigned long ms = millis();
  if (press_guard > ms)
    return;

  press_guard = ms + PRESS_INTERVAL_MS;
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
      } else if (quit_blink) {
        all_lights_on();
        enabled = 1;
        --quit_blink;
      }
    }
    if (pressed)
    {
      Serial.print("Start a new game!\n");
      score = 0;
      displayScore();
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
    err = register_press(pressed);
    if (err)
      quit = 1;

    displayScore();
    light_off(pressed);
    pressed = 0;
  }

  if (target_time < millis())
  {
    if (enabled)
    {
      delay_ms = 0.95f * delay_ms;

      all_lights_off();
      enabled = 0;
      target_time = millis() + delay_ms;
    } else {
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
    all_lights_on();
    return;
  }
}
