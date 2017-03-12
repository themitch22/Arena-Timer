//
// This is the driver for the Dallas Area Robot Combat arena display subsystem.
//
//  Copyright (C) 2017 William Gee Jr.
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

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif

#define PIN 6                     /* the NeoPixel strand is attached to this pin */

//
// The display receives short command strings via I2C
//
// Recognized command characters are:
//
// 'C' - set chase mode, any subsequent command clears display and exits chase mode
// '0'..'9' - set the value of a digit
// '&' - set a blank digit
// ':' - turn on colon
// ';' - turn off colon
// 'R' - set color to bright red
// 'r' - set color to dim red
// 'G' - set color to bright green
// 'g' - set color to dim green
// 'B' - set color to bright blue
// 'b' - set color to dim blue
// 'Y' - set color to bright yellow
// 'y' - set color to dim yellow
// 'W' - set color to bright white
// 'w' - set color to dim white
// '[' - set left frame color
// ']' - set right frame color
// '.' - execute the command
//
// Each character is acted upon in strict left to right order except for '0'..'9'.
// Only the most recent three digits are kept and if fewer than three are received,
// the rightmost N digits of the display are affected.
//

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(45 + 45 + 6 + 45 + 14 + 36 + 14 + 36, 
    PIN, NEO_GRB + NEO_KHZ800);

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

int       chaseMode = 1;          // Non-zero if the display is in "chase" mode
int       numDigits = 0;          // The number of digits received
char      newDigits[3];           // The new value of each of the three digits
uint32_t  digitColors[3];         // The new color for each of the three digits
uint32_t  currentColor;           // The current color for displaying new elements

//
// These are the base LED numbers for each of the digits.
//
#define DIGIT_0_BASE  45+45+6     /* First LED of digit 0 */
#define DIGIT_1_BASE  45          /* First LED of digit 1 */
#define DIGIT_2_BASE  0           /* First LED of digit 2 */

//
// These are the number of LEDs in each segment and the segment's relative base address.
//
unsigned char segments[7][2] =
{
  { 6, 0 },
  { 7, 6 },
  { 6, 6 + 7 },
  { 7, 6 + 7 + 6 },
  { 6, 6 + 7 + 6 + 7 },
  { 7, 6 + 7 + 6 + 7 + 6 },
  { 6, 6 + 7 + 6 + 7 + 6 + 7 }
};

//
// These are the segments to light for each numeric digit.
//
unsigned char digits[][7] =
{
  { 1, 1, 1, 0, 1, 1, 1 },  // '0'
  { 1, 0, 0, 0, 1, 0, 0 },  // '1'
  { 1, 1, 0, 1, 0, 1, 1 },  // '2'
  { 1, 1, 0, 1, 1, 1, 0 },  // '3'
  { 1, 0, 1, 1, 1, 0, 0 },  // '4'
  { 0, 1, 1, 1, 1, 1, 0 },  // '5'
  { 0, 1, 1, 1, 1, 1, 1 },  // '6'
  { 1, 1, 0, 0, 1, 0, 0 },  // '7'
  { 1, 1, 1, 1, 1, 1, 1 },  // '8'
  { 1, 1, 1, 1, 1, 1, 0 }   // '9'
};

//
// Set the color for a digit.  A value of '&' denotes a blank digit.
//
// Inputs:
//  base = the number of the first LED of the digit
//  digit = the value of the digit, 0..9
//  color = the color to display the digit
//
void doDigit(unsigned char base, unsigned char digit, uint32_t color)
{
  int i;
  int j;
  int segment;

  for (segment = 0; segment < 7; segment++)
    if (digit != '&' && digits[digit][segment])
      // Segment is on
      for (j = 0; j < segments[segment][0]; j++)
        strip.setPixelColor(segments[segment][1] + j + base, color);
    else
      // Segment is off
      for (j = 0; j < segments[segment][0]; j++)
        strip.setPixelColor(segments[segment][1] + j + base, 0);
}

//
// Set the color for the colon.
//
// Inputs:
//  color = the color to display the colon
//
void doColon(uint32_t color)
{
  int i = 45 + 45;
  int j;

  for (j = 0; j < 6; j++)
    strip.setPixelColor(i + j, color);
}

//
// Set the color for the right half of the frame.
//
// Inputs:
//  color = the color to display the right frame
//
void doRightFrame(uint32_t color)
{
  int i = 17 + 14 + 45 + 6 + 45 + 45;
  int j;

  for (j = 0; j < 17 + 14 + 17; j++)
    strip.setPixelColor(i + j, color);
}


