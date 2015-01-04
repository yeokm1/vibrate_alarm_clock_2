#include <Wire.h>
#include <MicroView.h>
#include <DS3232RTC.h>
#include <Time.h>

#include "LowPower.h"


#define LEFT_BUTTON_PIN A3
#define MIDDLE_BUTTON_PIN A2
#define RIGHT_BUTTON_PIN A1

#define VOLTAGE_MEASURE_PIN A0

#define MOTOR_PIN1 5
#define MOTOR_PIN2 3
#define MOTOR_SLEEP_PIN 6

#define ALARM_PIN INT0 //Driven LOW by Chronodot on alarm triggered

#define ADC_PRECISION 1024

#define MIN_BATTERY_MILLIVOLT 3200 //You may need to calibrate this
#define MAX_BATTERY_MILLIVOLT 4200 //You may need to calibrate this

#define MIN_TIME_BETWEEN_BUTTON_PRESSES 100  //Debouncing purposes

#define MAX_ALARM_LENGTH 600000 //Alarm rings for 10 minutes max


#define BLINK_INTERVAL_SETTINGS 100
#define BLINK_INTERVAL_SECONDS 500

#define MIN_TIME_BETWEEN_ALARM_STARTS 60000 //60 seconds
#define MIN_TIME_BETWEEN_MOTOR_STATES 500

#define INITIAL_TEXT "Happy 25thBirthday\nKai Yao!\n\nBy: YKM\n12/01/2015"
#define INITIAL_TEXT_DELAY 5000

typedef enum {
  NORMAL, ALARM, SETTING_TIME, SETTING_ALARM} 
CLOCK_STATE ;
volatile CLOCK_STATE currentState = NORMAL;

typedef enum {
  T_HOUR, T_MINUTE} 
SETTING_TIME_STATE ;
SETTING_TIME_STATE settingTimeProcess;

typedef enum {
  A_HOUR, A_MINUTE, A_TYPE} 
SETTING_ALARM_STATE ;
SETTING_ALARM_STATE settingAlarmProcess;

int alarmHour = 07;
int alarmMinute = 00;

unsigned long alarmLastStarted;

unsigned long timeLastPressedLeftButton;
unsigned long timeLastPressedMiddleButton;
unsigned long timeLastPressedRightButton;

boolean vibrate = false;

boolean previousBlinkState;
unsigned long previousBlinkTime;

boolean previousSecondsBlinkState;
unsigned long previousSecondsBlinkTime;

boolean showLCD = true;
unsigned long lastVibration;
boolean lastMotorState = false;


const int numReadings = 40;

int percentReadings[numReadings];
int percentIndex = 0;    
long percentTotal = 0; 

int milliVoltReadings[numReadings];
int milliVoltIndex = 0;     
long milliVoltTotal = 0;     



void setup() {

  Wire.begin();
  Serial.begin(9600);

  setSyncProvider(RTC.get);
  
    //Clear prior alarm settings on reload
  RTC.squareWave(SQWAVE_NONE);
  RTC.alarmInterrupt(ALARM_1, false);  
  RTC.alarmInterrupt(ALARM_2, false);
  
  

  uView.begin();      // begin of MicroView
  uView.clear(ALL);   // erase hardware memory inside the OLED controller

  pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);

  pinMode(VOLTAGE_MEASURE_PIN, INPUT);


  pinMode(MOTOR_PIN1, OUTPUT); 
  pinMode(MOTOR_PIN2, OUTPUT);
  pinMode(MOTOR_SLEEP_PIN, OUTPUT);

  timeLastPressedLeftButton = 0;
  timeLastPressedMiddleButton = 0;
  timeLastPressedRightButton = 0;

  uView.clear(PAGE); 
  uView.setFontType(0);

  uView.println(INITIAL_TEXT); 

  uView.display();


  delay(INITIAL_TEXT_DELAY);
  
  attachInterrupt(ALARM_PIN, alarmTriggered, FALLING);




}

