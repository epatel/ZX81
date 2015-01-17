// ZX81 USB Keyboard for Leonardo
// (c) Dave Curran
// 2013-04-27

// Modified with Function keys by Tony Smith
// 2014-02-15

// Modified with Media keys and for Swedish layout by Edward Patel
// 2015-01-17

// New HID stuff after http://stefanjones.ca/blog/arduino-leonardo-remote-multimedia-keys/
#include "newUSBAPI.h"

#define NUM_ROWS 8
#define NUM_COLS 5

#define SHIFT_COL 4
#define SHIFT_ROW 5

#define DEBOUNCE_VALUE 250
#define REPEAT_DELAY 800

#define ALT_KEY_ON 255
#define ALT_KEY_OFF 0

#define MOD_ALT   0x01
#define MOD_SHIFT 0x02

// Keymap for normal use
byte keyMap[NUM_ROWS][NUM_COLS] = {
  {'5', '4', '3', '2', '1'},
  {'t', 'r', 'e', 'w', 'q'},
  {'6', '7', '8', '9', '0'},
  {'g', 'f', 'd', 's', 'a'},
  {'y', 'u', 'i', 'o', 'p'},
  {'v', 'c', 'x', 'z', 0},
  {'h', 'j', 'k', 'l', KEY_RETURN},
  {'b', 'n', 'm', '.', ' '}
};

// Keymap if Shift is pressed
byte keyMapShifted[NUM_ROWS][NUM_COLS] = {
  {KEY_LEFT_ARROW, 0, 0, 0, KEY_F4},
  {'T', 'R', 'E', 'W', 'Q'},
  {KEY_DOWN_ARROW, KEY_UP_ARROW, KEY_RIGHT_ARROW, 0, KEY_BACKSPACE},
  {'G', 'F', 'D', 'S', 'A'},
  {'Y', 'U', 'I', 'O', 'P'},
  {'V', 'C', 'X', 'Z', 0},
  {'H', 'J', 'K', 'L', KEY_F5},
  {'B', 'N', 'M', ',', '£'} 
};

// Keymap if Function-Shift pressed
// NEXT key read should be from this table
byte keyMapAlt[NUM_ROWS][NUM_COLS] = {
  {KEY_LEFT_ARROW, '}', '{', ']', '['},
  {'^', '\\', '_', 0, '"'},
  {KEY_DOWN_ARROW, KEY_UP_ARROW, KEY_RIGHT_ARROW, '!', KEY_BACKSPACE},
  {0, 0, '~', '|', '@'},
  {'%', '$', '(', ')', '"'},
  {'/', '?', ';', ':', 0},
  {'*', '-', '+', '=', KEY_RETURN},
  {'*', '<', '>', '\'', '#'}
};

struct s_mapping {
  byte character;
  byte sendByte;
  byte modifiers;
} mappings[] = {
  { '[',  '8',  1 },
  { ']',  '9',  1 },
  { '{',  '8',  3 },
  { '}',  '9',  3 },
  { '\\', '7',  3 },
  { '|',  '7',  1 },
  { '/',  '7',  2 },
  { '£',  '3',  1 },
  { '@',  '2',  1 },
  { '<',  '`',  0 },
  { '>',  '~',  0 },
  { '^',  ']',  2 },
  { '_',  '/',  2 },
  { '"',  '2',  2 },
  { '$',  '4',  1 },
  { '(',  '8',  2 },
  { ')',  '9',  2 },
  { '=',  '0',  2 },
  { '~',  ']',  1 },
  { '?',  '_',  2 },
  { '\'', '\\', 0 },
  { '-',  '/',  0 },
  { ';',  '<',  0 },
  { ':',  '>',  0 },
  { '*',  '|',  0 },
  { '&',  '6',  2 },
};

byte num_mappings = sizeof(mappings)/sizeof(s_mapping);

// Global Variables
int debounceCount[NUM_ROWS][NUM_COLS];
int altKeyFlag;

// Define the row and column pins
byte colPins[NUM_COLS] = {13, 12, 11, 10, 9};
byte rowPins[NUM_ROWS] = {7, 6, 5, 4, 3, 2, 1, 0};

