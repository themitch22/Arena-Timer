//
// This is the driver for the Dallas Area Robot Combat arena control console subsystem.
//
//  Copyright (C) 2017, 2019 William Gee Jr.
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <http://www.gnu.org/licenses/>.//
//

#include <Wire.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

// Length of the fight
#define FIGHT_MINUTES 3
#define FIGHT_SECONDS 0

// I2C address of the display.  Stick with the default address of 0x70
// unless you've changed the address jumpers on the back of the display.
#define DISPLAY_ADDRESS   0x70

#define LED_LEFT_BRACKET  0x39
#define LED_RIGHT_BRACKET 0x0F
#define LED_DASH          0x40

#define LED_D             0x5E
#define LED_A             0x77
#define LED_R             0x50
#define LED_C             0x58

// Create display object.  This is a global variable that
// can be accessed from both the setup and loop function below.
Adafruit_7segment clockDisplay = Adafruit_7segment();

#define DEBOUNCE_DELAY  10        /* milliseconds for button to settle */

#define PIN_ARENA_LIGHTS  3       /* Relay for arena lights */
#define PIN_ARENA_SWITCH  8       /* Switch for arena lights*/
#define PIN_DISPLAY_POWER 9       /* Relay for display panel */
#define PIN_BELL          5       /* Relay for the bell */

#define PIN_RESET         12      /* The RESET button is on this pin */
#define PIN_GO            6       /* The GO button is on this pin */
#define PIN_PAUSE         7       /* The PAUSE button is on this pin */
#define PIN_RED_READY     11      /* The RED READY button is on this pin */
#define PIN_BLUE_READY    10      /* The BLUE READY button is on this pin */

#define STATE_IDLE            0   /* Nothing going on */
#define STATE_READY           1   /* Wait for combatants to signal ready */
#define STATE_RED_READY       2   /* Only RED is ready */
#define STATE_BLUE_READY      3   /* Only BLUE is ready */
#define STATE_READY_TO_FIGHT  4   /* Both are ready */
#define STATE_FIGHTING        5   /* Fight in progress */
#define STATE_PAUSE           6   /* Fight is paused */
#define STATE_STOP            7   /* Fight is over */

int   state = STATE_IDLE;         // The current state
int   minutes;                    // Whole minutes remaining in the fight
int   seconds;                    // Whole tens of seconds remaining in the fight
int   tenSeconds;                 // Whole seconds remaining in the fight
int   colonOn;                    // Colon is on for the first half-second, off in the second
int   enterState = 0;             // Entering a new state
int   arenaLights;                // Arena lights on or off

//
// Get the state of a button with debouncing.
//
// Inputs:
//  pin = the number of the pin attached to the button
//
// Output:
//  the return value is non-zero if the button is pushed
//
int buttonPushed(int pin)
{
  int state;
  int previousState;

  // Get button state
  previousState = digitalRead(pin);

  // Wait until it is stable
  for (int counter = 0; counter < DEBOUNCE_DELAY; counter++)
    {
    // Wait a millisecond
    delay(1);

    // Get button state
    state = digitalRead(pin);

    if (state != previousState)
      {
      // Still bouncing, reset counter
      counter = 0;

      // Remember the current state
      previousState = state;
      }
    }

  // Button is pushed if the pin is grounded
  return (state == 0);
}

//
// Display the current time in a specified color.
//
// Inputs:
//  color = the desired color for the time
//
void displayTime(char color)
{
  char  ch;

  Serial.write(color);
  ch = minutes + '0';
  Serial.write(ch);
  ch = tenSeconds + '0';
  Serial.write(ch);
  ch = seconds + '0';
  Serial.write(ch);
  if (colonOn)
    Serial.write(":");
  else
    Serial.write(";");
}

