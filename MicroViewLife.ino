/*
Copyright (c) 2014 Scott Allen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

// -----------------------------------------------
// Conway's Game Of Life for the MicroView Arduino
// -----------------------------------------------

// Note: buttons are only tested once each generation, so at slow speeds
// a button may have to be held down for up to 2 seconds to be detected.

#include <MicroView.h>

// Width and height of the screen in pixels (one pixel per cell)
#define LIFEWIDTH LCDWIDTH
#define LIFEHEIGHT LCDHEIGHT // must be a multiple of 8

// Control buttons.
// Normally open pushbuttons placed between the pin and GND.
const int btnNew = 2; // pin for button to start a new game
const int btnReplay = 3; // pin for button to restart the same game
const int btnSlower = 5; // pin for button to decrease the speed
const int btnFaster = 6; // pin for button to increase the speed

const int debounceWait = 50; // delay for button debounce

// Pointer to the screen buffer as a two dimensional array,
// the way the Life engine wants it.
static uint8_t (*grid)[LIFEWIDTH];

unsigned int speedDly = 64;
unsigned char speedNum = 6;

long randSeed = 12345;

void setup() {
  pinMode(btnNew, INPUT_PULLUP);
  pinMode(btnReplay, INPUT_PULLUP);
  pinMode(btnSlower, INPUT_PULLUP);
  pinMode(btnFaster, INPUT_PULLUP);

  uView.begin();
  uView.clear(PAGE);
  grid = (uint8_t (*)[LIFEWIDTH]) uView.getScreenBuffer();

  // This will be the initial grid pattern
  // Note that the restart button won't restart this pattern
  uView.setFontType(0); // 5x7 font
  uView.setCursor(5, 2);
  uView.print("MicroView");
  uView.setCursor(8, 16);
  uView.print("CONWAY'S");
  uView.setCursor(11, 24);
  uView.print("Game of");
  uView.setCursor(20, 32);
  uView.print("LIFE");

  uView.display();
  delay(4000);
}

void loop() {
  lifeIterate(grid);
  uView.display();
  delay(speedDly);

  if (digitalRead(btnNew) == LOW) {
    newGame();
  }
  if (digitalRead(btnReplay) == LOW) {
    replayGame();
  }
  if (digitalRead(btnSlower) == LOW) {
    goSlower();
  }
  if (digitalRead(btnFaster) == LOW) {
    goFaster();
  }
}

// Handler for new game button
void newGame(void) {
  uView.clear(PAGE, 0);
  delay(debounceWait);
  do {
    randSeed++;
  } while (digitalRead(btnNew) == LOW);
  genGrid(randSeed);
}

// Handler for restart button
void replayGame() {
  uView.clear(PAGE, 0);
  genGrid(randSeed);
  debounce(btnReplay);
}

// Generate a new game grid.
// Places a random number of random characters from the current font
// at random positions on the screen.
void genGrid(long seed) {
  int numChars;
  int maxX = LIFEWIDTH - uView.getFontWidth() + 1;
  int maxY = LIFEHEIGHT - uView.getFontHeight() + 1;
  uint8_t fontStart = uView.getFontStartChar();
  uint8_t fontEnd = fontStart + uView.getFontTotalChar();

  randomSeed(seed);
  numChars = random(5, 20);  
  for (int c = 0; c <= numChars; c++) {
    uView.drawChar(random(maxX), random(maxY),
                   random(fontStart, fontEnd));
  }
  uView.display();
  delay(3000);
}

// Handler for the decrease speed button
// Halves the current speed, down to a minimum of about 2 seconds
void goSlower() {
  if (speedDly < 16) {
    speedDly = 16;
    speedNum = 8;
  }
  else if (speedDly <= 1024) {
    speedDly *= 2;
    speedNum--;
  }
  showSpeed();
  debounce(btnSlower);
}

// Handler for the increase speed button
// Doubles the current speed, up to the maximum possible 
void goFaster() {
  if (speedDly == 16) {
    speedDly = 0;
    speedNum = 9;
  }
  else if (speedDly > 0) {
    speedDly /= 2;
    speedNum++;
  }
  showSpeed();
  debounce(btnFaster);
}

// Display the speed level
// Leave the screen buffer unaltered
void showSpeed() {
  uint8_t saveBuf[64]; // screen buffer save area

  memcpy(saveBuf, grid, sizeof saveBuf);
  uView.setCursor(0, 0);
  uView.print("Speed ");
  uView.print(speedNum);
  uView.print(" ");
  uView.display();
  memcpy(grid, saveBuf, sizeof saveBuf);
}

// Simple button debounce
void debounce(int button) {
  delay(debounceWait);
  while (digitalRead(button) == LOW) {}
  delay(debounceWait);
}

//------------------------------------------------------------
// The Life engine
//
// Updates the provided grid with the next generation.
//
// Only the torus type finite grid is implemented. (Left edge joins to the
// right edge and top edge joins to the bottom edge.)
//
// The grid is mapped as horizontal rows of bytes where each byte is a
// vertical line of cells with the least significant bit being the top cell.

// LIFEWIDTH and LIFEHEIGHT must have been previously defined.
// LIFEWIDTH is the width of the grid.
// LIFEHEIGHT is the height of the grid and must be a multiple of 8.
#define LIFELINES (LIFEHEIGHT / 8)
#define LIFEHIGHROW (LIFELINES - 1)
#define LIFEHIGHCOL (LIFEWIDTH - 1)

// number of bits in values from 0 to 7
static const unsigned char bitCount[] = { 0, 1, 1, 2, 1, 2, 2, 3 };

void lifeIterate(uint8_t grid[][LIFEWIDTH]) {
  uint8_t cur[LIFELINES][LIFEWIDTH]; // working copy of the current grid

  unsigned char row, col; // current row & column numbers
  unsigned char rowA, rowB, colR; // row above, row below, column to the right
  unsigned int left, centre, right; // packed vertical cell groups

  memcpy(cur, grid, sizeof cur);

  rowA = LIFEHIGHROW;
  rowB = 1;
  for (row = 0; row < LIFELINES ; row++) {
    left = (((unsigned int) (cur[rowA][LIFEHIGHCOL])) >> 7) |
           (((unsigned int) cur[row][LIFEHIGHCOL]) << 1) |
           (((unsigned int) cur[rowB][LIFEHIGHCOL]) << 9);

    centre = (((unsigned int) (cur[rowA][0])) >> 7) |
             (((unsigned int) cur[row][0]) << 1) |
             (((unsigned int) cur[rowB][0]) << 9);

    colR = 1;
    for (col=0; col < LIFEWIDTH; col++) {
      right = (((unsigned int) (cur[rowA][colR])) >> 7) |
              (((unsigned int) cur[row][colR]) << 1) |
              (((unsigned int) cur[rowB][colR]) << 9);

      grid[row][col] = lifeByte(left, centre, right);

      left = centre;
      centre = right;

      colR = (colR < LIFEHIGHCOL) ? colR + 1 : 0;
    }
    rowA = (rowA < LIFEHIGHROW) ? rowA + 1 : 0;
    rowB = (rowB < LIFEHIGHROW) ? rowB + 1 : 0;
  }
}

// Calculate the next generation for 8 vertical cells (one byte of the
// array) that have been packet along with their neighbours into ints.
uint8_t lifeByte(unsigned int left, unsigned int centre, unsigned int right) {
  unsigned char count;
  uint8_t newByte = 0;

  for (unsigned char i=0; i<8; i++) {
    count = bitCount[left & 7] + bitCount[centre & 7] + bitCount[right & 7];

    if ((count == 3) || ((count == 4) && (centre & 2))) {
      newByte |= (1 << i);
    }
    left >>= 1;
    centre >>= 1;
    right >>= 1;
  }
  return newByte;
}

