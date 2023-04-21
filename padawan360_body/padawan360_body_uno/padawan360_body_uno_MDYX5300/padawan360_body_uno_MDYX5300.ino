//************************** Set speed and turn speeds here************************************//

//set these 3 to whatever speeds work for you. 0-stop, 127-full speed.
const byte DRIVESPEED1 = 50;
//Recommend beginner: 50 to 75, experienced: 100 to 127, I like 100.
const byte DRIVESPEED2 = 100;
//Set to 0 if you only want 2 speeds.
const byte DRIVESPEED3 = 127;

byte drivespeed = DRIVESPEED1;

// the higher this number the faster the droid will spin in place, lower - easier to control.
// Recommend beginner: 40 to 50, experienced: 50 $ up, I like 70
const byte TURNSPEED = 70;

// Set isLeftStickDrive to true for driving  with the left stick
// Set isLeftStickDrive to false for driving with the right stick (legacy and original configuration)
boolean isLeftStickDrive = true; 

// If using a speed controller for the dome, sets the top speed. You'll want to vary it potenitally
// depending on your motor. My Pittman is really fast so I dial this down a ways from top speed.
// Use a number up to 127 for serial
const byte DOMESPEED = 80;

// Ramping- the lower this number the longer R2 will take to speedup or slow down,
// change this by incriments of 1
const byte RAMPING = 5;

// Compensation is for deadband/deadzone checking. There's a little play in the neutral zone
// which gets a reading of a value of something other than 0 when you're not moving the stick.
// It may vary a bit across controllers and how broken in they are, sometimex 360 controllers
// develop a little bit of play in the stick at the center position. You can do this with the
// direct method calls against the Syren/Sabertooth library itself but it's not supported in all
// serial modes so just manage and check it in software here
// use the lowest number with no drift
// DOMEDEADZONERANGE for the left stick, DRIVEDEADZONERANGE for the right stick
const byte DOMEDEADZONERANGE = 20;
const byte DRIVEDEADZONERANGE = 20;

// Set the baude rate for the Syren motor controller
// for packetized options are: 2400, 9600, 19200 and 38400. I think you need to pick one that works
// and I think it varies across different firmware versions.
// for simple serial use 9600
const int DOMEBAUDRATE = 9600;

// Comment the SYRENSIMPLE out for packetized serial connection to Syren - Recomended.
// I've never tested Syrene Simple, it's a carry-over from DanF's library.
// Un-comment for simple serial - do not use in close contact with people
//#define SYRENSIMPLE

#define EXTINGUISHERPIN 3

#include <Sabertooth.h>
#include <SyRenSimplified.h>
#include <Servo.h>
#include <MD_YX5300.h>
#include <XBOXRECV.h>
#include <SoftwareSerial.h>

// Connections for serial interface to the YX5300 module
#define MP3Stream Serial  // Native serial port - change to suit the application

// Define global variables
MD_YX5300 mp3(MP3Stream);

// These are the pins for the Sabertooth and Syren
SoftwareSerial STSerial(NOT_A_PIN, 4);
SoftwareSerial SyRSerial(2, 5);

/////////////////////////////////////////////////////////////////
Sabertooth ST(128, STSerial);
#if defined(SYRENSIMPLE)
SyRenSimplified SyR(SyRSerial); // Use SWSerial as the serial port.
#else
Sabertooth SyR(128, SyRSerial);
#endif

// Satisfy IDE, which only needs to see the include statment in the ino.
#ifdef dobogusinclude
#include <spi4teensy3.h>
#endif

// Set some defaults for start up
// 0 = full volume, 255 off
int vol = 14;

// 0 = drive motors off ( right stick disabled ) at start
boolean isDriveEnabled = false;

USB Usb;
XBOXRECV Xbox(&Usb);

// read the switboolean isDriveEnabled = false;

// Automated function variables
// Used as a boolean to turn on/off automated functions like periodic random sounds and periodic dome turns
boolean isInAutomationMode = false;
unsigned long automateMillis = 0;
byte automateDelay = random(5,20);// set this to min and max seconds between sounds
//How much the dome may turn during automation.
int turnDirection = 20;
// Action number used to randomly choose a sound effect or a dome turn
byte automateAction = 0;

int driveThrottle = 0;
int throttleStickValue = 0;
int domeThrottle = 0;
int turnThrottle = 0;

