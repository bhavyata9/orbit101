extern "C" {
#include <FillPat.h>
#include <EEPROM.h>
#include <LaunchPad.h>
#include <my_oled.h>
#include <delay.h>
#include "OrbitBoosterPackDefs.h"
#include <OrbitOled.h>
#include <OrbitOledChar.h>
#include <OrbitOledGrph.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
}
#define LED RED_LED



extern int xchOledMax; // defined in OrbitOled.c
extern int ychOledMax; // defined in OrbitOled.c
bool fClearOled; // local variable needed

/*
 * Used with getIt to check which Cmd_Type is being requested
 */
enum Cmd_Type { // Enums don't work as function return types or parameters for TI :/
  twist_it,
  shake_it,
  orb_it,
  flick_it
};

const int nameSize = 3;
const int scoreSize = 50;
const int SHAKE_THRESH = 600; // for shake it!
const int TWIST_THRESH = 550; // for twist it!
const int MAX_TWIST_READING = 4095;
const int START_DELAY = 2000; // delay before welcome and start msg (startscreen)
const int CNTDWN_DELAY = 1000; // delay between numbers for count down (startscreen)
const int BTN_DELAY = 190; // delay between button presses for name select and start screen
const int SGL_PLAYER_DELAY = 500; // single player delay
const int PASS_DELAY = 2000; // delay between passes
const int ROUND_DELAY = 1500; // delay between each round
const int FAIL_DELAY = 3000; // how long the game over screen plays
const int END_DELAY = 1500; // delay in endscreen before high scores
const int DEC_DEADLINE = 200; // amount the time decreases (halved for single player)
const int MIN_DEADLINE = 600; // the minimum time decreased
int deadLine = 3000; // time allotted per player
char *CMDSTRINGS[] = {"Twist It!","Shake It!","Orb It!","Flick It!"}; // in the same order as enum
char welcomeMsg[] = "ORB IT!";
char successMsg[] = "PASS IT ON"; // confirmation for correct input
char failMsg[] = "Game Over :("; // displayed when incorrect input, time too long
char scoreMsg[] = "Your Score:"; // displayed on final score screen
char newHighScoreMsg[] = "NEW HIGH SCORE";
char highScoreMsg[] = "High Scores";
char selectMsg[] = "Choose a Name:";
char confirmMsg[] = "FLICK to End";
char scoreString[scoreSize]; // 50 digits for the score because wtf 9000
char roundString[10]; // stores the current round
int MAX_PLAYERS = 10;
int totalPlayers = 1;
int curPlayer = 1; // current player number
int curRound = 0; // rounds passed
bool DEBUG = false; // for debugging
bool RESET = false; // set true to reset all the scores to dev 0
char debugString[10];

const int ledPin1 =  GREEN_LED;
const int ledPin2 =  BLUE_LED;
const int ledPin3 =  RED_LED;

void setup() {
  if (RESET) {
    char *s = "dev";
    int scoreS = 0;
    char *t = "dev";
    int scoreT = 0;
    char *bha = "dev";
    int scoreBha = 0;
    writeEEPROM(200, scoreS, s);
    writeEEPROM(100, scoreT, t);
    writeEEPROM(0, scoreBha, bha);
  }

  pinMode(LED, OUTPUT);
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);

  GPIOPadConfigSet(BTN1Port, BTN1, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD);
  GPIOPadConfigSet(BTN2Port, BTN2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD);
  GPIOPadConfigSet(SWTPort, SWT1 | SWT2, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPD);

  GPIOPinTypeGPIOInput(BTN1Port, BTN1);
  GPIOPinTypeGPIOInput(BTN2Port, BTN2);
  GPIOPinTypeGPIOInput(SWTPort, SWT1 | SWT2);

  //Potentiometer stuff (The twist thing)
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
  GPIOPinTypeADC(AINPort, AIN);
  ADCSequenceConfigure(ADC0_BASE, 0, ADC_TRIGGER_PROCESSOR, 0);
  ADCSequenceStepConfigure(ADC0_BASE, 0, 0, ADC_CTL_IE | ADC_CTL_END | ADC_CTL_CH0);
  ADCSequenceEnable(ADC0_BASE, 0);

  // nice lil Serial debugging port
  Serial.begin(9600);

  // seed the random number with random noise? this code is copied :|
  randomSeed(analogRead(0));

  OrbitOledInit(); // set up the oled display
  fClearOled = true; // clears the screen
  if (!DEBUG) startScreen(); // remove it for debugging
}

