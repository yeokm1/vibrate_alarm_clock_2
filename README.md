vibrate_alarm_clock_2
===================

An alarm clock with vibration, battery monitoring and wireless charging capabilities.  Buttons available to set the alarm, turn off OLED and reset.
This device is meant for my friend's birthday gift. As you can tell from the name, this is the second one. It has been slightly streamlined compared to the [first](https://github.com/yeokm1/vibrate_alarm_clock).

<a href="/misc/front-assembled.jpg"><img src="/misc/front-assembled.jpg" align="centre" height="480" width="480" ></a>

Front, fully assembled.

<a href="/misc/side-not-charging.jpg"><img src="/misc/side-not-charging.jpg" align="centre" height="399" width="480" ></a>

Side view with top open. A Qi wireless charger in the background. Not so messy this time.

##Parts used
1. Microview with 64x48 OLED screen (based on ATmega328P chip)  
2. Chronodot v2.1 Real Time Clock (based on DS3231 temperature compensated RTC crystal)
3. Adafruit PowerBoost 500 Charger
4. 3.3V voltage regulator (LD1117V33)
5. 4x button switches (1 switch as reset button)  
6. Pololu DRV8833 dual motor driver  
7. 500mA Qi wireless receiver module
8. 10mm vibration motor
9. 4400mAh Lithium battery

Others:  

1. 1x half-breadboard 
2. 4x Mini-breadboard (2x8 points)
3. Translucent case  

![Screen](/misc/schematic.png)

This schematic only represents the logical connections I made. The physical connections differs due to space issues.
Closest components used as Fritzing does not have them.

1. MD030A as DRV8833 motor driver. (Pin 6 on the Microview is connected to the sleep pin on the DRV8833.)
2. RTC module as Chronodot
3. The battery is a cylindrical style 4400mAh one.

##Differences with version 1 (v1)
1. No more pull-down resistors for the switches. I use the internal pull-up resistors of the Arduino by setting pinmode to INPUT_PULLUP.
2. Battery capacity: The battery size was an afterthought in V1. The battery size is now more than 4x larger.
3. Power consumption: Reduced by putting the Arduino to sleep and only waking via alarm interrupts from the Chronodot or button press. Battery life is expected to be about 5-6 days.
4. The Pololu DRV8833 motor driver is powered behind a 3.3V regulator as 5V is too high for the 10mm vibration motor.

##Notes
1. The Analog to Digital Converter (ADC) of the Microview is rather unstable. As the battery voltage is retrieved via the ADC, I had to do a running average to get a stable value to display.

##References and libraries
1. [Creating fonts for Microview](http://learn.microview.io/font/creating-fonts-for-microview.html)
2. [DS3232 RTC library](https://github.com/JChristensen/DS3232RTC)
3. [Rocketscream Low Power library](http://www.rocketscream.com/blog/2011/07/04/lightweight-low-power-arduino-library/)
