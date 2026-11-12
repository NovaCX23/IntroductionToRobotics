/*
  Homework 5 - Stop the Counter!

  A reaction game on a four-digit seven-segment display (driven through
  a 74HC595 shift register) controlled with a joystick.
  The display shows a target number. Push the joystick UP to start a
  countdown from 9999, then press the joystick button to stop it as close
  to the target as possible. The error |stopped value - target| is printed
  to the Serial Monitor. Push DOWN to reset and play again.

  Controls:
    - Joystick UP:     start the countdown (from the target screen)
    - Joystick button: stop the countdown
    - Joystick DOWN:   reset the game (after a stop, or to abort a round)

  Circuit:
    - 74HC595: VCC -> 3V3, GND -> GND, MR -> 3V3, OE -> GND
               DS -> GPIO23, SHCP -> GPIO18, STCP -> GPIO22
    - Q7..Q0 -> 1 kohm -> segments A, B, C, D, E, F, G, DP
    - Display digits D1-D4 (common cathode, D1 = leftmost) -> GPIO25, 26, 27, 33
    - Joystick (powered from 3V3): VRx -> GPIO34, VRy -> GPIO35, SW -> GPIO32

  Based on the Laboratory 4 examples (writeReg, byteEncodings, joystick)
  and on my Homework 4 debouncing code.

  Author: Xhamani Christian
  Date: 12.11.2026
*/

#include <Arduino.h>

// 74HC595 connections
const uint8_t dataPin = 23;   // DS
const uint8_t clockPin = 18;  // SHCP
const uint8_t latchPin = 22;  // STCP

// Four-digit display connections (index 0 = D1 = leftmost digit)
const uint8_t displayDigits[4] = {25, 26, 27, 33};
const uint8_t displayCount = 4;

// Joystick connections
const uint8_t joystickXPin = 34;
const uint8_t joystickYPin = 35;
const uint8_t joystickSWPin = 32;

// Display type: false = common cathode (my display)
const bool COMMON_ANODE = false;
const uint8_t DIGIT_ON = COMMON_ANODE ? HIGH : LOW;
const uint8_t DIGIT_OFF = COMMON_ANODE ? LOW : HIGH;

// Segment order: A B C D E F G DP (bit 7 = A ... bit 0 = DP)
const uint8_t blankPattern = 0b00000000;

// Digit encodings 0-9
const uint8_t byteEncodings[10] = {
  0b11111100, // 0
  0b01100000, // 1
  0b11011010, // 2
  0b11110010, // 3
  0b01100110, // 4
  0b10110110, // 5
  0b10111110, // 6
  0b11100000, // 7
  0b11111110, // 8
  0b11110110  // 9
};

// Number formatting
const int numberBase = 10;
const int maxDisplayNumber = 9999;

// Game settings: 9999 -> 5000 at 1 unit/ms takes about 5 s
const int defaultTargetNumber = 5000;
const int countdownStartValue = 9999;
const unsigned long countdownStepMs = 1;  // time for the counter to drop by 1

// Joystick calibration (measured with the diagnostic program)
const int joystickXCenter = 1950;
const int joystickYCenter = 1925;
const int joystickTriggerThreshold = 1200;  // distance from center that counts as a move
const int joystickDeadZone = 600;           // both axes closer than this = back in the center

// Serial
const unsigned long serialBaudRate = 115200;

// Timing
const unsigned long refreshIntervalMs = 2;        // time per digit while multiplexing
const unsigned long joystickReadIntervalMs = 10;
const unsigned long debounceDelayMs = 30;

// Joystick directions
enum JoystickDirection
{
  JOYSTICK_NONE,
  JOYSTICK_UP,
  JOYSTICK_DOWN,
  JOYSTICK_LEFT,
  JOYSTICK_RIGHT
};

// Game states
enum GameState
{
  STATE_SHOW_TARGET,  // target on the display, waiting for UP
  STATE_COUNTING,     // countdown running, waiting for the button
  STATE_STOPPED       // stopped value frozen, waiting for DOWN
};

// Display state: one segment pattern per digit
uint8_t displayBuffer[4] = {blankPattern, blankPattern, blankPattern, blankPattern};
uint8_t currentDigit = 0;
unsigned long lastRefreshTime = 0;

// Joystick state (edge detection)
bool joystickCentered = true;  // true once the stick has come back to the center
unsigned long lastJoystickReadTime = 0;

// Button state (debouncing)
int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;

// Game state
GameState gameState = STATE_SHOW_TARGET;
int targetNumber = defaultTargetNumber;
int counterValue = countdownStartValue;   // value currently shown during the countdown
unsigned long countdownStartTime = 0;