void loop() {
  if (!DEBUG){ // set debug to true; paste code in else portion

    if (runAction()) { // increment the score based on the number of rounds
      //Serial.println(deadLine);
      if (totalPlayers == 1) {
        fClearOled = true;


        curRound++;
        Serial.println("success");
        delay(20);
        Serial.println(187,DEC);
        sprintf(roundString,"%d",curRound);
        updateScreen(roundString,true);
        delay(SGL_PLAYER_DELAY); // shortened delay for single player
        if (deadLine > MIN_DEADLINE) deadLine -= DEC_DEADLINE/2; // time shortens more slowly for single player
      } else {
        fClearOled = true;
        curPlayer++;
        if (curPlayer > totalPlayers){ // if it's the last player of the round
        Serial.println("success");
        delay(20);
        Serial.println(187,DEC);
        delay(2);
        Serial.println(78,DEC);
        delay(2);
          char roundMsg[20] = "Round ";
          curPlayer = 1;
          curRound++;
          sprintf(roundString,"%d",curRound);
          strcat(roundMsg,roundString);
          strcat(roundMsg," Over");
          updateScreen(roundMsg,true);
          delay(ROUND_DELAY);
          if (deadLine > MIN_DEADLINE) deadLine -= DEC_DEADLINE; // time shortens each round
        } else {
          Serial.println("success");
          delay(20);
          Serial.println(187,DEC);
          delay(2);
          Serial.println(78,DEC);
          delay(2);
          updateScreen(successMsg, true, 2);
          delay(PASS_DELAY);
        }
      }
      // sprintf(debugString,"%d",deadLine);
    }
    else {
      return endScreen();
    }
  } else {
    // Serial.println(selectScreen());
    // while(1);
  }
}

/**
 * Returns a score based on a formula dependent on the current player
 * and current round
 */
int getPasses(){
  int scorePerPlayer = 100;
  int score = 0;
  int nPasses = (curPlayer - 1) + curRound*totalPlayers; // how many passes so far
  return nPasses;
}

/*
 * Rainbow Blinking for the beginning of the game
 */
int overTheRainbow(int t){
  unsigned long initTime = millis();
  unsigned long currentTime = millis();

  while(currentTime - initTime < t){
      digitalWrite(ledPin1, HIGH);
      delay(100);
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, HIGH);
      delay(100);
      digitalWrite(ledPin2, LOW);
      digitalWrite(ledPin3, HIGH);
      delay(100);
      digitalWrite(ledPin3, LOW);
      currentTime = millis();
  }
}

/*
 * Shows the startscreen
 * Sets the number of players
 */
int startScreen(){
  char startMsg[13] = "# Players:";
  char startMsg2[20] = "FLICK to Start";
  char playerString[4]; // max players is 2 digits long + space + null

  // INITIALIZE FLICK
  int original1 = GPIOPinRead(SWT1Port, SWT1); // original switch positions
  int original2 = GPIOPinRead(SWT2Port, SWT2);

  fClearOled = true;

  updateScreen(welcomeMsg, true, 2);
  overTheRainbow(START_DELAY);
  fClearOled = true;
  while (GPIOPinRead(SWT1Port, SWT1) == original1 && GPIOPinRead(SWT2Port, SWT2) == original2){ // while the switches haven't been flicked
    if (GPIOPinRead(BTN2Port, BTN2)) { // move up
      if (totalPlayers == MAX_PLAYERS)
        totalPlayers = 1;
      else
        totalPlayers++;
    }
    else if (GPIOPinRead(BTN1Port, BTN1)) { // move down
      if (totalPlayers == 1)
        totalPlayers = MAX_PLAYERS;
      else
        totalPlayers--;
    }
    delay(BTN_DELAY); // button sensitivity
    sprintf(playerString, "%d",totalPlayers);

    if (totalPlayers==9 || totalPlayers == 1){ // gets rid of that pesky leading 1 digit
        updateScreen(" ",xchOledMax-2,1,true);
    }

    updateScreen(startMsg, 0, 1, true);
    updateScreen(playerString, xchOledMax-strlen(playerString),1,true);
    updateScreen(startMsg2,true,3);
  }
  fClearOled = true;
  strcpy(startMsg,"Players: ");
  strcat(startMsg, playerString);
  updateScreen(startMsg, true);
  delay(CNTDWN_DELAY/2);
  fClearOled = true;
  updateScreen("3",true);
  delay(CNTDWN_DELAY);
  fClearOled = true;
  updateScreen("2",true);
  delay(CNTDWN_DELAY);
  fClearOled = true;
  updateScreen("1",true);
  delay(CNTDWN_DELAY);
  fClearOled = true;
  updateScreen("Start!",true);

  delay(CNTDWN_DELAY/2); // start's a bit quicker
}

/**
 * Shows the endscreen
 * Just to make our end configurable
 */
void endScreen(){
  int score = getPasses();
  if (checkScoreEEPROM(score)){
  Serial.println(1000,DEC);
  delay(20);
  Serial.println(111,DEC);
  delay(2);
  Serial.println(1000,DEC);
  delay(2);}
else {
  Serial.println(78,DEC);
  delay(2);
  Serial.println("stop");
  delay(15);
  Serial.println(78,DEC);
  delay(2);
  int score = getPasses();}
  fClearOled = true;
  updateScreen(failMsg,true);


  digitalWrite(LED, HIGH); // red lights on
  delay(FAIL_DELAY);
  digitalWrite(LED, LOW); // red lights off
  fClearOled = true;
  score = getPasses();
  sprintf(scoreString, "%d", score);
  updateScreen(scoreMsg, true, 1);
  updateScreen(scoreString, true, 3);
  delay(END_DELAY);
  fClearOled = true;

  // HIGH SCORES
  if (checkScoreEEPROM(score)){


    updateScreen(newHighScoreMsg,true);

    overTheRainbow(END_DELAY);
    char *name = selectScreen();
    updateEEPROM(score,name);
  } else;
  printEEPROM();
  while (1);
}

char* selectScreen(){
  char *nameString = (char*)malloc(4*sizeof(char));
  if (!nameString) //Serial.println("Error allocating memory for namestring");
  nameString[0] = 'A'; nameString[1] = 'A'; nameString[2] = 'A'; nameString[3] = '\0';
  char cursorString[4] = {'^',' ',' ','\0'}; // indicates the current cursor position
  int NALPHA = 26;
  int letterAt = 0;
  char letter = nameString[letterAt]; // initialize

  // INITIALIZE FLICK & SHAKE
  int original1 = GPIOPinRead(SWT1Port, SWT1); // original switch positions
  int original2 = GPIOPinRead(SWT2Port, SWT2);

  short dataX, dataY, dataZ; // coordinate values
  char  chPwrCtlReg = 0x2D, chX0Addr = 0x32, chY0Addr = 0x34,  chZ0Addr = 0x36; // addresses?
  char  rgchReadAccl_x[] = {0, 0, 0}, rgchReadAccl_y[] = {0, 0, 0}, rgchReadAccl_z[] = {0, 0, 0}; // where the data's stored
  char  rgchWriteAccl[] = {0, 0}; // facilitates writing
  SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0); // enable the I2C peripheral (accel)
  SysCtlPeripheralReset(SYSCTL_PERIPH_I2C0);
  GPIOPinTypeI2C(I2CSDAPort, I2CSDA_PIN); // set the pins
  GPIOPinTypeI2CSCL(I2CSCLPort, I2CSCL_PIN);
  GPIOPinConfigure(I2CSCL);
  GPIOPinConfigure(I2CSDA);
  I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), false); // set up I2C
  GPIOPinTypeGPIOInput(ACCL_INT2Port, ACCL_INT2); // initialize accl
  rgchWriteAccl[0] = chPwrCtlReg;
  rgchWriteAccl[1] = 1 << 3; // sets accl in measurement mode
  I2CGenTransmit(rgchWriteAccl, 1, WRITE, ACCLADDR);

  fClearOled = true;
  updateScreen(selectMsg, true, 0);
  updateScreen(nameString,true,1);
  updateScreen(cursorString,true,2);
  updateScreen(confirmMsg,true, 5);
  while (GPIOPinRead(SWT1Port, SWT1) == original1 && GPIOPinRead(SWT2Port, SWT2) == original2){ // while a button hasn't been pressed
    if (GPIOPinRead(BTN2Port, BTN2)) { // move up
      if (letter == 'Z') // 'Z' go to 'A'
        letter = 'A';
      else
        letter++;
      delay(BTN_DELAY); // button sensitivity
    }
    else if (GPIOPinRead(BTN1Port, BTN1)) { // move down
      if (letter == 'A') // 'A' go to 'Z'
        letter = 'Z';
      else
        letter--;
      delay(BTN_DELAY); // button sensitivity
    }
    nameString[letterAt] = letter;
    //Serial.println(nameString); // TODO remove
    updateScreen(nameString,true,1);

    // SHAKE
    rgchReadAccl_x[0] = chX0Addr; // sets the channels for reading, i think
    rgchReadAccl_y[0] = chY0Addr;
    rgchReadAccl_z[0] = chZ0Addr;
    I2CGenTransmit(rgchReadAccl_x, 2, READ, ACCLADDR); // gets the accelerometer data
    I2CGenTransmit(rgchReadAccl_y, 2, READ, ACCLADDR);
    I2CGenTransmit(rgchReadAccl_z, 2, READ, ACCLADDR);
    dataX = (rgchReadAccl_x[2] << 8) | rgchReadAccl_x[1]; // get readable data
    dataY = (rgchReadAccl_y[2] << 8) | rgchReadAccl_y[1];
    dataZ = (rgchReadAccl_z[2] << 8) | rgchReadAccl_z[1];

    if (sqrt(dataX*dataX+dataY*dataY+dataZ*dataZ) > SHAKE_THRESH){ // shake to switch letter at
      cursorString[letterAt] = ' ';
      if (letterAt == 2)
        letterAt = 0;
      else
        letterAt++;
      cursorString[letterAt] = '^';
      letter = nameString[letterAt];
      updateScreen(cursorString,true,2);
      delay(BTN_DELAY);
    }
  }
  //delay(1000);
  return nameString;
}

/*
 * Runs a random command
 */
int runAction() {
  int action = random(0, 4); // note: this is exclusive; rub it currently not included
  fClearOled = true;
  updateScreen(CMDSTRINGS[action],true);
  return getIt(action);
}

/*
 * Sends the actual command for the user to respond to
 * Returns 1 if the command was correct, 0 else
 */
int getIt(int cmd){ // enums aren't working for some reason
  /*
   * FLICK IT
   */
  int original1 = GPIOPinRead(SWT1Port, SWT1); // original switch positions
  int original2 = GPIOPinRead(SWT2Port, SWT2);
  /*
   * TWIST IT
   */
  uint32_t ulAIN0; // where we store the current potentio readings
  uint32_t init; // the initial reading as reference point
  ADCProcessorTrigger(ADC0_BASE, 0); // initiate ADC conversion
  while (!ADCIntStatus(ADC0_BASE, 0, false)); // waits until ready (i think)
  ADCSequenceDataGet(ADC0_BASE, 0, &ulAIN0); // TODO: switch to init
  ADCProcessorTrigger(ADC0_BASE, 0); // repeat for accuracy (required!)
  while (!ADCIntStatus(ADC0_BASE, 0, false));
  ADCSequenceDataGet(ADC0_BASE, 0, &ulAIN0);
  init = ulAIN0;
  int diff = 0; // initialize the difference we're gonna measure
  /*
   * SHAKE IT
   */
  short dataX, dataY, dataZ; // coordinate values
  char  chPwrCtlReg = 0x2D, chX0Addr = 0x32, chY0Addr = 0x34,  chZ0Addr = 0x36; // addresses?
  char  rgchReadAccl_x[] = {0, 0, 0}, rgchReadAccl_y[] = {0, 0, 0}, rgchReadAccl_z[] = {0, 0, 0}; // where the data's stored
  char  rgchWriteAccl[] = {0, 0}; // facilitates writing
  SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0); // enable the I2C peripheral (accel)
  SysCtlPeripheralReset(SYSCTL_PERIPH_I2C0);
  GPIOPinTypeI2C(I2CSDAPort, I2CSDA_PIN); // set the pins
  GPIOPinTypeI2CSCL(I2CSCLPort, I2CSCL_PIN);
  GPIOPinConfigure(I2CSCL);
  GPIOPinConfigure(I2CSDA);
  I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), false); // set up I2C
  GPIOPinTypeGPIOInput(ACCL_INT2Port, ACCL_INT2); // initialize accl
  rgchWriteAccl[0] = chPwrCtlReg;
  rgchWriteAccl[1] = 1 << 3; // sets accl in measurement mode
  I2CGenTransmit(rgchWriteAccl, 1, WRITE, ACCLADDR);

  int deadTime = millis()+deadLine; // set timer

  while(millis()<deadTime){ // check conditions
    /*
     * FLICK IT
     */
    if (GPIOPinRead(SWT1Port, SWT1) != original1 ||
      GPIOPinRead(SWT2Port, SWT2) != original2){ // check if switches have moved
      if (cmd == flick_it) return 1;
      else return 0;
    }
    /*
     * ORB IT
     */
    if (GPIOPinRead(BTN1Port, BTN1) ||
      GPIOPinRead(BTN2Port, BTN2)){ // check if button pressed
      if (cmd == orb_it) return 1;
      else return 0;
    }
    /*
     * TWIST IT
     */
    ADCProcessorTrigger(ADC0_BASE, 0); // initiate ADC Conversion
    while (!ADCIntStatus(ADC0_BASE, 0, false)); // waits until ready
    ADCSequenceDataGet(ADC0_BASE, 0, &ulAIN0); // stores potentiometer data
    diff = abs(ulAIN0 - init); // difference between current state and initial state
    if (diff > TWIST_THRESH){ // if twisted beyond the threshold
      if (cmd == twist_it) return 1;
      else return 0;
    }
    /*
     * SHAKE IT
     */
    rgchReadAccl_x[0] = chX0Addr; // sets the channels for reading, i think
    rgchReadAccl_y[0] = chY0Addr;
    rgchReadAccl_z[0] = chZ0Addr;
    I2CGenTransmit(rgchReadAccl_x, 2, READ, ACCLADDR); // gets the accelerometer data
    I2CGenTransmit(rgchReadAccl_y, 2, READ, ACCLADDR);
    I2CGenTransmit(rgchReadAccl_z, 2, READ, ACCLADDR);
    dataX = (rgchReadAccl_x[2] << 8) | rgchReadAccl_x[1]; // get readable data
    dataY = (rgchReadAccl_y[2] << 8) | rgchReadAccl_y[1];
    dataZ = (rgchReadAccl_z[2] << 8) | rgchReadAccl_z[1];
    if (sqrt(dataX*dataX+dataY*dataY+dataZ*dataZ) > SHAKE_THRESH){ // if the magnitude is beyond
      if (cmd == shake_it) return 1;
      else return 0;
    }
  }
  return 0;
}

