/*
 * Arduino DIY Code Lock
 *
 *    9,8,7,6 5,4,3,2
 *
 *   {'1','2','3','A'},
 *   {'4','5','6','B'},
 *   {'7','8','9','C'},
 *   {'*','0','#','D'}
 *
 * keypad columns = 9,8,7,6 
 * keypad rows = 5,4,3,2
 * door lock relay = A0(54) 
 * A1(55) A2(56) 暂无设置
 * mini_speaker = A3 (57)
 * HC-SR04,ECHO_PIN = A4(58),TRIG_PIN = A5(59)
 * LED = 13
 * Serial1	TX1(18),RX1(19)	开发板上的TX接元件上的RX，开发板上的RX接元件上上的TX，TX就是transmit发送，RX就是receive接收
 */
 
 
// http://forum.arduino.cc/index.php/topic,46900.0.html 
#define DEBUG //comment this line if you want to disable serial message
#ifdef DEBUG
	#define DEBUG_PRINT(x) Serial.print(x)
	#define DEBUG_PRINTLN(x) Serial.println(x)
#else
	#define DEBUG_PRINT(x)
	#define DEBUG_PRINTLN(x)
#endif
#include <Arduino.h>
#include <Wire.h>
#include <eeprom_string.h>      // http://playground.arduino.cc/Code/EepromUtil        
#include <LiquidCrystal_I2C.h>  
#include <Keypad.h>             // library for using matrix style keypads
#include "pitches.h"            // constant value for pitches
#include "matrixKeypad.h"       // define 4x4 or 3x4 matrix keypad 
#include <Time.h>
#include <SoftwareSerial.h>
#include <DFPlayer_Mini_Mp3.h>
int reset_settings_pin  = 16; //hold the switch in order to restore settings
int sw_state;

#define DOOR_LOCK_PIN 54           //connect relay to digital 54
#define LOCK_HOLD_TIME 5           //if door lock is released, lock it after 5 seconds
#define MINI_SPEAKER_PIN 57        //connect speaker to Analog 3 (digital 57)
#define TRIG_PIN 59				   //HC-SR04 trig pin A5(59)
#define ECHO_PIN 58				   //HC-SR04 echo pin A4(58)
#define LED 13
// set the LCD address to 0x27 for a 16 chars and 2 line display
//http://www.yfrobot.com/forum.php?mod=viewthread&tid=2263
// Uno, Ethernet  A4 (SDA), A5 (SCL)
// Mega2560	  20 (SDA), 21 (SCL)
// Leonardo	  2 (SDA), 3 (SCL)
// Due	          20 (SDA), 21 (SCL), SDA1, SCL1
LiquidCrystal_I2C lcd(0x27,16,2); 

// timeout variable
unsigned long previousMillis = 0;   //hold the current millis     
#define KEYBOARD_EVENT_TIMEOUT 10   //timeout (in second) for keyboard input

// Actions
#define ADMIN 0     //use to change ADMIN password
#define USER 1      //use to change USER password
byte action = USER; //action is set to USER now


////////////////////////////////////////////////////////////////////////////////
// password variable
////////////////////////////////////////////////////////////////////////////////
String userPassword;              //variable to hold USER password
String adminPassword;             //variable to hole ADMIN password
const byte maxPasswordLength = 4; //length of ADMIN & USER password, not more than 10

char buf[maxPasswordLength + 1];           //temporary storage for EEPROM data, hold the string read/write from EEPROM
#define USER_PASSWORD_EEPROM_ADDRESS 0     //EEPROM address to store USER password
#define ADMIN_PASSWORD_EEPROM_ADDRESS 10   //EEPROM address to store ADMIN password


////////////////////////////////////////////////////////////////////////////////
// variable for password retry
////////////////////////////////////////////////////////////////////////////////
const byte MAX_PASSWORD_RETRY = 3;        //Maximum number of retry
byte retryCount = 0;                      //number of retry, start from 0
boolean reached_maximum_retries = false;  //use to disable keyboard input
const byte AUTO_RESET_RETRY_COUNT = 1;    //use to reset (in minute) for password retry count