void loop (){
  
  setSyncProvider(RTC.get);

  int leftButtonState = digitalRead(LEFT_BUTTON_PIN);
  int middleButtonState = digitalRead(MIDDLE_BUTTON_PIN);
  int rightPinState = digitalRead(RIGHT_BUTTON_PIN);


  if(leftButtonState == LOW){
    processLeftButtonPressed();
  }

  if(middleButtonState == LOW){
    processMiddleButtonPressed();
  }

  if(rightPinState == LOW){
    processRightButtonPressed();
  }

  soundAlarmAtThisPointIfNeeded();

  //Disable alarm if it has gone longer than maximum
  if(currentState == ALARM && (millis() - alarmLastStarted) > MAX_ALARM_LENGTH){
    stopAlarm();
  }

  if(showLCD || currentState == ALARM){

    uView.clear(PAGE);

    boolean blinkOn = shouldBlinkNow();
    boolean secondsBlink = shouldSecondsBlinkNow();

    int voltageADCReading = getVoltageADCReading();

    long vccVoltage = getVcc();
    //Serial.println(vccVoltage);
    int batteryMilliVolt = ((long) voltageADCReading * vccVoltage) / ADC_PRECISION;



    writeTimeToDisplayBuffer(blinkOn, secondsBlink);
    writeAlarmToDisplayBuffer(blinkOn);
    writeVoltageToDisplayBuffer(batteryMilliVolt);
    writeButtonStateToDisplayBuffer(blinkOn);


    uView.display();
  }


  if(currentState == NORMAL){
    //Only sleep if not doing anything else
     LowPower.powerDown(SLEEP_15Ms, ADC_OFF, BOD_OFF);

  }


}

int getVoltageADCReading(){
  return analogRead(VOLTAGE_MEASURE_PIN); 
}


void turnOffLCDIfCommandIsOff(){
  if(!showLCD){
    uView.clear(PAGE);
    uView.display();
  }
}

void processLeftButtonPressed(){
  unsigned long currentMillis = millis();

  if((currentMillis - timeLastPressedLeftButton) < MIN_TIME_BETWEEN_BUTTON_PRESSES){
    return;
  }

  timeLastPressedLeftButton = currentMillis;

  Serial.println("Left Button Pressed");

  switch(currentState)
  {
  case ALARM: 
    stopAlarm();
    break;
  case NORMAL :
    {
      showLCD = !showLCD;
      turnOffLCDIfCommandIsOff();

    }
    break;
  case SETTING_ALARM:
    {
      if(settingAlarmProcess == A_HOUR){
        alarmHour = getNextHourFromCurrentHour(alarmHour, false);
      } 
      else if(settingAlarmProcess == A_MINUTE){
        alarmMinute = getNextMinSecFromCurrentMinSec(alarmMinute, false);
      } 
      else if(settingAlarmProcess == A_TYPE){
        vibrate = !vibrate;
      }

    }
    break;
  case SETTING_TIME:
    {
      switch(settingTimeProcess){
      case T_HOUR:
        setTime(getNextHourFromCurrentHour(hour(), false), minute(), second(), day(), month(), year());
        break;
      case T_MINUTE:
        setTime(hour(), getNextMinSecFromCurrentMinSec(minute(), false), second(), day(), month(), year());
        break;
      default: 
        break;

      }


      RTC.set(now()); 
    }
    break;
  default: 
    break;
  }


}

void processMiddleButtonPressed(){
  unsigned long currentMillis = millis();


  if((currentMillis - timeLastPressedMiddleButton) < MIN_TIME_BETWEEN_BUTTON_PRESSES){
    return;
  }

  timeLastPressedMiddleButton = currentMillis;

  Serial.println("Middle Button Pressed");

  switch(currentState)
  {
  case ALARM: 
    stopAlarm();
    break;
  case NORMAL :
    {
      if(showLCD){
        currentState = SETTING_ALARM;
        settingAlarmProcess = A_HOUR;
      } 
      else {
        showLCD = true;
      }

    }
    break;
  case SETTING_ALARM:
    {
      if(settingAlarmProcess == A_HOUR){
        settingAlarmProcess = A_MINUTE;
      } 
      else if(settingAlarmProcess == A_MINUTE){
        settingAlarmProcess = A_TYPE;
      } 
      else {
        if(vibrate){
            RTC.setAlarm(ALM2_MATCH_HOURS, alarmMinute, alarmHour, 0);
            RTC.alarmInterrupt(ALARM_2, true);
        } else{
            RTC.alarmInterrupt(ALARM_2, false);
        }
        currentState = NORMAL;
      }

    }
    break;
  case SETTING_TIME:
    {
      if(settingTimeProcess == T_HOUR){
        settingTimeProcess = T_MINUTE;
      } 
      else {
        currentState = NORMAL;
      }

    }
    break;
  default: 
    break;
  }


}