/**********************************************
 * CODE NEEDED FOR SHAKE
 *********************************************/
 char I2CGenTransmit(char * pbData, int cSize, bool fRW, char bAddr) {
  int i;
  char * pbTemp;
  pbTemp = pbData;
  /*Start*/
  /*Send Address High Byte*/
  /* Send Write Block Cmd*/
  I2CMasterSlaveAddrSet(I2C0_BASE, bAddr, WRITE);
  I2CMasterDataPut(I2C0_BASE, *pbTemp);
  I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_START);
  DelayMs(1);
  /* Idle wait*/
  while (I2CGenIsNotIdle())
    ;
  /* Increment data pointer*/
  pbTemp++;
  /*Execute Read or Write*/
  if (fRW == READ) {
    /* Resend Start condition
     ** Then send new control byte
     ** then begin reading
     */
     I2CMasterSlaveAddrSet(I2C0_BASE, bAddr, READ);
     while (I2CMasterBusy(I2C0_BASE))
      ;
    /* Begin Reading*/
     for (i = 0; i < cSize; i++) {
      if (cSize == i + 1 && cSize == 1) {
        I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_SINGLE_RECEIVE);
        DelayMs(1);
        while (I2CMasterBusy(I2C0_BASE))
          ;
      } else if (cSize == i + 1 && cSize > 1) {
        I2CMasterControl(I2C0_BASE,
          I2C_MASTER_CMD_BURST_RECEIVE_FINISH);
        DelayMs(1);
        while (I2CMasterBusy(I2C0_BASE))
          ;
      } else if (i == 0) {
        I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_RECEIVE_START);
        DelayMs(1);
        while (I2CMasterBusy(I2C0_BASE))
          ;
        /* Idle wait*/
        while (I2CGenIsNotIdle())
          ;
      } else {
        I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_RECEIVE_CONT);
        DelayMs(1);
        while (I2CMasterBusy(I2C0_BASE))
          ;
        /* Idle wait */
        while (I2CGenIsNotIdle())
          ;
      }
      while (I2CMasterBusy(I2C0_BASE))
        ;
      /* Read Data */
      *pbTemp = (char) I2CMasterDataGet(I2C0_BASE);
      pbTemp++;
     }
    } else if (fRW == WRITE) {
    /*Loop data bytes */
      for (i = 0; i < cSize; i++) {
      /* Send Data */
        I2CMasterDataPut(I2C0_BASE, *pbTemp);
        while (I2CMasterBusy(I2C0_BASE))
          ;
        if (i == cSize - 1) {
          I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_FINISH);
          DelayMs(1);
          while (I2CMasterBusy(I2C0_BASE))
            ;
        } else {
          I2CMasterControl(I2C0_BASE, I2C_MASTER_CMD_BURST_SEND_CONT);
          DelayMs(1);
          while (I2CMasterBusy(I2C0_BASE))
            ;
        /* Idle wait */
          while (I2CGenIsNotIdle())
            ;
        }
        pbTemp++;
      }
    }
    return 0x00;
  }

 bool I2CGenIsNotIdle() {
  return !I2CMasterBusBusy(I2C0_BASE);
 }