boolean firstLoadOnConnect = false;

AnalogHatEnum throttleAxis;
AnalogHatEnum turnAxis;
AnalogHatEnum domeAxis;
ButtonEnum speedSelectButton;
ButtonEnum hpLightToggleButton;


boolean isHPOn = false;

void setup(){
  MP3Stream.begin(MD_YX5300::SERIAL_BPS);
  mp3.begin();
  // Serial.begin(115200);
  mp3.volume(vol);
  SyRSerial.begin(DOMEBAUDRATE);
  #if defined(SYRENSIMPLE)
    SyR.motor(0);
  #else
    SyR.autobaud();
  #endif

  // 9600 is the default baud rate for Sabertooth packet serial.
  STSerial.begin(9600);
  // Send the autobaud command to the Sabertooth controller(s).
  ST.autobaud();
  /* NOTE: *Not all* Sabertooth controllers need this command.
  It doesn't hurt anything, but V2 controllers use an
  EEPROM setting (changeable with the function setBaudRate) to set
  the baud rate instead of detecting with autobaud.
  If you have a 2x12, 2x25 V2, 2x60 or SyRen 50, you can remove
  the autobaud line and save yourself two seconds of startup delay.
  */

  ST.setTimeout(950);
  #if !defined(SYRENSIMPLE)
    SyR.setTimeout(950);
  #endif

  // The Sabertooth won't act on mixed mode packet serial commands until
  // it has received power levels for BOTH throttle and turning, since it
  // mixes the two together to get diff-drive power levels for both motors.
  ST.drive(0);
  ST.turn(0);

  pinMode(EXTINGUISHERPIN, OUTPUT);
  digitalWrite(EXTINGUISHERPIN, HIGH);

  if(isLeftStickDrive) {
    throttleAxis = LeftHatY;
    turnAxis = LeftHatX;
    domeAxis = RightHatX;
    speedSelectButton = L3;
    hpLightToggleButton = R3;

  } else {
    throttleAxis = RightHatY;
    turnAxis = RightHatX;
    domeAxis = LeftHatX;
    speedSelectButton = R3;
    hpLightToggleButton = L3;
  }


//   Serial.begin(115200);
  // Wait for serial port to connect - used on Leonardo, Teensy and other boards with built-in USB CDC serial connection
  while (!Serial);
    if (Usb.Init() == -1) {
      //Serial.print(F("\r\nOSC did not start"));
      while (1); //halt
    }
  Serial.print(F("\r\nXbox Wireless Receiver Library Started"));
}