// Function prototypes
void writeReg(uint8_t logicalPattern);
void turnOffAllDigits();
void refreshDisplay(unsigned long currentTime);
void setDisplayNumber(int number);
void updateJoystick(unsigned long currentTime);
JoystickDirection getJoystickDirection(int xOffset, int yOffset);
void updateButton(unsigned long currentTime);
void onJoystickMoved(JoystickDirection direction, unsigned long currentTime);
void onButtonPressed();
void updateCountdown(unsigned long currentTime);
void startCountdown(unsigned long currentTime);
void stopCountdown();
void resetGame();
void printControls();

void setup()
{
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(latchPin, OUTPUT);
  digitalWrite(dataPin, LOW);
  digitalWrite(clockPin, LOW);
  digitalWrite(latchPin, LOW);

  // All digits off at startup
  for (uint8_t i = 0; i < displayCount; i++)
  {
    pinMode(displayDigits[i], OUTPUT);
    digitalWrite(displayDigits[i], DIGIT_OFF);
  }
  writeReg(blankPattern);

  // SW: internal pull-up, pressed = LOW
  pinMode(joystickSWPin, INPUT_PULLUP);
  lastButtonReading = digitalRead(joystickSWPin);
  buttonState = lastButtonReading;

  Serial.begin(serialBaudRate);
  Serial.println("Stop the Counter!");
  printControls();

  resetGame();
}

void loop()
{
  unsigned long currentTime = millis();
  updateButton(currentTime);
  updateJoystick(currentTime);
  updateCountdown(currentTime);
  refreshDisplay(currentTime);
}

// Send one byte to the 74HC595, MSB (segment A) first
void writeReg(uint8_t logicalPattern)
{
  // Convert logical segments to electrical levels
  uint8_t pattern = COMMON_ANODE ? static_cast<uint8_t>(~logicalPattern) : logicalPattern;

  digitalWrite(latchPin, LOW);

  // Transmit all 8 bits, from bit 7 down to bit 0
  for (int i = 7; i >= 0; i--)
  {
    uint8_t bit = (pattern >> i) & 0x01;
    digitalWrite(clockPin, LOW);
    digitalWrite(dataPin, bit);
    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);
  }

  // Copy the shifted bits to the outputs
  digitalWrite(latchPin, HIGH);
}

void turnOffAllDigits()
{
  for (uint8_t i = 0; i < displayCount; i++)
  {
    digitalWrite(displayDigits[i], DIGIT_OFF);
  }
}

// Multiplexing: show the next digit from displayBuffer every refreshIntervalMs
void refreshDisplay(unsigned long currentTime)
{
  if (currentTime - lastRefreshTime < refreshIntervalMs)
  {
    return;
  }
  lastRefreshTime = currentTime;

  // Turn everything off first, so the old pattern never shows on the new digit
  turnOffAllDigits();
  writeReg(displayBuffer[currentDigit]);
  digitalWrite(displayDigits[currentDigit], DIGIT_ON);

  currentDigit = (currentDigit + 1) % displayCount;
}

/*
  Fill displayBuffer with the patterns for number (0-9999).
  Digits are taken from the right (units) to the left; once nothing is
  left of the number, the remaining positions are leading zeros and are
  blanked. The units digit is always shown, so 0 is displayed as "0".
*/
void setDisplayNumber(int number)
{
  int remaining = constrain(number, 0, maxDisplayNumber);

  // From the rightmost position (units) to the leftmost (thousands)
  for (int position = displayCount - 1; position >= 0; position--)
  {
    bool isUnitsPosition = (position == displayCount - 1);

    if (remaining == 0 && !isUnitsPosition)
    {
      displayBuffer[position] = blankPattern;
    }
    else
    {
      displayBuffer[position] = byteEncodings[remaining % numberBase];
    }

    remaining = remaining / numberBase;
  }
}

/*
  Read the joystick and report each move only once (edge detection).
  A move is reported when the stick goes past joystickTriggerThreshold,
  and the next move is accepted only after it returns inside the dead zone.
*/
void updateJoystick(unsigned long currentTime)
{
  if (currentTime - lastJoystickReadTime < joystickReadIntervalMs)
  {
    return;
  }
  lastJoystickReadTime = currentTime;

  int xOffset = analogRead(joystickXPin) - joystickXCenter;
  int yOffset = analogRead(joystickYPin) - joystickYCenter;

  // Stick back in the center: ready for the next move
  if (abs(xOffset) < joystickDeadZone && abs(yOffset) < joystickDeadZone)
  {
    joystickCentered = true;
    return;
  }

  // Still held from the previous move: ignore
  if (!joystickCentered)
  {
    return;
  }

  JoystickDirection direction = getJoystickDirection(xOffset, yOffset);
  if (direction != JOYSTICK_NONE)
  {
    joystickCentered = false;
    onJoystickMoved(direction, currentTime);
  }
}