//
// Delay while optionally detecting debounced buttons.
//
// Inputs:
//  ticks = the number of milliseconds to delay
//  reset = non-zero to debounce the RESET button
//  pause = non-zero to debounce the PAUSE button
//
// Output:
//  the return value is 
//    0 if no buttons pushed
//    PIN_RESET if the RESET button is pushed
//    PIN_PAUSE if the PAUSE button is pushed
//
int delayAndDetect(int ticks, int reset, int pause)
{
  int pauseCounter = 0;
  int pausePrevious;
  int pauseState;
  int resetCounter = 0;
  int resetPrevious;
  int resetState;

  // Get button states
  if (pause)
    pausePrevious = digitalRead(PIN_PAUSE);
  if (reset)
    resetPrevious = digitalRead(PIN_RESET);

  while ((reset || pause) && ticks > DEBOUNCE_DELAY)
    {
    // Wait a millisecond
    delay(1);
    ticks--;

    // Check button states
    if (pause)
      {
      pauseState = digitalRead(PIN_PAUSE);
      if (pauseState != pausePrevious || pauseState != 0)
        {
        // Not pushed or still bouncing, reset counter
        pauseCounter = 0;

        // Remember the previous state
        pausePrevious = pauseState;
        }

      pauseCounter++;
      if (pauseCounter >= DEBOUNCE_DELAY)
        return PIN_PAUSE;
      }

    if (reset)
      {
      resetState = digitalRead(PIN_RESET);
      if (resetState != resetPrevious || resetState != 0)
        {
        // Not pushed or still bouncing, reset counter
        resetCounter = 0;

        // Remember the previous state
        resetPrevious = resetState;
        }

      resetCounter++;
      if (resetCounter >= DEBOUNCE_DELAY)
        return PIN_RESET;
      }
    }

  // Eat up what time is left
  if (ticks > 0)
    delay(ticks);

  return 0;
}

//
// Perform startup initializations.
//
void setup()
{
  // Enable output pins for relays
  pinMode(PIN_ARENA_LIGHTS, OUTPUT);
  pinMode(PIN_DISPLAY_POWER, OUTPUT);
  pinMode(PIN_BELL, OUTPUT);

  // Enable input pins for the buttons
  pinMode(PIN_RESET, INPUT);
  pinMode(PIN_GO, INPUT);
  pinMode(PIN_PAUSE, INPUT);
  pinMode(PIN_RED_READY, INPUT);
  pinMode(PIN_BLUE_READY, INPUT);
  pinMode(PIN_ARENA_SWITCH, INPUT);

  // Enable pullup resistors for the buttons
  digitalWrite(PIN_RESET, HIGH);
  digitalWrite(PIN_GO, HIGH);
  digitalWrite(PIN_PAUSE, HIGH);
  digitalWrite(PIN_RED_READY, HIGH);
  digitalWrite(PIN_BLUE_READY, HIGH);
  digitalWrite(PIN_ARENA_SWITCH, HIGH);

  // The serial port is the interface to the display module
  Serial.begin(9600);

  // Turn on display power
  digitalWrite(PIN_DISPLAY_POWER, HIGH);

  // Turn on arena lighting power if switch is on
  arenaLights = digitalRead(PIN_ARENA_SWITCH);
  digitalWrite(PIN_ARENA_LIGHTS, arenaLights);

  // Setup the LED display.
  clockDisplay.begin(DISPLAY_ADDRESS);

  // Clear display
  clockDisplay.writeDigitRaw(0, 0);
  clockDisplay.writeDigitRaw(1, 0);
  clockDisplay.drawColon(0);
  clockDisplay.writeDigitRaw(3, 0);
  clockDisplay.writeDigitRaw(4, 0);

  // Now push out to the display the new values that were set above.
  clockDisplay.writeDisplay();
}