////////////////////////////////////////////////////////////////////////////////
// these code will be execute on each Arduino reset
////////////////////////////////////////////////////////////////////////////////
void setup(){
	Serial.begin (9600);
	Serial1.begin(9600);
	mp3_set_serial (Serial1);	//set Serial for DFPlayer-mini mp3 module 
	delay(1);  //wait 1ms for mp3 module to set volume
	mp3_set_volume (15);
	lcd.init();                        // initialize the lcd 
	lcd.backlight();                   // turn on LCD back light  
	pinMode(DOOR_LOCK_PIN, OUTPUT);    // set DOOR_LOCK_PIN as output
	digitalWrite(DOOR_LOCK_PIN, LOW);  // lock the door
	pinMode(ECHO_PIN,INPUT);		//receive sighal.
	pinMode(TRIG_PIN,OUTPUT);		//touch off issue sighal
	pinMode(LED, OUTPUT);
	//use to initialize EEPROM for first time use
	pinMode(reset_settings_pin, INPUT);          // Set the reset_settings_pin  as input
	digitalWrite(reset_settings_pin,LOW);      // turn on pullup resistor & set reset_settings_pin  to HIGH
	digitalWrite(LED, LOW);
	sw_state = digitalRead(reset_settings_pin);  // read input value from reset_settings_pin 
	if (sw_state == LOW) 
		reset_settings();     // proceed write configuration routine if reset_settings_pin =high
  
	loadConfiguration();                         // load settings from EEPROM
	Serial.println("Arduino DIY Code Lock");
	welcome();                                   // show welcome message
}