void processRightButtonPressed(){
  unsigned long currentMillis = millis();


  if((currentMillis - timeLastPressedRightButton) < MIN_TIME_BETWEEN_BUTTON_PRESSES){
    return;
  }

  timeLastPressedRightButton = currentMillis;

  Serial.println("Right Button Pressed");


  switch(currentState)
  {
  case ALARM: 
    stopAlarm();
    break;
  case NORMAL :
    { 
      if(showLCD){
        currentState = SETTING_TIME;
        settingTimeProcess = T_HOUR;
      } 
      else {
        showLCD = true;
      }

    }
    break;
  case SETTING_ALARM:
    {
      if(settingAlarmProcess == A_HOUR){
        alarmHour = getNextHourFromCurrentHour(alarmHour, true);
      } 
      else if(settingAlarmProcess == A_MINUTE){
        alarmMinute = getNextMinSecFromCurrentMinSec(alarmMinute, true);
      } 
      else if(settingAlarmProcess == A_TYPE){
        vibrate = !vibrate;
      }
      
    }
    break;
  case SETTING_TIME:
    {
      switch(settingTimeProcess){
      case T_HOUR:
        setTime(getNextHourFromCurrentHour(hour(), true), minute(), second(), day(), month(), year());
        break;
      case T_MINUTE:
        setTime(hour(), getNextMinSecFromCurrentMinSec(minute(), true), second(), day(), month(), year());
        break;
      default: 
        break;

      }

      RTC.set(now()); 
    }


    break;
  default: 
    break;
  }

}


void alarmTriggered(){
  
  if(currentState == NORMAL && currentState != ALARM){
    currentState = ALARM;
    motorDriverState(true);
  }
}


void soundAlarmAtThisPointIfNeeded(){
  if(currentState == ALARM){

    unsigned long currentMillis = millis();
    if((currentMillis - lastVibration) > MIN_TIME_BETWEEN_MOTOR_STATES){
      lastVibration = currentMillis;

      if(lastMotorState){
        lastMotorState = false;
        changeMotorState(false, false);
      } 
      else {
        lastMotorState = true;
        changeMotorState(true, true);
      }


    }


  }


}



void stopAlarm(){
  RTC.alarm(ALARM_2);
  currentState = NORMAL;
  motorDriverState(false); 
  turnOffLCDIfCommandIsOff();
}

void motorDriverState(boolean state){

  if(state){
    digitalWrite(MOTOR_SLEEP_PIN, HIGH);
  } 
  else {
    digitalWrite(MOTOR_SLEEP_PIN, LOW);
  }
}

void changeMotorState(boolean active, bool direction){

  if(!active){
    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, LOW);
    return; 
  }

  if(direction){
    digitalWrite(MOTOR_PIN1, HIGH);
    digitalWrite(MOTOR_PIN2, LOW);     
  } 
  else {
    digitalWrite(MOTOR_PIN1, LOW);
    digitalWrite(MOTOR_PIN2, HIGH); 
  }



}

void writeTimeToDisplayBuffer(boolean blinkOn, boolean secondsBlinkOn){
  uView.setCursor(0,0);
  uView.setFontType(7);

  String timeString = generateTimeString(hour(), minute(), blinkOn, secondsBlinkOn);
  uView.println(timeString);
}

String generateTimeString(int hour, int minute, boolean blinkOn, boolean secondsBlinkOn){
  char buff[3];

  String hourString;

  if(currentState == SETTING_TIME && settingTimeProcess == T_HOUR && !blinkOn){
    hourString = "  ";
  } 
  else {
    sprintf(buff, "%02d", hour);
    hourString = buff;
  }

  String minuteString;
  if(currentState == SETTING_TIME && settingTimeProcess == T_MINUTE && !blinkOn){
    minuteString = "  ";
  } 
  else {
    sprintf(buff, "%02d", minute);
    minuteString = buff;
  }

  String secondString;

  if(secondsBlinkOn || currentState == SETTING_TIME){
    secondString = ":";
  } 
  else {
    secondString = " ";
  }


  String result = hourString + secondString + minuteString;

  return result;
}

