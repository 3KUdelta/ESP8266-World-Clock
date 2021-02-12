
/*----------------------------------------------------------------------------------------------------
  Project Name : World Clock on an ESP8266
  Features: Accurate time taken from NTP servers, automatically adjusted timezones and daylight saving times,
  display switch off after 2 minutes if no movement, automatic brightness adjustment, RTC support if no WiFi available
  Author: Marc Stähli / Dec 2020

  Hardware Settings Mac:
  LOLIN(WEMOS) D1 mini, standard settings

  updated 28.12.20
  - fixed the bug if there is no WiFi
  - go_online() calls also get_NTP_time() --> leaving go_online() as the main function for updates

  updated 12. 02.21
  - sometimes call to NTP returns wrong values --> algorithm added to remain on RTC values

  /***************************************************
   VERY IMPORTANT:
 *                                                 *
   Enter your personal settings in Settings.h !
 *                                                 *
 **************************************************/

#include "Settings.h"

#include <Arduino.h>
#include <TM1637Display.h>                  // https://github.com/avishorp/TM1637
#include <ESP8266WiFi.h>                    // WiFI
#include <WiFiUdp.h>                        // For NTP Signal fetch
#include <EasyNTPClient.h>                  // For NTP Signal read https://github.com/aharshac/EasyNTPClient
#include <TimeLib.h>                        // For converting NTP time https://github.com/PaulStalloffregen/Time.git
#include "RTClib.h"                         // standard Arduino library handling ds3231 RTC module
#include <Wire.h>                           // I2C connection to ds3231

unsigned int LDR_Pin = A0;                  // Data pin for light sensor

DateTime now_3231;                          // RTC variable
time_t SYD_time, ZRH_time, LON_time, SIN_time;
unsigned long timestamp, epochtime_3231, epochtime_ntp, timebuffer;
unsigned int raw_LDR;
unsigned int brightness = 1;
unsigned int counter = 0;

// Variables for smoothening the analog read (light sensor)
const int numReadings = 8;      // number of readings for running average
int readings[numReadings];      // the readings from the analog input
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average

// Variable for Segment display (predefined words/chars)
const uint8_t SEG_GOIN[] = {
  SEG_A | SEG_B | SEG_C |  SEG_D | SEG_F | SEG_G,  // g
  SEG_C | SEG_D | SEG_E | SEG_G,                   // O
  SEG_C,                                           // I
  SEG_C | SEG_E | SEG_G                            // N
};
const uint8_t SEG_ONLN[] = {
  SEG_C | SEG_D | SEG_E | SEG_G,                   // o
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_E | SEG_F,                                   // l
  SEG_C | SEG_E | SEG_G                            // n
};
const uint8_t SEG_LAN[] = {
  SEG_D | SEG_E | SEG_F,                           // L
  SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G,   // A
  SEG_C | SEG_E | SEG_G                            // n
};
const uint8_t SEG_ERR[] = {
  SEG_A | SEG_D | SEG_E | SEG_F | SEG_G,           // E
  SEG_E | SEG_G,                                   // r
  SEG_E | SEG_G                                    // r
};
const uint8_t SEG_RTC[] = {
  SEG_E | SEG_G,                                   // r
  SEG_D | SEG_E | SEG_F | SEG_G,                   // t
  SEG_D | SEG_E | SEG_G                            // c
};
const uint8_t SEG_GOOD[] = {
  SEG_A | SEG_B | SEG_C |  SEG_D | SEG_F | SEG_G,  // g
  SEG_C | SEG_D | SEG_E | SEG_G,                   // o
  SEG_C | SEG_D | SEG_E | SEG_G,                   // o
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G            // d
};
const uint8_t SEG_NTP[] = {
  SEG_C | SEG_E | SEG_G,                           // n
  SEG_D | SEG_E | SEG_F | SEG_G,                   // t
  SEG_A | SEG_B | SEG_E | SEG_F | SEG_G            // p
};
const uint8_t alloff[] = {0x00, 0x00, 0x00, 0x00}; // all lights off