////////////////////////////////////////////////////////////////////////////////
// main loop
////////////////////////////////////////////////////////////////////////////////
void loop(){

	// retryCount increase by one each time an incorrect password has been entered
	// retryCount will reset to zero when idle for one minute (set by AUTO_RESET_RETRY_COUNT)
	if (retryCount > 0) { //if someone have entered wrong password
		if (millis() >= previousMillis + AUTO_RESET_RETRY_COUNT * 60000) { 
			resetRetryCount();
		}
	}

	if (!reached_maximum_retries) { // if did not reach maximum number of retry
		char key = keypad.getKey();   //waiting for keyboard input (non blocking)
		if (key != NO_KEY){           //if press a key
			beep();
			delay(30);                  //debounce
			if (key == '#')  
				enterPasswordForMenu();     //require to enter password before proceed to menu selection
			else 
				enterPasswordToRelease_DoorLock(key);   //otherwise proceed to enterPasswordToRelease_DoorLock routine
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// clear screen & show welcome message
////////////////////////////////////////////////////////////////////////////////
void welcome() {
	DEBUG_PRINTLN(); 
	DEBUG_PRINT("Please enter password. "); 
	DEBUG_PRINTLN(retryCount);
	lcd.clear(); 
	lcd.print("Enter password "); 
	lcd.print(retryCount);  
}

////////////////////////////////////////////////////////////////////////////////
// show date and time on lcd
////////////////////////////////////////////////////////////////////////////////
void print_date_time() {
  lcd.setCursor(0, 1);        //second line (x=0, y=1)

  if(day() < 10) 
  	lcd.print('0');
  lcd.print(day());
  lcd.print("-");

  if(month() < 10) 
  	lcd.print('0');
  lcd.print(month());
  lcd.print("   ");

  if(hour() < 10) 
  	lcd.print('0');
  lcd.print(hour());
  lcd.print(":");
  
  
  if(minute() < 10) 
  	lcd.print('0');  
  lcd.print(minute());  
  lcd.print(":");
  
  if(second() < 10) 
  	lcd.print('0');
  lcd.print(second());
}

////////////////////////////////////////////////////////////////////////////////
// clear one line from lcd
////////////////////////////////////////////////////////////////////////////////
void clear_lcd_line(byte y){
	y = y-1;
	lcd.setCursor(0, y); 
	lcd.print("                ");
	lcd.setCursor(0, y);
}


////////////////////////////////////////////////////////////////////////////////
// load settings from EEPROM
////////////////////////////////////////////////////////////////////////////////
void loadConfiguration() {
	eeprom_read_string(USER_PASSWORD_EEPROM_ADDRESS, buf, maxPasswordLength + 1);  //read USER password from EEPROM
    userPassword = buf;   //use this password to release door lock
	eeprom_read_string(ADMIN_PASSWORD_EEPROM_ADDRESS, buf, maxPasswordLength + 1); //read ADMIN password from EEPROM
    adminPassword = buf;  //use this password to enter menu selection
}
  
////////////////////////////////////////////////////////////////////////////////
// reset retry count & enable keyboard input
////////////////////////////////////////////////////////////////////////////////
void resetRetryCount() {
    retryCount = 0;                   //reset retry count
    reached_maximum_retries = false;  // enable keyboard input
    welcome();
}  


////////////////////////////////////////////////////////////////////////////////
// input password for menu
////////////////////////////////////////////////////////////////////////////////
void enterPasswordForMenu() {
	String inputPassword = "";
	lcd.clear();
	lcd.print("AEnterPassword ");
	lcd.print(retryCount);
     
	// get input from keyboard
	// only accept 0,1,2,3,4,5,6,7,8,9 key
	// an * is print to the second line of LCD when a key is pressed  
	inputPassword = getInput('0','9',maxPasswordLength,false);
	if (inputPassword.length() > 0) { 
	// if user had input the password
    // compare input password with ADMIN password
    // unlock the Door Lock if a correct password had been enter
    // otherwise output warning alert & increase retry by one
		if (isCorrectPassword(inputPassword, adminPassword)) {
			playOKTone();
			playWarningTone();
			menuSelection();  // let user make a selection
		} 
		else {
			processFailedPassword(); //something must be done when user enter wrong password
		}
	}
	if (retryCount < MAX_PASSWORD_RETRY) 
		welcome();  
}


////////////////////////////////////////////////////////////////////////////////
// let user make a selection
// valid key ranged from 1 to 5
////////////////////////////////////////////////////////////////////////////////
void menuSelection() {
	DEBUG_PRINTLN(); 
	DEBUG_PRINT("Selection (1-3): "); 
	lcd.clear(); 
	lcd.print("Selection (1-3)");   
  
	// get input from keyboard
	// only accept 1,2 key
	String selectedMenuString = getInput('1','3', 1,true); 
  
	if (selectedMenuString.length() > 0) { // if user made an selection
		if (selectedMenuString == "1") {     // user select 1
			action = USER;                     // indicated user want to change USER password 
			changePassword();                  // proceed with change password routine
		} 
		if (selectedMenuString == "2") {    // user select 2
			action = ADMIN;                   // indicated user want to change USER password 
			changePassword();                 // proceed with change password routine
		}
		if (selectedMenuString == "3") {    // user select 3
      		setDateTime(); 
    	}     
	}
	welcome();
}


////////////////////////////////////////////////////////////////////////////////
// input password to unlock DOOR LOCK
////////////////////////////////////////////////////////////////////////////////
void enterPasswordToRelease_DoorLock(char key) {
	DEBUG_PRINT("*");
	clear_lcd_line(2); //clear second line
	lcd.print("*");     
	// get input from keyboard
	// only accept 0,1,2,3,4,5,6,7,8,9 key
	// an * is print to the second line of LCD when a key is pressed  
	String inputPassword = getInput('0','9', maxPasswordLength-1,false);
	if (inputPassword.length() > 0) { // if user had input the password
		inputPassword = key + inputPassword;
    
		// compare input password with USER password
		// open the door if correct password had been enter
		// otherwise output warning alert & increase retry count by one
		if (isCorrectPassword(inputPassword, userPassword)) {
			//DEBUG_PRINTLN("Password accepted.");
			//lcd.clear();
			//lcd.print("Pwd accepted.");
			retryCount = 0;
			lockRelease(); 
		} 
		else {
			processFailedPassword();
		}
	}
	if (retryCount < MAX_PASSWORD_RETRY) 
		welcome();  
}


////////////////////////////////////////////////////////////////////////////////
// incorrect password had been enter
////////////////////////////////////////////////////////////////////////////////
void processFailedPassword() {
	playWarningTone();
	retryCount = retryCount + 1;
	if (retryCount == MAX_PASSWORD_RETRY) { // reached maximum retries
		DEBUG_PRINTLN(); 
		DEBUG_PRINT("Reached maximum retries, wait ");
		DEBUG_PRINT(AUTO_RESET_RETRY_COUNT); 
		DEBUG_PRINTLN(" minute.");
		lcd.clear(); 
		lcd.print("Wait "); 
		lcd.print(AUTO_RESET_RETRY_COUNT); 
		lcd.print(" minute.");
		reached_maximum_retries = true;     //cause main loop disable keyboard input
	}	  
}


////////////////////////////////////////////////////////////////////////////////
// a sub routine for keyboard input with timeout
// timeout occured if user did not press any key within certain amount time (set by KEYBOARD_EVENT_TIMEOUT)
// lowKey & highKey limit input range from lowKey to highKey
// maxStringLength control how many alphanumeric will be enter
// show input key when showKey=true, otherwise show "*"
////////////////////////////////////////////////////////////////////////////////
String getInput(char lowKey, char highKey, byte maxStringLength,boolean showKey) {
	boolean inputIsValid = false; // indicate valid password had been enter
  	String inputString = "";
  	byte inputStringLength = 0;
  	previousMillis = millis(); // use by timeout

  	while (!isTimeout()) {         // loop until timeout
    	char key = keypad.getKey();  // waiting for keyboard input
    	if (key != NO_KEY){          // if a key is pressed
    		previousMillis = millis(); // reset timeout so that the timeout will not occur
      		beep();                    
      		if (key == '*') 
      			break;     // exit loop if user press a * key
      		if (key >= lowKey && key <= highKey) { // user press a key which is acceptable
        		if (showKey) {
          			DEBUG_PRINT(key);
          			lcd.print(key);
        		} else {  
          			DEBUG_PRINT("*");
          			lcd.print("*"); 
        		}
        		inputStringLength++; 
        		inputString = inputString + key;
        		if (inputString.length() >= maxStringLength) { // if input reach maxStringLength
          			inputIsValid = true;                         // indicated user had enter string (eg. password) 
          			break;                                       // and then exit the loop
        		}  
      		} else 
      			playWarningTone();                            // alert user if unacceptable key is pressed
    	}
  	}
  	if (!inputIsValid) 
  		inputString = "";                 // if did not enter any input, return blank
  	return inputString;                                  // otherwise return inputString
}



////////////////////////////////////////////////////////////////////////////////
// compare input password with USER/ADMIN password
////////////////////////////////////////////////////////////////////////////////
boolean isCorrectPassword(String inputPassword, String password) {
	if (inputPassword == password) return true; else return false; 
}


////////////////////////////////////////////////////////////////////////////////
// send LOW to DOOR_LOCK_PIN in order to release the Door Lock
// lock the door after certain amount of time (defined by LOCK_HOLD_TIME)
////////////////////////////////////////////////////////////////////////////////
void lockRelease() {
		
	DEBUG_PRINTLN("Lock released.");
	lcd.clear(); 
	lcd.print("Lock released.");   
	digitalWrite(DOOR_LOCK_PIN, HIGH);  // release door lock
	digitalWrite(LED, HIGH);
	playOKTone();
	delay(30);
	mp3_loop(1);
	
	while (true){ 
		print_date_time();
		if(keypad.getKeys()==false){
			if(echo() == false){
	    		mp3_loop(0);
	    		break;// Box off the sound off
	    	}
		}
		if(keypad.getKeys()==true){
			char key = keypad.getKey();
			if(key == 'A')
				beep();
				delay(30);
				mp3_prev ();
			if(key == 'B')
				beep();
				delay(30);
				mp3_next ();
			if(key == 'C')
			    beep();
				delay(30);
				mp3_increase_volume();
			if(key == 'D')
				beep();
				delay(30);
				mp3_decrease_volume();
			if(echo() == false){
	    		mp3_loop(0);
	    		break;// Box off the sound off
	    	}
		}
	}

	// timer0_overflow_count = 0; //reset timer0 in order to prevent timer overflow after 49 days
  
	digitalWrite(DOOR_LOCK_PIN, LOW); // lock the door 
	digitalWrite(LED, LOW); 
	playTimeOutTone();
}


////////////////////////////////////////////////////////////////////////////////
// change USER password or ADMIN password
////////////////////////////////////////////////////////////////////////////////
void changePassword() {
	if (action == ADMIN) {
		DEBUG_PRINTLN(); 
		DEBUG_PRINT("Admin Password: ");
		lcd.clear(); 
		lcd.print("Admin Password");
	} 
	else {
		DEBUG_PRINTLN(); 
		DEBUG_PRINT("User Password: ");
		lcd.clear(); 
		lcd.print("User Password");    
	}
  
	clear_lcd_line(2); //clear second line
	String newPassword = getInput('0','9', maxPasswordLength,false); // waiting for new password
	if (newPassword.length() > 0) { // if new password had been enter
		DEBUG_PRINTLN(); 
		DEBUG_PRINT("Confirm Password: ");
		lcd.clear(); 
		lcd.print("Confirm Password");
		clear_lcd_line(2); //clear second line
		String confirmPassword = getInput('0','9', maxPasswordLength,false); // waiting for confirm password
		if (confirmPassword.length() > 0 && newPassword == confirmPassword) { 
			updatePassword(newPassword); // update old password with new password
			playOKTone();
		}
	}
	welcome();    
}


////////////////////////////////////////////////////////////////////////////////
// update old password with new password
////////////////////////////////////////////////////////////////////////////////
void updatePassword(String newPassword){
	char newPasswordChar[maxPasswordLength + 1]; 
	newPassword.toCharArray(newPasswordChar, maxPasswordLength+1); //convert string to char array
	strcpy(buf, newPasswordChar); 
	if (action == ADMIN) {
		adminPassword = newPassword; 
		eeprom_write_string(ADMIN_PASSWORD_EEPROM_ADDRESS, buf);
		DEBUG_PRINTLN(); 
		DEBUG_PRINT("ADMIN password changed to ");
	}  
	else {
		userPassword = newPassword;
		eeprom_write_string(USER_PASSWORD_EEPROM_ADDRESS, buf);
		DEBUG_PRINTLN(); 
		DEBUG_PRINT("USER password changed to ");    
	}  
  
	DEBUG_PRINTLN(newPassword);
	lcd.clear(); 
	lcd.print("Password changed");
	delay(1000);
	welcome();
}
////////////////////////////////////////////////////////////////////////////////
// set date and time
////////////////////////////////////////////////////////////////////////////////
void setDateTime() {
  	byte hh, mm, ss, dd, mo, yy;
  	DEBUG_PRINTLN(); DEBUG_PRINT("Date (ddmmyyyy): "); // The full four digit year (eg. 2013)
  	lcd.clear(); lcd.print("Date (ddmmyyyy):");
  	lcd.cursor(); // show cursor
  	lcd.setCursor(0,1); //second line
  	if(day() < 10) lcd.print('0'); 
  		lcd.print(day()); 
  	if(month() < 10) lcd.print('0'); 
  		lcd.print(month()); 
  	if(year() < 10) lcd.print('0'); 
  		lcd.print(year()); // show current date
  		lcd.setCursor(0,1); //second line
  	String newDate = getInput('0','9', 8, true); // date input
  	dd = (newDate.substring(0, 2)).toInt(); // get frist two lettter from newDate and convert it to integer 
  	mo = (newDate.substring(2, 4)).toInt();
  	yy = (newDate.substring(4, 8)).toInt();
  	if (isValidDate(dd, mo, yy)) { // valid date has been enter
    	DEBUG_PRINT("Time (hhmmss): ");
    	lcd.clear(); lcd.print("Time (hhmmss): ");
    	lcd.setCursor(0,1); //second line
    	if(hour() < 10) lcd.print('0'); 
    		lcd.print(hour()); 
    	if(minute() < 10) lcd.print('0'); 
    		lcd.print(minute()); 
    	if(second() < 10) lcd.print('0'); 
    		lcd.print(second()); // show current time 
    		lcd.setCursor(0,1); //second line
    		String newTime = getInput('0','9', 6, true); // time input
    		hh = (newTime.substring(0, 2)).toInt();
    		mm = (newTime.substring(2, 4)).toInt();
    		ss = (newTime.substring(4, 6)).toInt();
    	if (isValidTime(hh, mm, ss)) {
      		setTime(hh,mm,ss,dd,mo,yy); // set date time  
      		playOKTone();
    	} else {
     	 	DEBUG_PRINT("Invalid Time");
      		lcd.clear(); lcd.print("Invalid Time");
      		playWarningTone();      
      		delay(1000);
    	}
  	} else { // invalid date
    	DEBUG_PRINT("Invalid Date");
    	lcd.clear(); lcd.print("Invalid Date");
    	playWarningTone();
    	delay(1000);
  	}  
  	lcd.noCursor(); // hide cursor
}

boolean isValidDate(byte dd, byte mo, byte yy) {
  	boolean result = true;
  	if (dd <1 || dd>31) 
  		result = false;
  	if (mo <1 || mo>12) 
  		result = false;
  	//if (yy <1970 || yy>2099) result = false;
  	return result;
}

boolean isValidTime(byte hh, byte mm, byte ss) {
  	boolean result = true;
  	if (hh <1 || hh>23) 
  		result = false;
  	if (mm <1 || mm>59) 
  		result = false;
  	if (ss <1 || ss>59) 
  		result = false;
  	return result;
}

////////////////////////////////////////////////////////////////////////////////
// monitoring a key is pressed
// return true when idle for 10 seconds (defined by KEYBOARD_EVENT_TIMEOUT)
////////////////////////////////////////////////////////////////////////////////
boolean isTimeout() {  
	if (millis() >= previousMillis + KEYBOARD_EVENT_TIMEOUT *1000){ //convert KEYBOARD_EVENT_TIMEOUT in milli second to second
		playTimeOutTone();
		return true; 
	} 
	else 
		return false; 
}


////////////////////////////////////////////////////////////////////////////////
// reset settings, write configuration to EEPROM
/// press and hold button for 5 seconds (defined by HOLD_DELAY) to activate reset settings
////////////////////////////////////////////////////////////////////////////////
void reset_settings() {
	unsigned long start_hold = millis();                 // mark the time
	int HOLD_DELAY = 5000;    // Sets the hold delay 
	DEBUG_PRINTLN("Please keep on pressing for 5 sconds.");
	while (sw_state == LOW) {
		sw_state = digitalRead(reset_settings_pin );      // read input value
		if ((millis() - start_hold) >= HOLD_DELAY){      // for longer than HOLD_DELAY
		//initialize_is_running = true;  // keep loop running even though reset_settings_pin  is low
			initializeEEPROM();
			break; //break the loop after initialized
		}
	}
}


////////////////////////////////////////////////////////////////////////////////
// initialize EEPROM (write configuration)
// syntax: eeprom_write_string(int addr, const char* string)
// addr: the EEPROM address of Arduino, ATMega328 = 0 to 1023 (1024 Byte), ATmega168 = 512 Bytes
////////////////////////////////////////////////////////////////////////////////
void initializeEEPROM(){
	strcpy(buf, "2345"); //set password to 2345
	eeprom_write_string(USER_PASSWORD_EEPROM_ADDRESS, buf);  //write password to EEPROM
	strcpy(buf, "1234"); //set password to 1234
	eeprom_write_string(ADMIN_PASSWORD_EEPROM_ADDRESS, buf); //write password to EEPROM
	DEBUG_PRINTLN("Initialize completed");
	lcd.clear(); 
	lcd.print("Initialized.");
	delay(1000);
	playOKTone();
}
  
  
////////////////////////////////////////////////////////////////////////////////
// making sound to speaker, tone() function uses timer2
// syntax: tone(pin, frequency, duration)
// pin: the pin on which to generate the tone
// frequency: the frequency of the tone in hertz - unsigned int
// duration: the duration of the tone in milliseconds (optional) - unsigned long
////////////////////////////////////////////////////////////////////////////////
void beep(){
	tone(MINI_SPEAKER_PIN,NOTE_C7,90); 
	delay(20);
	noTone(MINI_SPEAKER_PIN);
}


void playTone(int note1, int note2, int note3, byte delayTime=100){
	tone(MINI_SPEAKER_PIN, note1, 90); 
	delay(delayTime);

	tone(MINI_SPEAKER_PIN, note2, 90);
	delay(delayTime);
 
	tone(MINI_SPEAKER_PIN, note3, 190);
	delay(delayTime);
	noTone(MINI_SPEAKER_PIN);
}


void playOKTone(){
	playTone(NOTE_C5, NOTE_D5, NOTE_E5);     //playing Do-Re-Mi
}  


void playTimeOutTone(){
	playTone(NOTE_E5, NOTE_D5, NOTE_C5);     //playing Mi-Re-Do
}


void playWarningTone(){
	playTone(NOTE_E5, NOTE_E5, NOTE_E5, 150); //Plaing Mi-Mi-Mi
}

boolean echo(){
	digitalWrite(TRIG_PIN, LOW); // To issue a low level 2 s of the ultrasonic signal interface
	delayMicroseconds(2);
	digitalWrite(TRIG_PIN, HIGH); // To issue a high level 10 s, which is at least 10 s.
	delayMicroseconds(10);
	digitalWrite(TRIG_PIN, LOW); // To maintain a low level signal interface
	int distance = pulseIn(ECHO_PIN, HIGH); // Readout pulse time
	distance= distance/58; // The pulse time is converted to distance (unit: cm)
	Serial.println(distance); //Output distance
	delay(70);
	if (distance >=3){//If the distance is greater than 3 cm lights up
		Serial.println(">3");
		return true;
	}else{
		Serial.println("<3");
		return false;
	}
}

