/*
  Homework 4 - Smart Lamp Controller

  A lamp (external LED) controlled by one pushbutton, with 4 modes:
  OFF -> DIM -> FULL -> BLINK -> OFF ...
  Short press: next mode. Long press (>= 1000 ms): lamp OFF immediately.
  The built-in LED blinks independently as a system heartbeat.
  Button handling is interrupt-assisted: a FALLING interrupt starts
  a debounced validation, which runs until a stable release.

  Bonus B: BREATHING mode after BLINK (set extraModesEnabled = true).
  Bonus C: raw interrupt notifications vs accepted (debounced) presses
  are printed to the Serial Monitor.

  Circuit:
    - Pushbutton: GPIO21 <-> GND (internal pull-up, active LOW)
    - Lamp (external LED): GPIO18 -> LED -> 220 ohm -> GND
    - Heartbeat: built-in LED on GPIO2

  Author: Xhamani Christian
  Date: 04.11.2026
*/

#include <Arduino.h>

// Pins
const int buttonPin = 21;
const int lampPin = 18;
const int builtInLedPin = 2;

// Serial
const unsigned long serialBaudRate = 115200;

// Timing
const unsigned long debounceDelayMs = 30;
const unsigned long blinkIntervalMs = 500;
const unsigned long heartbeatIntervalMs = 500;
const unsigned long longPressThresholdMs = 1000;
const unsigned long breathingStepMs = 6;  // 255 steps * 6 ms = ~1.5 s up + ~1.5 s down

// PWM values for lamp (0-255)
const int offPwmValue = 0;
const int dimPwmValue = 64;
const int fullPwmValue = 255;

// Lamp operating modes
enum LampMode
{
  MODE_OFF,
  MODE_DIM,
  MODE_FULL,
  MODE_BLINK,
  MODE_BREATHING  // bonus, only reachable when extraModesEnabled is true
};

const int baseModeCount = 4;      
const int extendedModeCount = 5;  

// Bonus switch: false = only the mandatory 4 modes, true = extra modes too
const bool extraModesEnabled = true;

// Button state (debouncing)
int lastReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned int buttonPushCounter = 0;

// Interrupt-assisted button handling
volatile bool buttonEvent = false;             // set by the ISR (volatile: changes outside the normal program flow)
volatile unsigned long rawInterruptCount = 0;  // every ISR call, including bounce
bool buttonMonitoring = false;                 // true while the button is being validated after an interrupt

// Long press state
unsigned long pressStartTime = 0;
bool longPressHandled = false;

// Lamp state
LampMode currentMode = MODE_OFF;
bool blinkLampOn = false;
unsigned long lastBlinkToggleTime = 0;

// Breathing state (bonus)
int breathingPwm = 0;         // current brightness (0-255)
int breathingDirection = 1;   // +1 = getting brighter, -1 = getting dimmer
unsigned long lastBreathingStepTime = 0;

// Heartbeat state
bool heartbeatOn = false;
unsigned long lastHeartbeatToggleTime = 0;

// Function prototypes
void IRAM_ATTR handleButtonInterrupt();
void updateButton(unsigned long currentTime);
void onConfirmedPress(unsigned long currentTime);
void onConfirmedRelease(unsigned long currentTime);
void checkLongPress(unsigned long currentTime);
void updateLamp(unsigned long currentTime);
void updateHeartbeat(unsigned long currentTime);
LampMode getNextMode(LampMode mode);
const char *getModeName(LampMode mode);

void setup()
{
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(lampPin, OUTPUT);
  pinMode(builtInLedPin, OUTPUT);

  analogWrite(lampPin, offPwmValue);
  digitalWrite(builtInLedPin, LOW);

  Serial.begin(serialBaudRate);
  Serial.println("Smart Lamp started");
  Serial.print("Mode: ");
  Serial.println(getModeName(currentMode));

  // Initialize the button states from the actual input
  lastReading = digitalRead(buttonPin);
  buttonState = lastReading;

  // Start all timers from now
  lastDebounceTime = millis();
  lastHeartbeatToggleTime = millis();

  // FALLING = HIGH -> LOW = possible button press
  attachInterrupt(digitalPinToInterrupt(buttonPin), handleButtonInterrupt, FALLING);
}

void loop()
{
  unsigned long currentTime = millis();
  updateButton(currentTime);
  updateLamp(currentTime);
  updateHeartbeat(currentTime);
}

// ISR: record the event and count it
void IRAM_ATTR handleButtonInterrupt()
{
  buttonEvent = true;
  rawInterruptCount++;
}

