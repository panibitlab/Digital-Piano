/*
 * LCD Digital Piano 🎹
 * --------------------
 * Created: August 2026
 * Author: Parnian Ghorbani
 *
 * A simple digital piano built with Arduino,
 * a 4x4 matrix keypad, buzzer, and 16x2 I2C LCD.
 *
 * Features:
 * - 16 musical notes
 * - Real-time sound using tone()
 * - Custom LCD note graphics
 * - Scrolling note patterns
 * - Non-blocking animation using millis()
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// frequency of each musical note in Hz
#define C4   262
#define CS4  277
#define D4   294
#define DS4  311
#define E4   330
#define F4   349
#define FS4  370
#define G4   392
#define GS4  415 
#define A4   440
#define AS4  466
#define B4   494
#define C5   523
#define CS5  554
#define D5   587
#define DS5  622

const int buzzer = 8;

// KEYPAD
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', '4'},
  {'5', '6', '7', '8'},
  {'9', 'A', 'B', 'C'},
  {'D', 'E', 'F', 'G'}
};
// Hardware connections for keypad
byte rowPins[ROWS] = {3, 2, 1, 0};
byte colPins[COLS] = {4, 5, 6, 7};
// Configure the 4x4 matrix keypad
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// NOTE BITMAPS used to create custom note characters on the LCD
// NOTE1 (2 chars)
byte name0x2[] = { B00000,B00000,B00000,B00000,B00001,B00001,B00001,B00001 };
byte name1x2[] = { B00001,B00001,B00001,B00111,B01111,B00110,B00000,B00000 };

// NOTE2 (3 chars)
byte name0x4[] = { B00000,B00000,B00000,B00001,B00001,B00001,B00001,B00001 };
byte name0x5[] = { B00000,B00000,B00000,B00000,B11000,B00100,B11000,B00100 };
byte name1x4[] = { B00001,B00001,B00111,B01001,B00110,B00000,B00000,B00000 };

// NOTE3 (4 chars)
byte name0x7[] = { B00000,B00000,B00000,B00000,B00000,B00011,B00010,B00010 };
byte name0x8[] = { B00000,B00000,B00000,B00000,B00000,B11110,B00010,B00010 };
byte name1x7[] = { B00010,B00010,B00010,B01110,B11110,B01100,B00000,B00000 };
byte name1x8[] = { B00010,B00010,B00010,B01110,B11110,B01100,B00000,B00000 };

// NOTE4 (4 chars)
byte name0x10[] = { B00000,B00000,B00000,B00000,B00000,B00110,B01111,B01110 };
byte name0x11[] = { B00000,B00000,B00000,B00000,B00000,B00110,B01111,B01110 };
byte name1x10[] = { B01000,B01000,B01000,B01111,B01000,B01111,B00000,B00000 };
byte name1x11[] = { B00100,B00100,B00100,B11100,B00100,B11100,B00000,B00000 };

// NOTE5 (3 chars)
byte name1x13[] = { B00001,B00001,B00111,B01001,B00110,B00000,B00000,B00000 };
byte name0x13[] = { B00000,B00000,B00000,B00000,B00001,B00001,B00001,B00001 };
byte name0x14[] = { B00000,B00000,B00000,B00000,B10000,B01000,B00100,B00000 };

// NOTE STRUCTURE (Store the bitmap data, character count and LCD width of each note)
struct Note {byte charCount; byte width; byte* chars[4];};

// NOTES database
Note notes[] = {
  // N1
  {2, 1, {name0x2, name1x2}},
  // N2
  {3, 2, {name0x4, name0x5, name1x4}},
  // N3
  {4, 2, {name0x7, name1x7, name0x8, name1x8}},
  // N4
  {4, 2, {name0x10, name1x10, name0x11, name1x11}},
  // N5
  {3, 2, {name0x13, name1x13, name0x14}}
};

// Give readable names to note indexes
enum NoteID {N1, N2, N3, N4, N5};
// Define combinations of notes used as scrolling patterns
struct Pattern {NoteID noteList[3]; byte count;};
Pattern patterns[] = {
  {{N3, N4}, 2},
  {{N3, N5}, 2},
  {{N3, N2}, 2},
  {{N3, N1}, 2},
  {{N1, N2}, 2},
  {{N2, N1, N5}, 3},
  {{N1, N5}, 2},
  {{N4, N5}, 2},
  {{N4, N2}, 2},
  {{N3, N1}, 2},
  {{N2, N5}, 2}
};
// Automatically calculate the number of available patterns
const byte PATTERN_COUNT = sizeof(patterns) / sizeof(patterns[0]);

// Timing and layout settings
const byte NOTE_GAP = 2;
const unsigned long SCROLL_INTERVAL = 200;
const unsigned long DEMO_TIMEOUT = 5000;

// Store the current operating state of the piano (FSM)
bool demoMode = false;
bool notePressed = false;
unsigned long lastKeyTime = 0;
unsigned long lastScroll = 0;
byte currentPattern = 0;
int demoSteps = 0;

// Convert a keypad key into its corresponding note frequency
int getFrequency(char key) {

  switch (key) {

    case '1': return C4; case '2': return CS4; case '3': return D4; case '4': return DS4;
    case '5': return E4; case '6': return F4; case '7': return FS4; case '8': return G4;
    case '9': return GS4; case 'A': return A4; case 'B': return AS4; case 'C': return B4;
    case 'D': return C5; case 'E': return CS5; case 'F': return D5; case 'G': return DS5;
  }
  return 0;
}

// Map each keypad key to an LCD note pattern
byte getPattern(char key) {

  switch (key) {

    case '1': return 0; case '2': return 1; case '3': return 2; case '4': return 3;
    case '5': return 4; case '6': return 5; case '7': return 6; case '8': return 7;
    case '9': return 8; case 'A': return 9; case 'B': return 10; case 'C': return 0;
    case 'D': return 1; case 'E': return 2; case 'F': return 3; case 'G': return 4;
  }
  return 0;
}

// Load the bitmap data of a pattern into the LCD custom character slots (MAX = 8)
void loadPattern(Pattern &p) {

  byte charIndex = 0;

  for (byte n = 0; n < p.count; n++) {

    Note &note = notes[p.noteList[n]];

    for (byte c = 0; c < note.charCount; c++) {lcd.createChar(charIndex + c, note.chars[c]);}
    charIndex += note.charCount;
  }
}

// Draw a complete note by placing its custom characters on the LCD
void drawNote(NoteID id, int x, byte &charIndex) {

  Note &note = notes[id];

  if (id == N1) {
    lcd.setCursor(x, 0); lcd.write(charIndex);
    lcd.setCursor(x, 1); lcd.write(charIndex + 1);
  }


  else if (id == N2) {
    lcd.setCursor(x, 0); lcd.write(charIndex);
    lcd.setCursor(x + 1, 0); lcd.write(charIndex + 1);
    lcd.setCursor(x, 1); lcd.write(charIndex + 2);
  }

  else if (id == N3) {
    lcd.setCursor(x, 0); lcd.write(charIndex);
    lcd.setCursor(x, 1); lcd.write(charIndex + 1);
    lcd.setCursor(x + 1, 0); lcd.write(charIndex + 2);
    lcd.setCursor(x + 1, 1); lcd.write(charIndex + 3);
  }

  else if (id == N4) {
    lcd.setCursor(x, 0); lcd.write(charIndex);
    lcd.setCursor(x, 1); lcd.write(charIndex + 1);
    lcd.setCursor(x + 1, 0); lcd.write(charIndex + 2);
    lcd.setCursor(x + 1, 1); lcd.write(charIndex + 3);
  }

  else if (id == N5) {
    lcd.setCursor(x, 0); lcd.write(charIndex);
    lcd.setCursor(x, 1); lcd.write(charIndex + 1);
    lcd.setCursor(x + 1, 0); lcd.write(charIndex + 2);
  }
  charIndex += note.charCount;
}

// Draw all notes in a pattern with a fixed gap between them
void drawPattern(Pattern &p, int startX) {

  byte charIndex = 0;
  int x = startX;

  for (byte n = 0; n < p.count; n++) {

    drawNote(p.noteList[n], x, charIndex);
    x += notes[p.noteList[n]].width + NOTE_GAP;
  }
}

// Display the pattern associated with the currently pressed key
void showPressedPattern(char key) {

  lcd.clear();

  byte patternIndex = getPattern(key);
  Pattern &p = patterns[patternIndex];

  loadPattern(p);

  drawPattern(p, 4);
}

// Start demo (scrolling the current pattern into the screen from the right)
void startDemoPattern() {

  lcd.clear();
  Pattern &p = patterns[currentPattern];
  loadPattern(p);
  drawPattern(p, 16);

  int width = 0;

  for (byte i = 0; i < p.count; i++) {

    width += notes[p.noteList[i]].width;
    if (i < p.count - 1) width += NOTE_GAP;
  }

  demoSteps = 16 + width;

  lastScroll = millis();
  demoMode = true;
}
// Stop demo
void stopDemo() {demoMode = false; lcd.clear();}

// Update demo scroll (Update the scrolling animation without blocking the main loop)
void updateDemo() {

  if (!demoMode) return;

  unsigned long now = millis();

  if (now - lastScroll >= SCROLL_INTERVAL) {

    lastScroll = now;
    lcd.scrollDisplayLeft();

    demoSteps--;

    if (demoSteps <= 0) {
      currentPattern++;
      if (currentPattern >= PATTERN_COUNT) {currentPattern = 0;}
      startDemoPattern();
    }
  }
}

// Start screen
void showStartScreen() {

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("PLAY MUSIC!");

  lcd.createChar(0, name0x7);
  lcd.createChar(1, name1x7);
  lcd.createChar(2, name1x8);
  lcd.createChar(3, name0x8);


  lcd.setCursor(12, 0);
  lcd.write(byte(0));

  lcd.setCursor(12, 1);
  lcd.write(byte(1));

  lcd.setCursor(13, 0);
  lcd.write(byte(3));

  lcd.setCursor(13, 1);
  lcd.write(byte(2));
}

void setup() {

  pinMode(buzzer, OUTPUT);

  lcd.init();
  lcd.backlight();

  showStartScreen();
  lastKeyTime = millis();
}

void loop() {

  keypad.getKeys();

  bool anyKeyPressed = false;

  for (byte i = 0; i < LIST_MAX; i++) {

    if (!keypad.key[i].kchar) continue;

    char key = keypad.key[i].kchar;
    // pressed
    if (keypad.key[i].stateChanged && keypad.key[i].kstate == PRESSED) {
      anyKeyPressed = true; lastKeyTime = millis(); if (demoMode) {stopDemo();}

      int frequency = getFrequency(key); tone(buzzer,frequency);
      showPressedPattern(key); notePressed = true;
    }
    // held
    if (keypad.key[i].kstate == HOLD) {anyKeyPressed = true; lastKeyTime = millis();}
    // released
    if (keypad.key[i].stateChanged && keypad.key[i].kstate == RELEASED) {
      noTone(buzzer); notePressed = false;
      lcd.clear(); lastKeyTime = millis();
    }
  }
  // demo mode after 5 seconds
  if (!notePressed && !demoMode && millis() - lastKeyTime >= DEMO_TIMEOUT) {currentPattern = 0; startDemoPattern();}
  updateDemo();
}
