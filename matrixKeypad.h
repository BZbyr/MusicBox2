// http://playground.arduino.cc/code/Keypad
#define use4x4Keypad //comment this using 3x4 keypad
#ifdef use4x4Keypad
  ////////////////////////////////////////////////////////////////////////////////
  // define 4x4 keyboard
  ////////////////////////////////////////////////////////////////////////////////
  const byte ROWS = 4;  // Four rows
  const byte COLS = 4;  // Four columns
  //Define the keymap
  char keys[ROWS][COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
  };
  byte rowPins[ROWS] = {2 , 3, 4, 5}; //connect to row pinouts
  byte colPins[COLS] = {6 , 7, 8, 9}; //connect to column pinouts
#else
  ////////////////////////////////////////////////////////////////////////////////
  // define 3x4 keyboard
  ////////////////////////////////////////////////////////////////////////////////
  const byte ROWS = 4; //four rows
  const byte COLS = 3; //three columns
  char keys[ROWS][COLS] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
  };
  byte rowPins[ROWS] = {9,8,7,6}; //connect to the row pinouts of the keypad
  byte colPins[COLS] = {5,4,3}; //connect to the column pinouts of the keypad
#endif

//// Create the Keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );