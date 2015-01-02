#include <Wire.h>
#include <SPI.h>
#include <MicroView.h>
#include <RTClib.h>
#include <RTC_DS1307.h>


#define LEFT_BUTTON_PIN A3
#define MIDDLE_BUTTON_PIN A2
#define RIGHT_BUTTON_PIN A1

#define VOLTAGE_MEASURE_PIN A0

#define ADC_PRECISION 1024

#define MIN_BATTERY_MILLIVOLT 3200 //You may need to calibrate this
#define MAX_BATTERY_MILLIVOLT 4200 //You may need to calibrate this

#define MIN_TIME_BETWEEN_BUTTON_PRESSES 100  //Debouncing purposes

#define MAX_ALARM_LENGTH 600000 //Alarm rings for 10 minutes max


#define BLINK_INTERVAL_SETTINGS 100
#define BLINK_INTERVAL_SECONDS 500

typedef enum {
  NORMAL, ALARM, SETTING_TIME, SETTING_ALARM} 
CLOCK_STATE ;
CLOCK_STATE currentState = NORMAL;

typedef enum {
  T_HOUR, T_MINUTE, T_SECOND, T_DAY, T_MONTH, T_YEAR} 
SETTING_TIME_STATE ;
SETTING_TIME_STATE settingTimeProcess;

typedef enum {
  A_HOUR, A_MINUTE, A_TYPE} 
SETTING_ALARM_STATE ;
SETTING_ALARM_STATE settingAlarmProcess;


boolean alarmVibrate = false;

int alarmHour = 07;
int alarmMinute = 00;

unsigned long alarmLastStarted;


RTC_DS1307 RTC;


unsigned long timeLastPressedLeftButton;
unsigned long timeLastPressedMiddleButton;
unsigned long timeLastPressedRightButton;


boolean vibrate1 = false;
boolean vibrate2 = false;

boolean previousBlinkState;
unsigned long previousBlinkTime;

boolean previousSecondsBlinkState;
unsigned long previousSecondsBlinkTime;

boolean showLCD = true;

void setup() {

  Wire.begin();
  RTC.begin();
  Serial.begin(9600);


  // This section grabs the current datetime and compares it to
  // the compilation time.  If necessary, the RTC is updated.
  DateTime now = RTC.now();
  DateTime compiled = DateTime(__DATE__, __TIME__);
  if (now.unixtime() < compiled.unixtime()) {
    RTC.adjust(DateTime(__DATE__, __TIME__));
  } 


  uView.begin();      // begin of MicroView
  uView.clear(ALL);   // erase hardware memory inside the OLED controller

  pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(MIDDLE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);

  pinMode(VOLTAGE_MEASURE_PIN, INPUT);


  timeLastPressedLeftButton = 0;
  timeLastPressedMiddleButton = 0;
  timeLastPressedRightButton = 0;


}

void loop (){

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


  DateTime now = RTC.now();


  checkAndSoundAlarm(now.hour(), now.minute());
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

    writeTimeToDisplayBuffer(now, blinkOn, secondsBlink);
    writeAlarmToDisplayBuffer(blinkOn);
    writeVoltageToDisplayBuffer(batteryMilliVolt);
    writeButtonStateToDisplayBuffer(blinkOn);


    uView.display();
  }


  delay(30);


}

int getVoltageADCReading(){
  return analogRead(VOLTAGE_MEASURE_PIN); 
}


