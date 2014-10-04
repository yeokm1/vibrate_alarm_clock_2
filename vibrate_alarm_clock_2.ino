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
    Serial.println(vccVoltage);
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
  //  // subtract the last reading:
  //  total= total - readings[index];         
  //  // read from the sensor:  
  //  readings[index] = analogRead(VOLTAGE_MEASURE_PIN); 
  //  Serial.println(readings[index]);
  //  // add the reading to the total:
  //  total= total + readings[index];       
  //  // advance to the next position in the array:  
  //  index = index + 1;                    
  //
  //  // if we're at the end of the array...
  //  if (index >= numReadings)              
  //    // ...wrap around to the beginning: 
  //    index = 0;                           
  //
  //  // calculate the average:
  //  average = total / numReadings;         
  //  // send it to the computer as ASCII digits
  //
  //    return average;  
  return analogRead(VOLTAGE_MEASURE_PIN); 
}
void processLeftButtonPressed(){
  unsigned long currentMillis = millis();

  if((currentMillis - timeLastPressedLeftButton) < MIN_TIME_BETWEEN_BUTTON_PRESSES){
    return;
  }

  timeLastPressedLeftButton = currentMillis;

  Serial.println("Left Button Pressed");


}

void processMiddleButtonPressed(){
  unsigned long currentMillis = millis();


  if((currentMillis - timeLastPressedMiddleButton) < MIN_TIME_BETWEEN_BUTTON_PRESSES){
    return;
  }

  timeLastPressedMiddleButton = currentMillis;

  Serial.println("Middle Button Pressed");


}

void processRightButtonPressed(){
  unsigned long currentMillis = millis();


  if((currentMillis - timeLastPressedRightButton) < MIN_TIME_BETWEEN_BUTTON_PRESSES){
    return;
  }

  timeLastPressedRightButton = currentMillis;

  Serial.println("Right Button Pressed");


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

  uView.print(now.hour());
  if(secondsBlinkOn){
    uView.print(":");
  } 
  else {
    uView.print(" ");
  }


  uView.println(now.minute());



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