// Initialize the four displays
TM1637Display zurich(CLK, ZRH);                    // CLK and ZRH data pins are defined in Settings.h
TM1637Display london(CLK, LON);                    // CLK and LON data pins are defined in Settings.h
TM1637Display singapore(CLK, SIN);                 // CLK and SIN data pins are defined in Settings.h
TM1637Display sydney(CLK, SYD);                    // CLK and SYD data pins are defined in Settings.h

WiFiUDP udp;                                       // WiFi UDP initialization for NTP
EasyNTPClient ntpClient(udp, NTP_SERVER, TZ_SEC);  // NTP initialization

RTC_DS3231 rtc_3231;                               // ds3231 RTC initialization

void setup() {

  Serial.begin(115200); while (!Serial); delay(200);
  Serial.printf("Starting WorldClock...\r\n");

  Wire.begin(4, 5);                                 // ensuring read from SLA,SLC ports 4 and 5 on ESP8266

  // initialize all the readings to 0  --> needed to smoothen analog read to avoid brightness flickering
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }

  // Initialize brighness
  zurich.setBrightness(brightness + 1);             // Yellow color needs more brightness as the white ones
  london.setBrightness(brightness);
  singapore.setBrightness(brightness);
  sydney.setBrightness(brightness);

  // Clear the display:
  zurich.clear();
  london.clear();
  singapore.clear();
  sydney.clear();

  if (! rtc_3231.begin()) {
    Serial.println("Couldn't find RTC");
    london.setSegments(SEG_ERR, 3, 0);
    singapore.setSegments(SEG_RTC, 3, 0);
    delay(3000);
    singapore.clear();
    london.clear();
  }
  else {
    Serial.println("RTC found");
    london.setSegments(SEG_RTC, 3, 0);
    singapore.setSegments(SEG_GOOD, 4, 0);
    delay(3000);
    singapore.clear();
    london.clear();
  }

  now_3231 = rtc_3231.now();              // reading time from 3231 RTC
  epochtime_3231 = now_3231.unixtime();   // storing RTC time in variable epochtime_3231
  setTime(epochtime_3231);                // in case of no WiFi we initialize the ESP8266 rtc with the values from 3231

  go_online();                            // go online and try to get NTP time

  timestamp = now();                      // initialize timestamp

  pinMode(RADARSENSOR, INPUT);
}

void loop() {

  if (digitalRead(RADARSENSOR) == 1) {                   // Radarsensor activated --> timer gets started
    timebuffer = millis();
    //Serial.println("Sensor active");
  }

  if (millis() - timebuffer < 120000) {                  // After movement, display remains on for 2 minutes = 120000 ms

    //Serial.println("Timebuffer active");

    total = total - readings[readIndex];
    readings[readIndex] = analogRead(LDR_Pin);
    total = total + readings[readIndex];
    readIndex = readIndex + 1;
    if (readIndex >= numReadings) {
      readIndex = 0;
    }

    raw_LDR =  average = total / numReadings;

    counter++;

    if (raw_LDR < 250 && counter > 10) {                   // introducing a counter to avoid flickering
      brightness = 1;
      counter = 0;
    }
    if (raw_LDR >= 250 && raw_LDR < 900 && counter > 10) {
      brightness = 2;
      counter = 0;
    }
    if (raw_LDR >= 900 && counter > 10) {
      brightness = 3;
      counter = 0;
    }

    zurich.setBrightness(brightness + 1);
    london.setBrightness(brightness);
    singapore.setBrightness(brightness);
    sydney.setBrightness(brightness);

    LON_time = UK.toLocal(now(), &tcr);
    ZRH_time = CE.toLocal(now(), &tcr);
    SYD_time = ausET.toLocal(now(), &tcr);
    SIN_time = SINGA.toLocal(now(), &tcr);

    int londontime = hour(LON_time) * 100 + minute(LON_time) + 10000;   // +1000 is for setting leading zeros
    int zurichtime = hour(ZRH_time) * 100 + minute(ZRH_time) + 10000;
    int singaporetime = hour(SIN_time) * 100 + minute(SIN_time) + 10000;
    int sydneytime = hour(SYD_time) * 100 + minute(SYD_time) + 10000;

    zurich.showNumberDecEx(zurichtime, 0b01000000, false, 4, 0);
    london.showNumberDecEx(londontime, 0b01000000, false, 4, 0);
    singapore.showNumberDecEx(singaporetime, 0b01000000, false, 4, 0);
    sydney.showNumberDecEx(sydneytime, 0b01000000, false, 4, 0);

    delay(500);

    zurich.showNumberDecEx(zurichtime, true);     // make : blinking every half a second on Zurich time

    delay(500);

    if ((now() - timestamp) > NTPUPDATE) {
      go_online();                               // get all x hours (NTPUPDATE in Settings.h) a time update from NTP Server --> avoiding a constant read from time server
      timestamp = now();                         // reset timestamp
    }
  }
  else {
    Serial.println("Display alloff");
    zurich.setSegments(alloff, 4, 0);             // if no movement on Radarsensor, display shut alloff
    london.setSegments(alloff, 4, 0);             // if no movement on Radarsensor, display shut alloff
    singapore.setSegments(alloff, 4, 0);          // if no movement on Radarsensor, display shut alloff
    sydney.setSegments(alloff, 4, 0);             // if no movement on Radarsensor, display shut alloff
  }
}