void loop(){
  Usb.Task();
  mp3.getStatus();
  // if we're not connected, return so we don't bother doing anything else.
  // set all movement to 0 so if we lose connection we don't have a runaway droid!
  // a restraining bolt and jawa droid caller won't save us here!
  if(!Xbox.XboxReceiverConnected || !Xbox.Xbox360Connected[0]){
    ST.drive(0);
    ST.turn(0);
    SyR.motor(1,0);
    firstLoadOnConnect = false;
    return;
  }

  // After the controller connects, Blink all the LEDs so we know drives are disengaged at start
  if(!firstLoadOnConnect){
    firstLoadOnConnect = true;
    mp3.playTrack(21);
    Xbox.setLedMode(ROTATING, 0);
  }
  
  if (Xbox.getButtonClick(XBOX, 0)) {
    if(Xbox.getButtonPress(L1, 0) && Xbox.getButtonPress(R1, 0)){ 
      Xbox.disconnect(0);
    }
  }

  // enable / disable right stick (droid movement) & play a sound to signal motor state
  if(Xbox.getButtonClick(START, 0)) {
    if(isDriveEnabled){
      isDriveEnabled = false;
      Xbox.setLedMode(ROTATING, 0);
      mp3.playTrack(53);
    } else {
      isDriveEnabled = true;
      mp3.playTrack(52);
      // //When the drive is enabled, set our LED accordingly to indicate speed
      if(drivespeed == DRIVESPEED1){
        Xbox.setLedOn(LED1, 0);
      } else if(drivespeed == DRIVESPEED2 && (DRIVESPEED3!=0)){
        Xbox.setLedOn(LED2, 0);
      } else {
        Xbox.setLedOn(LED3, 0);
      }
    }
  }

  //Toggle automation mode with the BACK button
  if(Xbox.getButtonClick(BACK, 0)) {
    if(isInAutomationMode){
      isInAutomationMode = false;
      automateAction = 0;
      mp3.playTrack(53);
    } else {
      isInAutomationMode = true;
      mp3.playTrack(52);
    }
  }

  // Plays random sounds or dome movements for automations when in automation mode
  if(isInAutomationMode){
    unsigned long currentMillis = millis();

    if(currentMillis - automateMillis > (automateDelay*1000)){
      automateMillis = millis();
      automateAction = random(1,5);

      if(automateAction > 1){
        mp3.playTrack(random(32,52));
      }
      if(automateAction < 4){
        #if defined(SYRENSIMPLE)
          SyR.motor(turnDirection);
        #else
          SyR.motor(1,turnDirection);
        #endif

        delay(750);

        #if defined(SYRENSIMPLE)
          SyR.motor(0);
        #else
          SyR.motor(1,0);
        #endif

        if(turnDirection > 0){
          turnDirection = -45;
        } else {
          turnDirection = 45;
        }
      }

      // sets the mix, max seconds between automation actions - sounds and dome movement
      automateDelay = random(5,20);
    }
  }

  // Volume Control of MP3 Trigger
  // Hold R1 and Press Up/down on D-pad to increase/decrease volume
  if(Xbox.getButtonClick(UP, 0)){
    // volume up
    if(Xbox.getButtonPress(R1, 0)){
      if (vol < 30){
        vol = vol + 2;
        mp3.volume(vol);
      }
    }
  }
  if(Xbox.getButtonClick(DOWN, 0)){
    //volume down
    if(Xbox.getButtonPress(R1, 0)){
      if (vol > 0){
        vol = vol - 2;
        mp3.volume(vol);
      }
    }
  }

  // Logic display brightness.
  // Hold L1 and press up/down on dpad to increase/decrease brightness
  /*
  if(Xbox.getButtonClick(UP, 0)){
    if(Xbox.getButtonPress(L1, 0)){
    }
  }
  if(Xbox.getButtonClick(DOWN, 0)){
    if(Xbox.getButtonPress(L1, 0)){
    }
  }
  */


  //FIRE EXTINGUISHER
  // When holding L2-UP, extinguisher is spraying. WHen released, stop spraying

  // TODO: ADD SERVO DOOR OPEN FIRST. ONLY ALLOW EXTINGUISHER ONCE IT'S SET TO 'OPENED'
  // THEN CLOSE THE SERVO DOOR
  if(Xbox.getButtonPress(L1, 0)){
    if(Xbox.getButtonPress(UP, 0)){
      digitalWrite(EXTINGUISHERPIN, LOW);
    } else {
      digitalWrite(EXTINGUISHERPIN, HIGH);
    }
  }


  // GENERAL SOUND PLAYBACK AND DISPLAY CHANGING

  // Y Button and Y combo buttons
  if(Xbox.getButtonClick(Y, 0)){
    if(Xbox.getButtonPress(L1, 0)){
      mp3.playTrack(8);
    } else if(Xbox.getButtonPress(L2, 0)){
      mp3.playTrack(2);
    } else if(Xbox.getButtonPress(R1, 0)){
      mp3.playTrack(9);
    } else {
      mp3.playTrack(random(13,17));
    }
  }

  // A Button and A combo Buttons
  if(Xbox.getButtonClick(A, 0)){
    if(Xbox.getButtonPress(L1, 0)){
      mp3.playTrack(6);
    } else if(Xbox.getButtonPress(L2, 0)){
      mp3.playTrack(1);
    } else if(Xbox.getButtonPress(R1, 0)){
      mp3.playTrack(11);
    } else {
      mp3.playTrack(random(17,25));
    }
  }

  // B Button and B combo Buttons
  if(Xbox.getButtonClick(B, 0)){
    if(Xbox.getButtonPress(L1, 0)){
      mp3.playTrack(7);
    } else if(Xbox.getButtonPress(L2, 0)){
      mp3.playTrack(3);
    } else if(Xbox.getButtonPress(R1, 0)){
      mp3.playTrack(10);
    } else {
      mp3.playTrack(random(32,52));
    }
  }

  // X Button and X combo Buttons
  if(Xbox.getButtonClick(X, 0)){
    // leia message L1+X
    if(Xbox.getButtonPress(L1, 0)){
      mp3.playTrack(5);
    } else if(Xbox.getButtonPress(L2, 0)){
      mp3.playTrack(4);
    } else if(Xbox.getButtonPress(R1, 0)){
      mp3.playTrack(12);
    } else {
      mp3.playTrack(random(25,32));
    }
  }

  // turn hp light on & off with Right Analog Stick Press (R3) for left stick drive mode
  // turn hp light on & off with Left Analog Stick Press (L3) for right stick drive mode
  /*
  if(Xbox.getButtonClick(hpLightToggleButton, 0))  {
    // if hp light is on, turn it off
    if(isHPOn){
      isHPOn = false;
      // turn hp light off
    } else {
      isHPOn = true;
      // turn hp light on
    }
  }
  */


  // Change drivespeed if drive is eabled
  // Press Left Analog Stick (L3) for left stick drive mode
  // Press Right Analog Stick (R3) for right stick drive mode
  // Set LEDs for speed - 1 LED, Low. 2 LED - Med. 3 LED High
  if(Xbox.getButtonClick(speedSelectButton, 0) && isDriveEnabled) {
    //if in lowest speed
    if(drivespeed == DRIVESPEED1){
      //change to medium speed and play sound 3-tone
      drivespeed = DRIVESPEED2;
      Xbox.setLedOn(LED2, 0);
      mp3.playTrack(53);
    } else if(drivespeed == DRIVESPEED2 && (DRIVESPEED3!=0)){
      //change to high speed and play sound scream
      drivespeed = DRIVESPEED3;
      Xbox.setLedOn(LED3, 0);
      mp3.playTrack(1);
    } else {
      //we must be in high speed
      //change to low speed and play sound 2-tone
      drivespeed = DRIVESPEED1;
      Xbox.setLedOn(LED1, 0);
      mp3.playTrack(52);
    }
  }


  // FOOT DRIVES
  // Xbox 360 analog stick values are signed 16 bit integer value
  // Sabertooth runs at 8 bit signed. -127 to 127 for speed (full speed reverse and  full speed forward)
  // Map the 360 stick values to our min/max current drive speed
  throttleStickValue = (map(Xbox.getAnalogHat(throttleAxis, 0), -32768, 32767, -drivespeed, drivespeed));
  if (throttleStickValue > -DRIVEDEADZONERANGE && throttleStickValue < DRIVEDEADZONERANGE) {
    // stick is in dead zone - don't drive
    driveThrottle = 0;
  } else {
    if (driveThrottle < throttleStickValue) {
      if (throttleStickValue - driveThrottle < (RAMPING + 1) ) {
        driveThrottle += RAMPING;
      } else {
        driveThrottle = throttleStickValue;
      }
    } else if (driveThrottle > throttleStickValue) {
      if (driveThrottle - throttleStickValue < (RAMPING + 1) ) {
        driveThrottle -= RAMPING;
      } else {
        driveThrottle = throttleStickValue;
      }
    }
  }

  turnThrottle = map(Xbox.getAnalogHat(turnAxis, 0), -32768, 32767, -TURNSPEED, TURNSPEED);

  // DRIVE!
  // right stick (drive)
  if (isDriveEnabled) {
    // Only do deadzone check for turning here. Our Drive throttle speed has some math applied
    // for RAMPING and stuff, so just keep it separate here
    if (turnThrottle > -DRIVEDEADZONERANGE && turnThrottle < DRIVEDEADZONERANGE) {
      // stick is in dead zone - don't turn
      turnThrottle = 0;
    }
    ST.turn(-turnThrottle);
    ST.drive(driveThrottle);
  }

  // DOME DRIVE!
  domeThrottle = (map(Xbox.getAnalogHat(domeAxis, 0), -32768, 32767, DOMESPEED, -DOMESPEED));
  if (domeThrottle > -DOMEDEADZONERANGE && domeThrottle < DOMEDEADZONERANGE) {
    //stick in dead zone - don't spin dome
    domeThrottle = 0;
  }

  #if defined(SYRENSIMPLE)
    SyR.motor(domeThrottle);
  #else
    SyR.motor(1,domeThrottle);
  #endif
} // END loop()
