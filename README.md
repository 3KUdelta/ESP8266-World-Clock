# ESP8266-World-Clock
Highly accurate world clock with automatic global daylight saving time settings.

Author: Marc Staehli, initial upload Nov 2020

[![ESP8266-World-Clock](https://github.com/3KUdelta/ESP8266-World-Clock/blob/main/pics/IMG_20201108_143037.jpg)](https://github.com/3KUdelta/ESP8266-World-Clock)


Build:
- WEMOS D1 mini (ESP8266) - using WiFi to get NTP Time
- 4 4-digit,7-segment LED display for showing 4 individual time zones
- 1 RTC 3231 unit for keeping the accurate time if there is no WIFI available
- 1 Radar sensor (RCWL-0516) for switching on for 2 minutes if there is movement in the room (hidden sensor)
- LDR sensor for adjusting bightness of LEDs

Settings.h includes all individal settings. 
Including <Timezone.h> for calculating the right timezone including global DST settings (https://github.com/JChristensen/Timezone).

Print the box yourself: (https://www.thingiverse.com/thing:4655531)

Hint: You cannot power the LED segment displays off the 5v output pin from the ESP8266, the output has not enough Amps. Instead get the +5v from a solder point directly from the board behind the micro USB plug (see pictures and use volt meter to find correct solder point).

[![ESP8266-World-Clock](https://github.com/3KUdelta/ESP8266-World-Clock/blob/main/pics/WorldClock_Schematic.png)](https://github.com/3KUdelta/ESP8266-World-Clock)

[![ESP8266-World-Clock](https://github.com/3KUdelta/ESP8266-World-Clock/blob/main/pics/IMG_20201108_141833.jpg)](https://github.com/3KUdelta/ESP8266-World-Clock)

[![ESP8266-World-Clock](https://github.com/3KUdelta/ESP8266-World-Clock/blob/main/pics/IMG_20201108_141814.jpg)](https://github.com/3KUdelta/ESP8266-World-Clock)