void turnOffLCD(){
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
  case ALARM: stopAlarm();
  break;
  case NORMAL :
  {
    showLCD = !showLCD;
    turnOffLCD();
    
  }
  break;
  case SETTING_ALARM:
  {
   if(settingAlarmProcess == A_HOUR){
       alarmHour = getNextHourFromCurrentHour(alarmHour, false);
    } else if(settingAlarmProcess == A_MINUTE){
       alarmMinute = getNextMinSecFromCurrentMinSec(alarmMinute, false);
    } else if(settingAlarmProcess == A_TYPE){
              
       if(!vibrate1 && !vibrate2){
          vibrate1 = true;
          vibrate2 = true;
       } else if(!vibrate1 && vibrate2){
          vibrate1 = false;
          vibrate2 = false;
       } else if(vibrate1 && !vibrate2){
         vibrate1 = false;
         vibrate2 = true;  
       } else {
          vibrate1 = true;
          vibrate2 = false;  
      }
   }
  
  }
  break;
  case SETTING_TIME:
  {
    DateTime now = RTC.now();
    DateTime newTime;
    switch(settingTimeProcess){
      case T_HOUR:
        newTime = DateTime(now.year(), now.month(), now.day(), getNextHourFromCurrentHour(now.hour(), false), now.minute(), now.second());
        break;
      case T_MINUTE:
        newTime = DateTime(now.year(), now.month(), now.day(), now.hour(), getNextMinSecFromCurrentMinSec(now.minute(), false), now.second());
        break;
      case T_SECOND:
        newTime = DateTime(now.year(), now.month(), now.day(), now.hour(), now.minute(), getNextMinSecFromCurrentMinSec(now.second(), false));
        break;
      case T_DAY:
        newTime = DateTime(now.year(), now.month(), now.day() - 1, now.hour(), now.minute(), now.second());
        break;
      case T_MONTH:
      {
        int month = now.month();
        if(month == 1){
          month = 12;
        } else {
          month--;
        }
      
        newTime = DateTime(now.year(), month, now.day(), now.hour(), now.minute(), now.second());
      }
        break;
      case T_YEAR:
        newTime = DateTime(now.year() - 1, now.month(), now.day(), now.hour(), now.minute(), now.second());
        break;
      default: break;
     
    }
       
    RTC.adjust(newTime);
  }
  break;
  default: break;
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
    case ALARM: stopAlarm();
    break;
    case NORMAL :
    {
      if(showLCD){
        currentState = SETTING_ALARM;
        settingAlarmProcess = A_HOUR;
      } else {
        showLCD = true;
      }

    }
    break;
    case SETTING_ALARM:
    {
      if(settingAlarmProcess == A_HOUR){
         settingAlarmProcess = A_MINUTE;
      } else if(settingAlarmProcess == A_MINUTE){
         settingAlarmProcess = A_TYPE;
      } else {
        currentState = NORMAL;
      }

    }
    break;
    case SETTING_TIME:
    {
      if(settingTimeProcess == T_HOUR){
         settingTimeProcess = T_MINUTE;
      } else if(settingTimeProcess == T_MINUTE){
         settingTimeProcess = T_SECOND;
      } else if(settingTimeProcess == T_SECOND){
         settingTimeProcess = T_DAY;      
      } else if(settingTimeProcess == T_DAY){
        settingTimeProcess = T_MONTH; 
      } else if(settingTimeProcess == T_MONTH){
         settingTimeProcess = T_YEAR;           
      } else {
        currentState = NORMAL;
      }
      
    }
    break;
    default: break;
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
    case ALARM: stopAlarm();
    break;
    case NORMAL :
   { 
      if(showLCD){
        currentState = SETTING_TIME;
        settingTimeProcess = T_HOUR;
      } else {
        showLCD = true;
      }
      
   }
    break;
    case SETTING_ALARM:
    {
        if(settingAlarmProcess == A_HOUR){
          alarmHour = getNextHourFromCurrentHour(alarmHour, true);
        } else if(settingAlarmProcess == A_MINUTE){
          alarmMinute = getNextMinSecFromCurrentMinSec(alarmMinute, true);
        } else if(settingAlarmProcess == A_TYPE){
              
          if(!vibrate1 && !vibrate2){
                vibrate1 = false;
                vibrate2 = true;
              } else if(!vibrate1 && vibrate2){
                vibrate1 = true;
                vibrate2 = false;
              } else if(vibrate1 && !vibrate2){
                vibrate1 = true;
                vibrate2 = true;  
              } else {
                vibrate1 = false;
                vibrate2 = false;  
              }
        }
    }
    break;
    case SETTING_TIME:
      {
    DateTime now = RTC.now();
    DateTime newTime;
    switch(settingTimeProcess){
      case T_HOUR:
        newTime = DateTime(now.year(), now.month(), now.day(), getNextHourFromCurrentHour(now.hour(), true), now.minute(), now.second());
        break;
      case T_MINUTE:
        newTime = DateTime(now.year(), now.month(), now.day(), now.hour(), getNextMinSecFromCurrentMinSec(now.minute(), true), now.second());
        break;
      case T_SECOND:
        newTime = DateTime(now.year(), now.month(), now.day(), now.hour(), now.minute(), getNextMinSecFromCurrentMinSec(now.second(), true));
        break;
      case T_DAY:
        newTime = DateTime(now.year(), now.month(), now.day() + 1, now.hour(), now.minute(), now.second());
        break;
      case T_MONTH:
      {
        int month = now.month();
        if(month == 12){
          month = 1;
        } else {
          month++;
        }
        newTime = DateTime(now.year(), month, now.day(), now.hour(), now.minute(), now.second());
      }
        break;
      case T_YEAR:
        newTime = DateTime(now.year() + 1, now.month(), now.day(), now.hour(), now.minute(), now.second());
        break;
      default: break;
     
    }
       
    RTC.adjust(newTime);
  }
    
    
    break;
    default: break;
  }

}



void checkAndSoundAlarm(int hour, int minute){


}


void soundAlarmAtThisPointIfNeeded(){



}



void stopAlarm(){
}

void writeTimeToDisplayBuffer(DateTime now, boolean blinkOn, boolean secondsBlinkOn){
  uView.setCursor(0,0);
  uView.setFontType(7);
  
  String timeString = generateTimeString(now.hour(), now.minute(), secondsBlinkOn);
  uView.println(timeString);
}

String generateTimeString(int hour, int minute, boolean blinkOn){
  char buff[3];
  
  String hourString;
  
  if(currentState == SETTING_TIME && settingTimeProcess == T_HOUR && !blinkOn){
    hourString = "  ";
  } else {
    sprintf(buff, "%02d", hour);
    hourString = buff;
  }

  String minuteString;
  if(currentState == SETTING_TIME && settingTimeProcess == T_MINUTE && !blinkOn){
    minuteString = "  ";
  } else {
    sprintf(buff, "%02d", minute);
    minuteString = buff;
  }
  
  String secondString;
  
 if(blinkOn){
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
  uView.print(alarmHour);
  uView.print(":");
  uView.print(alarmMinute);
  uView.print(" Vib");

}


void writeVoltageToDisplayBuffer(int batteryMilliVolt){
  uView.setFontType(0);
  uView.setCursor(0,32);


  float voltage = (float) batteryMilliVolt / 1000;

  char buff[6];
  String voltageString = dtostrf(voltage, 4, 2, buff);

  uView.print(voltageString);
  uView.print("V");

  int batteryRange = MAX_BATTERY_MILLIVOLT - MIN_BATTERY_MILLIVOLT;

  //Cast to long as Arduino int is only 16 bits wide. Not enough to hold this result
  long numerator =  ((long) ((batteryMilliVolt - MIN_BATTERY_MILLIVOLT))) * 100;
  int batteryPercent = numerator / batteryRange;

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
  uView.print("Led Alm Ti");
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
  } else {
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
  } else {
    change = -1;
  }
  
  int newValue = current + change;
  
  //To produce positive modulo result
  return (newValue % 60 + 60) % 60;
}