/**********************************************
 * SCREEN METHODS
 *********************************************/
void updateScreen(char sentence[], int x, int y, bool clear) {
  OrbitOledMoveTo(0, 0);
  if (clear) {
    if (fClearOled) {
      OrbitOledClear();
      OrbitOledMoveTo(0, 0);
      OrbitOledSetCursor(0, 0);
      fClearOled = false;
    }
  }
  OrbitOledSetCursor(x, y);
  OrbitOledPutString(sentence);

}

void updateScreen(char sentence[], bool clear) {
  if (clear) {
    if (fClearOled) {
      OrbitOledClear();
      OrbitOledMoveTo(0, 0);
      OrbitOledSetCursor(0, 0);
      fClearOled = false;
    }
  }
  int size = strlen(sentence);
  OrbitOledSetCursor((xchOledMax - size) / 2.0 + 1, ychOledMax / 2);
  OrbitOledPutString(sentence);
}

void updateScreen(char sentence[], bool clear, double y) {
  OrbitOledMoveTo(0, 0);
  if (clear) {
    if (fClearOled) {
      OrbitOledClear();
      OrbitOledMoveTo(0, 0);
      OrbitOledSetCursor(0, 0);
      fClearOled = false;
    }
  }
  int size = strlen(sentence);
  OrbitOledSetCursor((xchOledMax - size) / 2.0 + 1, y);
  OrbitOledPutString(sentence);
}

void updateScreen(char sentence[]) {
  int size = strlen(sentence);
  OrbitOledSetCursor((xchOledMax - size) / 2.0 + 1, ychOledMax / 2);
  OrbitOledPutString(sentence);
}

/************************************************
 * HIGH SCORES
 ***********************************************/

/*
 * Writes the name and score to a specific address
 */