// Wait for an interrupt, then debounce the button until a stable release
void updateButton(unsigned long currentTime)
{
  // 1. IDLE: wait for the interrupt notification, do not poll the button
  if (!buttonMonitoring)
  {
    if (buttonEvent)
    {
      buttonEvent = false;
      buttonMonitoring = true;
      lastReading = digitalRead(buttonPin);
      lastDebounceTime = currentTime;
    }
    else
    {
      return;
    }
  }

  // 2. MONITORING: software debounce (polling)
  int reading = digitalRead(buttonPin);

  // Reset the debounce timer whenever the raw input changes
  if (reading != lastReading)
  {
    lastDebounceTime = currentTime;
  }

  // Accept a new button state only after it remains stable
  if (currentTime - lastDebounceTime >= debounceDelayMs)
  {
    if (reading != buttonState)
    {
      buttonState = reading;

      if (buttonState == HIGH)
      {
        onConfirmedRelease(currentTime);
      }
      else
      {
        onConfirmedPress(currentTime);
      }
    }
  }

  lastReading = reading;

  // Runs on every loop while monitoring, not only on state changes
  checkLongPress(currentTime);

  // 3. Back to IDLE after a stable release (also ends false alarms)
  if (buttonState == HIGH && reading == HIGH &&
      currentTime - lastDebounceTime >= debounceDelayMs)
  {
    buttonMonitoring = false;
    buttonEvent = false;  // discard interrupts caused by bounce during this interaction
  }
}

void onConfirmedPress(unsigned long currentTime)
{
  pressStartTime = currentTime;
  longPressHandled = false;
}

void onConfirmedRelease(unsigned long currentTime)
{
  buttonPushCounter++;

  // A release after a long press must not change the mode again
  if (longPressHandled == false)
  {
    currentMode = getNextMode(currentMode);
    Serial.print("Mode: ");
    Serial.println(getModeName(currentMode));

    // Start a fresh blink cycle when entering BLINK
    if (currentMode == MODE_BLINK)
    {
      blinkLampOn = true;
      lastBlinkToggleTime = currentTime;
    }

    // Start breathing from dark when entering BREATHING
    if (currentMode == MODE_BREATHING)
    {
      breathingPwm = offPwmValue;
      breathingDirection = 1;
      lastBreathingStepTime = currentTime;
    }
  }

  // Compare raw interrupt notifications with accepted (debounced) presses
  Serial.print("Raw interrupts: ");
  Serial.print(rawInterruptCount);
  Serial.print(" | Accepted presses: ");
  Serial.println(buttonPushCounter);
}

// While the button is held, switch the lamp OFF once after longPressThresholdMs
void checkLongPress(unsigned long currentTime)
{
  if (buttonState == LOW && !longPressHandled)
  {
    if (currentTime - pressStartTime >= longPressThresholdMs)
    {
      currentMode = MODE_OFF;
      Serial.println("Long press -> OFF");
      longPressHandled = true;
    }
  }
}

// Drive the lamp according to the current mode
void updateLamp(unsigned long currentTime)
{
  switch (currentMode)
  {
  case MODE_OFF:
    analogWrite(lampPin, offPwmValue);
    break;
  case MODE_DIM:
    analogWrite(lampPin, dimPwmValue);
    break;
  case MODE_FULL:
    analogWrite(lampPin, fullPwmValue);
    break;
  case MODE_BLINK:
    // Toggle the lamp every blinkIntervalMs
    if (currentTime - lastBlinkToggleTime >= blinkIntervalMs)
    {
      lastBlinkToggleTime = currentTime;
      blinkLampOn = !blinkLampOn;
    }

    // Write the current blink state to the lamp
    if (blinkLampOn)
    {
      analogWrite(lampPin, fullPwmValue);
    }
    else
    {
      analogWrite(lampPin, offPwmValue);
    }
    break;
  case MODE_BREATHING:
    // Move the brightness one step every breathingStepMs
    if (currentTime - lastBreathingStepTime >= breathingStepMs)
    {
      lastBreathingStepTime = currentTime;
      breathingPwm += breathingDirection;

      // Reverse direction at the limits
      if (breathingPwm >= fullPwmValue)
      {
        breathingPwm = fullPwmValue;
        breathingDirection = -1;
      }
      else if (breathingPwm <= offPwmValue)
      {
        breathingPwm = offPwmValue;
        breathingDirection = 1;
      }
    }
    analogWrite(lampPin, breathingPwm);
    break;
  default:
    analogWrite(lampPin, offPwmValue);
    break;
  }
}

// Toggle the built-in LED every heartbeatIntervalMs, in every mode
void updateHeartbeat(unsigned long currentTime)
{
  if (currentTime - lastHeartbeatToggleTime >= heartbeatIntervalMs)
  {
    lastHeartbeatToggleTime = currentTime;
    heartbeatOn = !heartbeatOn;
    digitalWrite(builtInLedPin, heartbeatOn);
  }
}

// Return the next mode: OFF -> DIM -> FULL -> BLINK (-> BREATHING if extra modes are enabled) -> OFF
LampMode getNextMode(LampMode mode)
{
  int activeModeCount = extraModesEnabled ? extendedModeCount : baseModeCount;
  return (LampMode)((mode + 1) % activeModeCount);
}

// Return a readable name for a mode (Serial output)
const char *getModeName(LampMode mode)
{
  switch (mode)
  {
  case MODE_OFF:
    return "OFF";
  case MODE_DIM:
    return "DIM";
  case MODE_FULL:
    return "FULL";
  case MODE_BLINK:
    return "BLINK";
  case MODE_BREATHING:
    return "BREATHING";
  default:
    return "UNKNOWN";
  }
}