void setup()
{
  // Set all pins as inputs and activate pull-ups
  for (byte c=0; c < NUM_COLS; c++) {
    pinMode(colPins[c], INPUT);
    digitalWrite(colPins[c], HIGH);
    
    // Clear debounce counts    
    for (byte r=0; r < NUM_ROWS; r++) {
      debounceCount[r][c] = 0;
    }
  }
  
  // Set all pins as inputs  
  for (byte r=0; r < NUM_ROWS; r++) {
    pinMode(rowPins[r], INPUT);
  }
  
  // Function key is NOT pressed
  altKeyFlag = ALT_KEY_OFF;
  
  // Initialise the keyboard  
  Keyboard.begin();
  Remote.begin();
}

void loop()
{
  bool shifted = false;
  bool keyPressed = false;
  
  // Check for the Shift key being pressed
  pinMode(rowPins[SHIFT_ROW], OUTPUT);
  
  if (digitalRead(colPins[SHIFT_COL]) == LOW) {
    shifted = true;
  }
  
  if (shifted == true && altKeyFlag == ALT_KEY_ON) {
    // NOP to prevent Function selection from autorepeating
  } else {
    pinMode(rowPins[SHIFT_ROW], INPUT);
    
    for (byte r=0; r < NUM_ROWS; r++) {
      // Run through the rows, turn them on
      
      pinMode(rowPins[r], OUTPUT);
      digitalWrite(rowPins[r], LOW);
      
      for (byte c=0; c < NUM_COLS; c++) { 
        if (digitalRead(colPins[c]) == LOW) {
          // Increase the debounce count          
          debounceCount[r][c]++;
          
          // Has the switch been pressed continually for long enough?          
          int count = debounceCount[r][c];
          if (count == DEBOUNCE_VALUE) {
            // First press            
            keyPressed = true;
            pressKey(r, c, shifted);
          } else if (count > DEBOUNCE_VALUE) {
            // Check for repeats
            count -= DEBOUNCE_VALUE;
            if (count % REPEAT_DELAY == 0) {
              // Send repeat
              keyPressed = true;
              pressKey(r, c, shifted);
            }
          }
        } else {
          // Not pressed; reset debounce count
          debounceCount[r][c] = 0;
        }
      }
     
      // Turn the row back off     
      pinMode(rowPins[r], INPUT);
    }
    
    digitalWrite(rowPins[SHIFT_ROW], LOW);
  }
}

void sendByte(byte key) {
  byte sendByte = key;
  byte modifiers = 0;
  if ((key < '0' || key > '9') &&
      (key < 'a' || key > 'z') &&
      (key < 'A' || key > 'Z')) {
    for (byte i=0; i<num_mappings; i++) {
      if (key == mappings[i].character) {
        sendByte = mappings[i].sendByte;
        modifiers = mappings[i].modifiers;
      }
    }
  }  
  if (modifiers & MOD_ALT)
    Keyboard.press(KEY_LEFT_ALT);
  if (modifiers & MOD_SHIFT)
    Keyboard.press(KEY_LEFT_SHIFT);
  Keyboard.press(sendByte);
  Keyboard.releaseAll();
}

void sendString(char *str) {
  while (*str) {
    sendByte(*str);
    str++;
  }
}

void pressKey(byte r, byte c, bool shifted)
{
  static byte mode = 0;
  
  if (mode == 0) {
    // Send the keypress
    byte key = shifted ? keyMapShifted[r][c] : keyMap[r][c];
  
    if (altKeyFlag == ALT_KEY_ON) {
      // Get the Alt key pressed after Function has been selected
      key = keyMapAlt[r][c];
      altKeyFlag = ALT_KEY_OFF;
    } else if (key == KEY_F5) {
      // If the Function key pressed (Shift + New Line)
      altKeyFlag = ALT_KEY_ON;
      key = 0;
      debounceCount[r][c] = 0;
    }
    
    if (key == KEY_F4) {
      sendString("Player");
      mode = 1;
      key = 0;
      debounceCount[r][c] = 0;
    }
  
    if (key > 0) {
      // Send the key
      sendByte(key);
    }
  } else if (mode == 1) {
    byte key = keyMap[r][c];
    switch (key) {
    case '0':
      sendString("Keyboard");
      mode = 0;
      break;
    case 0:
      Remote.rewind();
      Remote.clear();
      break;
    case ' ':
      Remote.forward();
      Remote.clear();
      break;
    case 'q':
      Remote.increase();
      Remote.clear();
      break;
    case 'a':
      Remote.decrease();
      Remote.clear();
      break;
    default:
      Remote.play();
      Remote.clear();
      break;
    }
  }
}