void writeAlarmToDisplayBuffer(boolean blinkOn){
  uView.setFontType(0);
  uView.setCursor(0,24);

  char buff[3];

  String alarmHourString;

  if(currentState == SETTING_ALARM && settingAlarmProcess == A_HOUR && !blinkOn){
    //Now setting this, do not show 
    alarmHourString = "  ";
  } 
  else {
    sprintf(buff, "%02d", alarmHour);
    alarmHourString = buff;
  }

  String alarmMinuteString;

  if(currentState == SETTING_ALARM && settingAlarmProcess == A_MINUTE && !blinkOn){
    //Now setting this, do not show 
    alarmMinuteString = "  ";
  } 
  else {
    sprintf(buff, "%02d", alarmMinute);
    alarmMinuteString = buff;
  }

  String alarmTimeString;
  String alarmSetting;

  if(currentState == SETTING_ALARM && settingAlarmProcess == A_TYPE && !blinkOn){
    //Now setting this, do not show 
    alarmSetting = "";
  } 
  else {
    if(vibrate){
      alarmSetting = " On"; 
    }
    else {
      alarmSetting = " Off";
    }
  }

  alarmTimeString = alarmHourString + ":" + alarmMinuteString + alarmSetting;

  uView.print(alarmTimeString);

}


int getNewAverageReadingFromCurrentReading(int array[], int * index, long * total, int numReadings, int newReading){
  *total = *total - array[*index];
  array[*index] = newReading; 
  *total = *total + array[*index];    

  *index = *index + 1;                    


  if (*index >= numReadings) {
    *index = 0;                           
  }
  int average =  *total / numReadings;  
  return average;
}

void writeVoltageToDisplayBuffer(int batteryMilliVolt){
  uView.setFontType(0);
  uView.setCursor(0,32);

  batteryMilliVolt = getNewAverageReadingFromCurrentReading(milliVoltReadings, &milliVoltIndex, &milliVoltTotal, numReadings, batteryMilliVolt);
  float voltage = (float) batteryMilliVolt / 1000;

  //  char buff[6];
  //  String voltageString = dtostrf(voltage, 4, 2, buff);
  //
  //  uView.print(voltageString);
  //  uView.print("V");

  int batteryRange = MAX_BATTERY_MILLIVOLT - MIN_BATTERY_MILLIVOLT;

  //Cast to long as Arduino int is only 16 bits wide. Not enough to hold this result
  long numerator =  ((long) ((batteryMilliVolt - MIN_BATTERY_MILLIVOLT))) * 100;
  int batteryPercent = numerator / batteryRange;


  batteryPercent = getNewAverageReadingFromCurrentReading(percentReadings, &percentIndex, &percentTotal, numReadings, batteryPercent);


  if(batteryPercent < 100){
    uView.setCursor(47,32);
  } 
  else {
    uView.setCursor(41,32);
  }
  uView.print(batteryPercent);
  uView.print("%");

}

void writeButtonStateToDisplayBuffer(boolean blinkOn){
  uView.setFontType(0);
  uView.setCursor(0,41);

  String buttonFunction;
  switch(currentState)
  {
  case ALARM: 
    {
      if(!blinkOn){
        buttonFunction = "X   X   X";
      }

    }
    break;
  case NORMAL : 
    buttonFunction =  "Led Alm Ti";
    break;
  case SETTING_ALARM: 
    //Fallthrough
  case SETTING_TIME: 
    buttonFunction = "-  Next  +";
    break;
  default: 
    break;
  }

  uView.print(buttonFunction);
}


boolean shouldBlinkNow(){
  unsigned long currentMillis = millis();

  if(currentMillis - previousBlinkTime >  BLINK_INTERVAL_SETTINGS) {
    previousBlinkTime = currentMillis;   
    previousBlinkState = !previousBlinkState;
  }
  return previousBlinkState;
}


boolean shouldSecondsBlinkNow(){
  unsigned long currentMillis = millis();

  if(currentMillis - previousSecondsBlinkTime >  BLINK_INTERVAL_SECONDS) {
    previousSecondsBlinkTime = currentMillis;   
    previousSecondsBlinkState = !previousSecondsBlinkState;
  }
  return previousSecondsBlinkState;
}

//Access secret voltmeter for Analog reference comparison
//  https://code.google.com/p/tinkerit/wiki/SecretVoltmeter
long getVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV


  return result;
}


int getNextHourFromCurrentHour(int current, boolean increment){
  int change;
  if(increment){
    change = 1;
  } 
  else {
    change = -1;
  }

  int newValue = current + change;

  //To produce positive modulo result
  return (newValue % 24 + 24) % 24;
}

int getNextMinSecFromCurrentMinSec(int current, boolean increment){
  int change;
  if(increment){
    change = 1;
  } 
  else {
    change = -1;
  }

  int newValue = current + change;

  //To produce positive modulo result
  return (newValue % 60 + 60) % 60;
}