void writeEEPROM(int addr, int score, char* name){
  int nDigits;
  if (!score) nDigits = 1;
  else nDigits = (int) (floor(log10(abs(score))) + 1);
    for (int i = addr; i < addr + scoreSize-nDigits; i++){
      EEPROM.write(i, 0);
    }

    int a = score;
    if (!a)
      EEPROM.write(addr + scoreSize - 1, 0);
    else if(nDigits > 0)
    {
        for(int k = addr + scoreSize - 1; k >= addr + scoreSize - nDigits && a != 0; k--)
        {
           int nextDigit = a % 10;
            a /= 10;
            EEPROM.write(k, nextDigit);
        }
    }

    // put the name in some characters later
    int charIndex = 0;
    for(int i = addr + scoreSize + 1; i < addr + scoreSize + nameSize + 1; i++)
    {
      EEPROM.write(i, (int)name[charIndex]);
      charIndex++;
    }
}

/*
 * Reads the score from an address
 */
int readEEPROMScore(int addr){
  int a = 0;
  int power = 0;
  for(int i = addr; i < addr + scoreSize; i++){
    a += (int) EEPROM.read(i) * (int) floor(pow(10, scoreSize-power-1));
    power++;
  }

  return a;
}

/*
 * Reads the name from an address
 */
char * readEEPROMName(int addr){
  char * s = (char *)malloc((nameSize + 1) * sizeof(char));
  s[nameSize] = '\0';
  int a = 0;
  for(int i = addr + scoreSize + 1; i < addr + scoreSize + nameSize + 1; i++){
    s[a] = (char) EEPROM.read(i);
    a++;
  }
  return s;
}

/*
 * Adds a new entry to memory if score is high enough
 */
void updateEEPROM(int score, char *name){
  // addresses (constant)
  int addr1 = 0;
  int addr2 = 100;
  int addr3 = 200;
  if (score>readEEPROMScore(addr1)){ // if the score is the highest
    int score1 = readEEPROMScore(addr1);
    char* name1 = readEEPROMName(addr1);
    int score2 = readEEPROMScore(addr2);
    char *name2 = readEEPROMName(addr2);
    writeEEPROM(addr1,score,name);
    writeEEPROM(addr2,score1,name1);
    writeEEPROM(addr3,score2,name2);
  } else if (score>readEEPROMScore(addr2)){ // if the score is the second highest
    int score2 = readEEPROMScore(addr2);
    char *name2 = readEEPROMName(addr2);
    writeEEPROM(addr2,score,name);
    writeEEPROM(addr3,score2,name2);
  } else if (score>readEEPROMScore(addr3)){ // if the score is the third highest
    writeEEPROM(addr3,score,name);
  }
}

/*
 * Check if a score is higher than the current
 * listings
 */
int checkScoreEEPROM(int score){
  if(score > readEEPROMScore(200))
     return 1;
    return 0;
}

// print everybody on the top players list
int printEEPROM()
{
  int x = 5;
  double y = 1;
  fClearOled = true;
  updateScreen(highScoreMsg,true,0);
  // go through each addreess on the list buddy
  for(int i = 0; i < 3; i++)
  {
    int nextAddress = 100 * i;
    int nextScore = readEEPROMScore(nextAddress);
    char *player = append(nextAddress);

    updateScreen(player,x,y,true);
    y += 1;
    delay(1000);

  }
  delay(3000);
}

/*
 * Returns the appended name and score from an address
 */
char* append(int addr){
  int power = 0;
  int score = 0;
  int nDigits = 0;
  char *s;
  char *t;
  int a = 0;
  for(int i = addr; i < addr + scoreSize; i++){
      score += (int) EEPROM.read(i) * (int) floor(pow(10, scoreSize-power-1));
      power++;
  }

  if(score>0) {
    nDigits = floor(log10(abs(score))) + 1;
  } else { // log10(0) is undefined
   nDigits = 1;
  }

  s = (char *)malloc((nDigits + nameSize + 3) * sizeof(char));
  t = readEEPROMName(addr);
  s[nDigits + nameSize + 2] = '\0';

  for (int i = 0; i < nameSize; i++){
    s[i] = t[a];
    a++;
  }

  s[nameSize] = ':';
  s[nameSize + 1] = ' ';

  for(int i = nameSize + 1 + nDigits; i >= nameSize + 2; i--){
    s[i] = (char) (48 + score % 10);
    score/=10;
  }

  return s;
}
