/*********
  Complete project details at https://randomnerdtutorials.com

  This is an example for our Monochrome OLEDs based on SSD1306 drivers. Pick one up today in the adafruit shop! ------> http://www.adafruit.com/category/63_98
  This example is for a 128x32 pixel display using I2C to communicate 3 pins are required to interface (two I2C and one reset).
  Adafruit invests time and resources providing this open source code, please support Adafruit and open-source hardware by purchasing products from Adafruit!
  Written by Limor Fried/Ladyada for Adafruit Industries, with contributions from the open source community. BSD license, check license.txt for more information All text above, and the splash screen below must be included in any redistribution.
*********/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include "display-oled.h"
// #include <WiFi.h>

// OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

String textMessage = "Set up OLED...";

void displaysetup() {

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ; // Don't proceed, loop forever
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  Serial.println(F("Initializing OLED display"));  
  display.display();
  delay(2000); // Pause for 2 seconds

  // Clear the screen and prepare for drawing lines
  displayText(textMessage);

}

void displayText(String message) {

  display.clearDisplay();

  display.setTextSize(1.5);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start top left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font

  display.println(message);
  display.display();
}

void displayWeight(String weight, float voltage) {
  display.clearDisplay();

  // Draw weight on the left
  display.setTextSize(3.5);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print(weight);
  display.println("g");

  // Draw battery indicator in bottom-right (horizontal, compact)
  const int bw = 18; // battery body width
  const int bh = 8;  // battery body height
  const int tipW = 2; // tip width
  const int x = SCREEN_WIDTH - bw - tipW; // left of battery body
  const int y = SCREEN_HEIGHT - bh; // bottom aligned

  if (!isnan(voltage)) {
    // If external USB (~>=4.5V) or ADC reads very low treat as USB
    if (voltage >= 4.4 || voltage < 0.10) {
      // draw lightning bolt centered in the battery area
      const int bx = x + bw/2 - 6;
      const int by = y;
      // horizontal lightning bolt (points to the right)
      display.drawLine(bx,     by+3, bx+6,  by+1, WHITE);
      display.drawLine(bx+6,  by+1, bx+6,  by+5, WHITE);
      display.drawLine(bx+6,  by+5, bx+12, by+3, WHITE);
      // also draw a small filled battery outline to indicate presence
      display.drawRect(x, y, bw, bh, WHITE);
      display.fillRect(x + bw + 1, y + (bh/2) - 1, tipW, 2, WHITE);
    } else {
      // Constrain voltage to cell range 3.3..4.2
      float v = voltage;
      if (v < 3.3) v = 3.3;
      if (v > 4.2) v = 4.2;
      float pct = (v - 3.3) / (4.2 - 3.3);
      if (pct < 0) pct = 0; if (pct > 1) pct = 1;

      // draw battery outline and tip
      display.drawRect(x, y, bw, bh, WHITE);
      display.fillRect(x + bw, y + (bh/2) - (tipW/2), tipW, tipW, WHITE);

      // inner padding
      const int innerPad = 2;
      const int innerW = bw - innerPad*2;
      const int innerH = bh - innerPad*2;
      int fillW = (int)round(innerW * pct);
      // draw level from left to right
      if (fillW > 0) {
        display.fillRect(x + innerPad, y + innerPad, fillW, innerH, WHITE);
      }
    }

    // Print numeric voltage to left of icon
    char vb[8];
    snprintf(vb, sizeof(vb), "%.1fV", voltage);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    int tx = x - 2 - (6 * (int)strlen(vb)); // rough width estimate (6 px per char)
    if (tx < 0) tx = 0;
    display.setCursor(tx, y);
    display.print(vb);
  }

  display.display();
}