/*
  Turn the offsets from the center into a direction.
  Only the axis that moved the most is used, so a slightly diagonal push
  is not read as two moves. My joystick: X grows to the right,
  Y grows downwards (pushing UP lowers Y).
*/
JoystickDirection getJoystickDirection(int xOffset, int yOffset)
{
  if (abs(xOffset) >= abs(yOffset))
  {
    if (xOffset > joystickTriggerThreshold)
    {
      return JOYSTICK_RIGHT;
    }
    if (xOffset < -joystickTriggerThreshold)
    {
      return JOYSTICK_LEFT;
    }
  }
  else
  {
    if (yOffset < -joystickTriggerThreshold)
    {
      return JOYSTICK_UP;
    }
    if (yOffset > joystickTriggerThreshold)
    {
      return JOYSTICK_DOWN;
    }
  }
  return JOYSTICK_NONE;
}

// Debounce the joystick button and react once per confirmed press
void updateButton(unsigned long currentTime)
{
  int reading = digitalRead(joystickSWPin);

  // Reset the debounce timer whenever the raw input changes
  if (reading != lastButtonReading)
  {
    lastDebounceTime = currentTime;
  }

  // Accept a new state only after it stayed the same for debounceDelayMs
  if (currentTime - lastDebounceTime >= debounceDelayMs && reading != buttonState)
  {
    buttonState = reading;

    // Act on the press (HIGH -> LOW): fastest reaction for a reaction game
    if (buttonState == LOW)
    {
      onButtonPressed();
    }
  }

  lastButtonReading = reading;
}

// UP starts a round from the target screen, DOWN resets after a stop or aborts a round
void onJoystickMoved(JoystickDirection direction, unsigned long currentTime)
{
  if (direction == JOYSTICK_UP && gameState == STATE_SHOW_TARGET)
  {
    startCountdown(currentTime);
  }
  else if (direction == JOYSTICK_DOWN && gameState != STATE_SHOW_TARGET)
  {
    resetGame();
  }
}

// The button stops the countdown (ignored in the other states)
void onButtonPressed()
{
  if (gameState == STATE_COUNTING)
  {
    stopCountdown();
  }
}

/*
  While counting, compute the counter from the time elapsed since the start
  (not by subtracting 1 on every loop), so a slow loop never slows the game.
  Stops automatically at 0.
*/
void updateCountdown(unsigned long currentTime)
{
  if (gameState != STATE_COUNTING)
  {
    return;
  }

  unsigned long stepsDone = (currentTime - countdownStartTime) / countdownStepMs;

  // Reached zero without a stop: end the round automatically
  if (stepsDone >= (unsigned long)countdownStartValue)
  {
    counterValue = 0;
    setDisplayNumber(counterValue);
    Serial.println("Time's up! The counter reached 0.");
    stopCountdown();
    return;
  }

  // Only rewrite the display buffer when the value changes
  int newValue = countdownStartValue - (int)stepsDone;
  if (newValue != counterValue)
  {
    counterValue = newValue;
    setDisplayNumber(counterValue);
  }
}

// Start a new round: the countdown begins from countdownStartValue
void startCountdown(unsigned long currentTime)
{
  gameState = STATE_COUNTING;
  countdownStartTime = currentTime;
  counterValue = countdownStartValue;
  setDisplayNumber(counterValue);

  Serial.print("Countdown started from ");
  Serial.println(countdownStartValue);
}

// Freeze the display on the current value and print the result
void stopCountdown()
{
  gameState = STATE_STOPPED;
  int error = abs(counterValue - targetNumber);

  Serial.print("Target: ");
  Serial.print(targetNumber);
  Serial.print(" | Stopped at: ");
  Serial.print(counterValue);
  Serial.print(" | Error: ");
  Serial.println(error);
  Serial.println("Push DOWN to play again.");
}

// Back to the initial state: show the target and wait for UP
void resetGame()
{
  gameState = STATE_SHOW_TARGET;
  counterValue = countdownStartValue;
  setDisplayNumber(targetNumber);

  Serial.print("Target: ");
  Serial.print(targetNumber);
  Serial.println(" - push UP to start.");
}

// Print the controls once at startup
void printControls()
{
  Serial.println("Controls: UP = start | button = stop | DOWN = reset");
}