void go_online() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  WiFi.hostname("WorldClock");
  Serial.print("---> Connecting to WiFi ");
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (i++ > 10) {
      Serial.println("Could not connect to WiFi!");
      zurich.clear();
      london.clear();
      singapore.clear();
      sydney.clear();
      london.setSegments(SEG_ERR, 3, 0);
      singapore.setSegments(SEG_LAN, 3, 0);
      delay(3000);
      zurich.clear();
      london.clear();
      singapore.clear();
      sydney.clear();
      return;                       // if no WiFi, go back to loop and try to connect again at the next given time
    }
    london.setSegments(SEG_GOIN);
    singapore.setSegments(SEG_ONLN);
    Serial.print(".");
  }
  Serial.println(" Wifi connected ok");
  zurich.clear();
  london.clear();
  singapore.clear();
  sydney.clear();
  get_NTP_time();
}

void get_NTP_time() {

  Serial.println("---> Now reading time from NTP Server");

  int inc = 0;
  while (!ntpClient.getUnixTime()) {
    delay(100);
    if (inc++ > 10) {
      Serial.println("Could not connect to NPT!");
      zurich.clear();
      london.clear();
      singapore.clear();
      sydney.clear();
      london.setSegments(SEG_ERR, 3, 0);
      singapore.setSegments(SEG_NTP, 3, 0);
      delay(3000);
      zurich.clear();
      london.clear();
      singapore.clear();
      sydney.clear();
      return;                       // if no NTP, go back to loop and try to connect again within the next given time
    }
    london.setSegments(SEG_GOIN);
    singapore.setSegments(SEG_NTP, 3, 0);
    Serial.print(".");
  }
  epochtime_ntp = ntpClient.getUnixTime(); //get UNIX timestamp (seconds from 1.1.1970)
 
  /* sometimes there are reading errors from NTP, 
     time is only updated if within a difference of 30 seconds, 
     else stored time in RTC3231 shall remain */
  if (timedifference(epochtime_3231, epochtime_ntp) < 30) {  
    setTime(epochtime_ntp);                  // set systemtime in ESP8266 to UTC
    rtc_3231.adjust(DateTime(year(now()), month(now()), day(now()), hour(now()), minute(now()), second(now()))); // set UTC to 3231 RTC
  }

} // end get_NTP_time()

int timedifference(time_t timeone, time_t timetwo) {
  int t_diff = 0;
  if (timeone > timetwo) {
    t_diff = timeone - timetwo;
    return t_diff;
  }
  if (timeone < timetwo) {
    t_diff = timetwo - timeone;
    return t_diff;
  }
} // end timedifference()
