/*
  Fan Control and Status Panel (Homework 3)

  A potentiometer sets the requested fan speed, shown as the brightness
  of an external LED (PWM). An RGB LED shows the operating level and a
  passive buzzer beeps when entering the high level. The built-in LED
  blinks as a heartbeat. No delay() is used in loop().

  Circuit: built-in LED GPIO2, external LED GPIO18, potentiometer GPIO34,
  RGB LED GPIO25/26/27, passive buzzer GPIO23.

  Author: Xhamani Christian, October 2026
*/

#include <Arduino.h>

// Pins
const int heartbeatLedPin = 2;
const int fanLedPin = 18;
const int potPin = 34;
const int redPin = 25;
const int greenPin = 26;
const int bluePin = 27;
const int buzzerPin = 23;

// Heartbeat timing
const unsigned long heartbeatIntervalMs = 500;
unsigned long lastHeartbeatToggleMs = 0;
bool heartbeatLedState = LOW;

// Fan speed (potentiometer -> PWM)
const int adcMaxValue = 4095;   // 12-bit ADC
const int pwmMaxValue = 255;    // 8-bit PWM
int potValue = 0;
int fanPwmValue = 0;

// Operating levels (ADC thirds, see README calculations)
enum FanLevel { LOW_SPEED, MEDIUM_SPEED, HIGH_SPEED };
const int lowLevelMaxValue = 1365;
const int mediumLevelMaxValue = 2730;
FanLevel fanLevel = LOW_SPEED;
FanLevel previousFanLevel = LOW_SPEED;   // used to detect entering HIGH_SPEED

// Red is much brighter than green at 3.3 V, so yellow needs less red (tune by eye)
const int yellowRedValue = 60;

/*
  Sound notification. tone() conflicted with analogWrite() (both use the
  LEDC PWM peripheral), so the square wave is generated in software:
  the buzzer pin is toggled every half period, using micros().
*/
const unsigned long alertToneFrequencyHz = 1000;
const unsigned long alertToneDurationMs = 500;
const unsigned long buzzerHalfPeriodUs = 1000000UL / (2 * alertToneFrequencyHz);  // 500 us
bool buzzerActive = false;
unsigned long buzzerStartMs = 0;
unsigned long lastBuzzerToggleUs = 0;
bool buzzerPinState = LOW;

// Serial debugging
const unsigned long serialPrintIntervalMs = 500;
unsigned long lastSerialPrintMs = 0;

// Function prototypes (needed in .cpp files)
void updateHeartbeat(unsigned long currentMs);
void updateFanSpeed();
void updateStatusColor();
void setColor(int redValue, int greenValue, int blueValue);
void updateBuzzer(unsigned long currentMs);
void printDebugInfo(unsigned long currentMs);
const char* getLevelName(FanLevel level);

void setup() {
  Serial.begin(115200);
  pinMode(heartbeatLedPin, OUTPUT);
  pinMode(fanLedPin, OUTPUT);
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  unsigned long currentMs = millis();
  updateHeartbeat(currentMs);
  updateFanSpeed();
  updateStatusColor();
  updateBuzzer(currentMs);
  printDebugInfo(currentMs);
}

// Toggle the built-in LED every heartbeatIntervalMs, without blocking
void updateHeartbeat(unsigned long currentMs) {
  if (currentMs - lastHeartbeatToggleMs >= heartbeatIntervalMs) {
    lastHeartbeatToggleMs = currentMs;
    heartbeatLedState = !heartbeatLedState;
    digitalWrite(heartbeatLedPin, heartbeatLedState);
  }
}

// Read the requested speed and show it as LED brightness
void updateFanSpeed() {
  potValue = analogRead(potPin);
  fanPwmValue = map(potValue, 0, adcMaxValue, 0, pwmMaxValue);
  analogWrite(fanLedPin, fanPwmValue);
}

// Pick the operating level from the potentiometer and show its color
void updateStatusColor() {
  if (potValue <= lowLevelMaxValue) {
    fanLevel = LOW_SPEED;
    setColor(0, 0, pwmMaxValue);                  // blue
  } else if (potValue <= mediumLevelMaxValue) {
    fanLevel = MEDIUM_SPEED;
    setColor(yellowRedValue, pwmMaxValue, 0);     // yellow
  } else {
    fanLevel = HIGH_SPEED;
    setColor(pwmMaxValue, 0, 0);                  // red
  }
}

// Write one PWM value per RGB channel (common cathode: higher = brighter)
void setColor(int redValue, int greenValue, int blueValue) {
  analogWrite(redPin, redValue);
  analogWrite(greenPin, greenValue);
  analogWrite(bluePin, blueValue);
}

/*
  Start a beep only when the level changes into HIGH_SPEED (edge detection),
  then keep toggling the buzzer pin until the beep duration has passed.
*/
void updateBuzzer(unsigned long currentMs) {
  bool enteredHighSpeed = (fanLevel == HIGH_SPEED && previousFanLevel != HIGH_SPEED);
  previousFanLevel = fanLevel;

  if (enteredHighSpeed) {
    buzzerActive = true;
    buzzerStartMs = currentMs;
  }

  if (!buzzerActive) {
    return;
  }

  // Stop the beep after alertToneDurationMs
  if (currentMs - buzzerStartMs >= alertToneDurationMs) {
    buzzerActive = false;
    buzzerPinState = LOW;
    digitalWrite(buzzerPin, LOW);
    return;
  }

  // Generate the square wave: flip the pin every half period
  unsigned long currentUs = micros();
  if (currentUs - lastBuzzerToggleUs >= buzzerHalfPeriodUs) {
    lastBuzzerToggleUs = currentUs;
    buzzerPinState = !buzzerPinState;
    digitalWrite(buzzerPin, buzzerPinState);
  }
}

// Print the raw ADC value, PWM output and level every serialPrintIntervalMs
void printDebugInfo(unsigned long currentMs) {
  if (currentMs - lastSerialPrintMs >= serialPrintIntervalMs) {
    lastSerialPrintMs = currentMs;
    Serial.print("ADC raw: ");
    Serial.print(potValue);
    Serial.print(" | PWM: ");
    Serial.print(fanPwmValue);
    Serial.print(" | Level: ");
    Serial.println(getLevelName(fanLevel));
  }
}

// Human-readable level name for the Serial Monitor
const char* getLevelName(FanLevel level) {
  switch (level) {
    case LOW_SPEED:    return "LOW (blue)";
    case MEDIUM_SPEED: return "MEDIUM (yellow)";
    case HIGH_SPEED:   return "HIGH (red)";
  }
  return "UNKNOWN";
}