//
// Set the color for the left half of the frame.
//
// Inputs:
//  color = the color to display the left frame
//
void doLeftFrame(uint32_t color)
{
  int i = 45 + 6 + 45 + 45;
  int j;

  // The left frame is split
  for (j = 0; j < 14 + 17; j++)
    strip.setPixelColor(i + j, color);

  // Do final split portion
  i = 17 + 14 + 34 + 14 + 45 + 6 + 45 + 45;
  for (j = 0; j < 17; j++)
    strip.setPixelColor(i + j, color);
}

//
// Leaving chase mode; clear display.
//
void leavingChaseMode(void)
{
  int i = 45 + 6 + 45 + 45;
  int j;

  chaseMode = 0;

  // Clear all of the pixels
  i = 0;
  for (j = 0; j < 34 + 14 + 34 + 14 + 45 + 6 + 45 + 45; j++)
    strip.setPixelColor(i + j, 0);
}

//
// Perform startup initializations.
//
void setup(void)
{
  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
  #if defined (__AVR_ATtiny85__)
    if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  // End of trinket special code

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  Serial.begin(9600);

  // Current color is red
  currentColor = strip.Color(255, 0, 0);
}

//
// Do these things over and over again.
//
void loop(void)
{
  char  ch;

  int i;
  int j;
  int segment;
  static int current = 0;

  if (Serial.available())
    {
    ch = Serial.read();
    if (chaseMode && ch != '.')
      leavingChaseMode();
    switch (ch)
      {
      case 'C':
        // Set chase mode
        chaseMode = 1;
        break;

      case '.':
        // End of line
        if (numDigits > 2)
          // Got at least three new digits
          doDigit(DIGIT_0_BASE, newDigits[2], digitColors[2]);
        if (numDigits > 1)
          // Got at least two new digits
          doDigit(DIGIT_1_BASE, newDigits[1], digitColors[1]);
        if (numDigits > 0)
          // Got at least one new digit
          doDigit(DIGIT_2_BASE, newDigits[0], digitColors[0]);
        numDigits = 0;

        strip.show();
        break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '&':
        // Got a new digit
        // Shift previous digits over
        newDigits[2] = newDigits[1];
        newDigits[1] = newDigits[0];
        digitColors[2] = digitColors[1];
        digitColors[1] = digitColors[0];

        // Remember the latest digit
        if (ch != '&')
          newDigits[0] = ch - '0';
        else
          // This digit is blank
          newDigits[0] = '&';
        digitColors[0] = currentColor;
        if (numDigits < 3)
          numDigits++;
        break;

      case 'R':
        // Set bright red color
        currentColor = strip.Color(255, 0, 0);
        break;

      case 'r':
        // Set dim red color
        currentColor = strip.Color(47, 0, 0);
        break;

      case 'G':
        // Set bright green color
        currentColor = strip.Color(0, 255, 0);
        break;

      case 'g':
        // Set dim green color
        currentColor = strip.Color(0, 31, 0);
        break;

      case 'B':
        // Set bright blue color
        currentColor = strip.Color(0, 0, 255);
        break;

      case 'b':
        // Set dim blue color
        currentColor = strip.Color(0, 0, 31);
        break;

      case 'Y':
        // Set bright yellow color
        currentColor = strip.Color(255, 255, 0);
        break;

      case 'y':
        // Set dim yellow color
        currentColor = strip.Color(127, 127, 0);
        break;

      case 'W':
        // Set bright white color
        currentColor = strip.Color(255, 255, 255);
        break;

      case 'w':
        // Set dim white color
        currentColor = strip.Color(63, 63, 63);
        break;

      case ':':
        // Turn colon on
        doColon(currentColor);
        break;

      case ';':
        // Turn colon off
        doColon(0);
        break;

      case '[':
        // Set left frame color
        doLeftFrame(currentColor);
        break;

      case ']':
        // Set right frame color
        doRightFrame(currentColor);
        break;
      }
    }
  else
    if (chaseMode)
      theaterChase(strip.Color(127, 127, 127), 50); // White
}

//Theatre-style crawling lights with rainbow effect
void theaterChaseRainbow(uint8_t wait) {
  for (int j=0; j < 256; j++) {     // cycle all 256 colors in the wheel
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, Wheel( (i+j) % 255));    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

//Theatre-style crawling lights.
void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<10; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, c);    //turn every third pixel on
      }
      strip.show();

      delay(wait);

      for (int i=0; i < strip.numPixels(); i=i+3) {
        strip.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