//
// Do these things over and over again.
//
void loop()
{
  char  ch;
  int   button;
  long  endMillis;

  // Arena lights track the arena light switch
  button = digitalRead(PIN_ARENA_SWITCH);
  if (button != arenaLights)
    {
    digitalWrite(PIN_ARENA_LIGHTS, button);
    arenaLights = button;
    }

  switch (state)
    {
    //
    case STATE_IDLE:
      if (enterState)
        {
        // Initialize for the first time in this state
        enterState = 0;  

        // Put the display in chase mode
        Serial.write("C.");

        // Clear LED display
        clockDisplay.writeDigitRaw(0, 0);
        clockDisplay.writeDigitRaw(1, 0);
        clockDisplay.drawColon(0);
        clockDisplay.writeDigitRaw(3, 0);
        clockDisplay.writeDigitRaw(4, 0);
        clockDisplay.writeDisplay();
        }

      if (buttonPushed(PIN_GO))
        {
        // Wait for button release
        while (buttonPushed(PIN_GO))
          ;

        state = STATE_READY;
        enterState = 1;
        minutes = FIGHT_MINUTES;
        seconds = FIGHT_SECONDS;
        tenSeconds = 0;
        colonOn = 1;
        }
      break;

    //
    case STATE_READY:
      if (enterState)
        {
        // Initialize for the first time in this state
        enterState = 0;  
        
        // Show dim frame, total time in yellow
        Serial.write("r[b]");
        displayTime('y');
        Serial.write(".");

        // Show dashes in LED digits
        clockDisplay.writeDigitRaw(1, LED_DASH);
        clockDisplay.writeDigitRaw(3, LED_DASH);
        clockDisplay.writeDisplay();
        }

      if (buttonPushed(PIN_RESET))
        {
        // Wait for button release
        while (buttonPushed(PIN_RESET))
          ;

        state = STATE_IDLE;
        enterState = 1;
        }
      else if (buttonPushed(PIN_RED_READY))
        {
        state = STATE_RED_READY;

        // Highlight red frame
        Serial.write("R[.");

        // Show right bracket on LED
        clockDisplay.writeDigitRaw(4, LED_RIGHT_BRACKET);
        clockDisplay.writeDisplay();
        }
      else if (buttonPushed(PIN_BLUE_READY))
        {
        state = STATE_BLUE_READY;

        // Highlight blue frame
        Serial.write("B].");

        // Show left bracket on LED
        clockDisplay.writeDigitRaw(0, LED_LEFT_BRACKET);
        clockDisplay.writeDisplay();
        }
      break;

    //
    case STATE_RED_READY:
      if (buttonPushed(PIN_BLUE_READY))
        {
        state = STATE_READY_TO_FIGHT;

        // Highlight blue frame
        Serial.write("B].");

        // Show left bracket on LED
        clockDisplay.writeDigitRaw(0, LED_LEFT_BRACKET);
        clockDisplay.writeDisplay();
        }
      else if (buttonPushed(PIN_RESET))
        {
        state = STATE_READY;

        // Dim red frame
        Serial.write("r[.");

        // Clear right bracket on LED
        clockDisplay.writeDigitRaw(4, 0);
        clockDisplay.writeDisplay();

        // Wait for button release
        while (buttonPushed(PIN_RESET))
          ;
        }
      break;

    //
    case STATE_BLUE_READY:
      if (buttonPushed(PIN_RED_READY))
        {
        state = STATE_READY_TO_FIGHT;

        // Highlight red frame
        Serial.write("R[.");

        // Show right bracket on LED
        clockDisplay.writeDigitRaw(4, LED_RIGHT_BRACKET);
        clockDisplay.writeDisplay();
        }
      else if (buttonPushed(PIN_RESET))
        {
        state = STATE_READY;

        // Dim blue frame
        Serial.write("b].");

        // Clear left bracket on LED
        clockDisplay.writeDigitRaw(0, 0);
        clockDisplay.writeDisplay();

        // Wait for button release
        while (buttonPushed(PIN_RESET))
          ;
        }
      break;

    //
    case STATE_READY_TO_FIGHT:
      if (enterState)
        {
        // Initialize for the first time in this state
        enterState = 0;

        // Show time in yellow
        displayTime('y');
        Serial.write(".");

        // Restore display on LED
        clockDisplay.writeDigitRaw(0, LED_LEFT_BRACKET);
        clockDisplay.writeDigitRaw(1, LED_DASH);
        clockDisplay.writeDigitRaw(3, LED_DASH);
        clockDisplay.writeDigitRaw(4, LED_RIGHT_BRACKET);
        clockDisplay.drawColon(0);
        clockDisplay.writeDisplay();
        }

      if (buttonPushed(PIN_RESET))
        {
        state = STATE_READY;
        enterState = 1;

        // Clear ready indicators
        clockDisplay.writeDigitRaw(0, 0);
        clockDisplay.writeDigitRaw(4, 0);
        clockDisplay.writeDisplay();

        // Wait for button release
        while (buttonPushed(PIN_RESET))
          ;
        }
      else if (buttonPushed(PIN_GO))
        {
        // Wait for button release
        while (buttonPushed(PIN_GO))
          ;

        // Clear brackets on LED
        clockDisplay.writeDigitRaw(0, 0);
        clockDisplay.writeDigitRaw(1, 0);
        clockDisplay.drawColon(0);
        clockDisplay.writeDigitRaw(3, 0);
        clockDisplay.writeDigitRaw(4, 0);
        clockDisplay.writeDisplay();

#if 0
        // Wait a half second, monitoring the RESET button
        if (delayAndDetect(500, 1, 0) == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;

          // Go back to start of Ready to fight state
          enterState = 1;
          return;
          }
#endif

        // Count down from three seconds
        Serial.write(";Y&3&.");

        // Also on LED display
        clockDisplay.writeDigitNum(3, 3);
        clockDisplay.writeDisplay();

        if (delayAndDetect(1000, 1, 0) == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;

          // Go back to start of Ready state
          state = STATE_READY;
          enterState = 1;

          return;
          }
        Serial.write("2&.");

        // Also on LED display
        clockDisplay.writeDigitNum(3, 2);
        clockDisplay.writeDisplay();

        if (delayAndDetect(1000, 1, 0) == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;

          // Go back to start of Ready state
          state = STATE_READY;
          enterState = 1;

          return;
          }
        Serial.write("1&.");

        // Also on LED display
        clockDisplay.writeDigitNum(3, 1);
        clockDisplay.writeDisplay();

        if (delayAndDetect(1000, 1, 0) == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;

          // Go back to start of Ready state
          state = STATE_READY;
          enterState = 1;
          return;
          }

        displayTime('G');
        Serial.write(":.");

        // Now print the time value to the LED display.
        clockDisplay.print(300, DEC);

        // Blink the colon by flipping its value every loop iteration
        // (which happens every second).
        clockDisplay.drawColon(1);
        clockDisplay.writeDisplay();

        // Prepare to fight...
        // Turn bell on
        digitalWrite(PIN_BELL, HIGH);
        if (delayAndDetect(250, 1, 0) == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;

          // Turn bell off
          digitalWrite(PIN_BELL, LOW);

          // Go back to start of Ready to fight state
          enterState = 1;
          return;
          }

        // Turn bell off
        digitalWrite(PIN_BELL, LOW);
        if (delayAndDetect(250, 1, 0) == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;

          // Go back to start of Ready to fight state
          enterState = 1;
          return;
          }

        // Turn bell on
        digitalWrite(PIN_BELL, HIGH);
        if (delayAndDetect(250, 1, 0) == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;

          // Turn bell off
          digitalWrite(PIN_BELL, LOW);

          // Go back to start of Ready to fight state
          enterState = 1;
          return;
          }

        // Turn bell off
        digitalWrite(PIN_BELL, LOW);

        state = STATE_FIGHTING;
        enterState = 1;
        }
      break;

    //
    case STATE_FIGHTING:
      if (enterState)
        {
        // Initialize for the first time in this state
        enterState = 0;

        // Show time in green
        displayTime('G');
        if (colonOn)
          {
          Serial.write(":");

          // Show colon on LED
          clockDisplay.drawColon(true);
          }
        else
          {
          Serial.write(";");

          // Show colon on LED
          clockDisplay.drawColon(true);
          }
        Serial.write(".");

        // LED not blinking
        clockDisplay.blinkRate(0);
        clockDisplay.writeDisplay();
        }

      while (minutes > 0 || tenSeconds > 0 || seconds > 0)
        {
        // Turn off colon
        button = delayAndDetect(500, 1, 1);
        if (button == PIN_PAUSE)
          {
          // Wait for button release
          while (buttonPushed(PIN_PAUSE))
            ;
          state = STATE_PAUSE;
          enterState = 1;
          return;
          }
        else if (button == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;
          state = STATE_STOP;
          enterState = 1;
          return;
          }
        Serial.write(";.");

        // And on the LED
        clockDisplay.drawColon(false);
        clockDisplay.writeDisplay();

        colonOn = 0;

        // Turn on colon
        button = delayAndDetect(500, 1, 1);
        if (button == PIN_PAUSE)
          {
          // Wait for button release
          while (buttonPushed(PIN_PAUSE))
            ;

          // Turn colon on on the LED
          clockDisplay.drawColon(true);
          clockDisplay.writeDisplay();

          state = STATE_PAUSE;
          enterState = 1;
          return;
          }
        else if (button == PIN_RESET)
          {
          // Wait for button release
          while (buttonPushed(PIN_RESET))
            ;
          state = STATE_STOP;
          enterState = 1;
          return;
          }
        Serial.write(":");

        // And on the LED
        clockDisplay.drawColon(true);
        clockDisplay.writeDisplay();

        colonOn = 1;
        if (seconds > 0)
          {
          // Decrement seconds
          seconds--;
          ch = seconds + '0';
          Serial.write(ch);
          Serial.write(".");

          // And on the LED
          clockDisplay.writeDigitNum(4, seconds);
          clockDisplay.writeDisplay();
          }
        else
          if (tenSeconds > 0)
            {
            // Decrement tens of seconds
            tenSeconds--;
            seconds = 9;
            ch = tenSeconds + '0';
            Serial.write(ch);
            Serial.write("9.");

            // And on the LED
            clockDisplay.writeDigitNum(3, tenSeconds);
            clockDisplay.writeDigitNum(4, 9);
            clockDisplay.writeDisplay();
            }
          else
            {
            minutes--;
            ch = minutes + '0';
            tenSeconds = 5;
            seconds = 9;
            Serial.write(ch);
            Serial.write("59.");

            // And on the LED
            clockDisplay.writeDigitNum(1, minutes);
            clockDisplay.writeDigitNum(3, 5);
            clockDisplay.writeDigitNum(4, 9);
            clockDisplay.writeDisplay();
            }
        }

      Serial.write("R:000.");
      state = STATE_STOP;
      enterState = 1;

      // Turn bell on
      digitalWrite(PIN_BELL, HIGH);

      delay(1000);

      // Turn bell off
      digitalWrite(PIN_BELL, LOW);
      break;

    //
    case STATE_PAUSE:
      if (enterState)
        {
        // Initialize for the first time in this state
        enterState = 0;

        // Show time in yellow
        displayTime('Y');
        Serial.write(".");

        // Flash LED
        clockDisplay.blinkRate(2);
        }

      if (buttonPushed(PIN_GO))
        {
        state = STATE_FIGHTING;
        enterState = 1;
        clockDisplay.drawColon(0);
        clockDisplay.writeDisplay();

        // Wait for button release
        while (buttonPushed(PIN_GO))
          ;
        }
      if (buttonPushed(PIN_RESET))
        {
        state = STATE_STOP;
        enterState = 1;

        // Wait for button release
        while (buttonPushed(PIN_RESET))
          ;
        }
      break;

    //
    case STATE_STOP:
      if (enterState)
        {
        // Initialize for the first time in this state
        enterState = 0;

        // Show time in red
        displayTime('R');
        Serial.write(":.");

        // LED not flashing
        clockDisplay.blinkRate(0);

        // And on the LED
        clockDisplay.writeDigitNum(1, 0);
        clockDisplay.writeDigitNum(3, 0);
        clockDisplay.writeDigitNum(4, 0);
        clockDisplay.drawColon(1);
        clockDisplay.writeDisplay();
        }

      if (buttonPushed(PIN_RESET))
        {
        state = STATE_IDLE;

        Serial.write("C.");

        // Clear display
        clockDisplay.writeDigitRaw(0, 0);
        clockDisplay.writeDigitRaw(1, 0);
        clockDisplay.drawColon(0);
        clockDisplay.writeDigitRaw(3, 0);
        clockDisplay.writeDigitRaw(4, 0);

        // Now push out to the display the new values that were set above.
        clockDisplay.writeDisplay();

        // Wait for button release
        while (buttonPushed(PIN_RESET))
          ;
        }
      break;
    }
}

