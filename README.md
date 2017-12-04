# MailboxNotifiyv2.0

For more see: https://adambyers.com/2017/11/mailbox-notifier-v2-0

This is the code my mailbox delivery notification system. 

The ATTiny85 was incorporated into the system to control the notification LEDs on the mailbox and to reset (i.e. wake up) the ESP8266 so it would run its email delivery routine.

The ESP8266 is capable of all the functions the ATTiny85 is preforming except that you cannot set a PIN state to HIGH and have it remain HIGH while the ESP8266 is in deepSleep. This is untenable because I want to turn on the mailbox notification LEDs and then put the MCU back to sleep to save power. 

Also, the ESP8266 can only be woken up from deepSleep by a watchdog timer (not usable for this purpose) or by reseting the MCU. When in deepSleep there are no external interrupts that can be used to wake the MCU, like on the Arduino class of MCUs.

So, I used the ATTiny85 to monitor the delivery/retrieval status, control the notification LEDs, and to reset the ESP8266 when required. The ESP8266 is relegated to the task of reading the sensors (battery level, temperature, date/time) and sending the delivery notification email.

The entire setup, including solar charge controller, uses only 0.4mA when sleeping. At its most active (WiFi is in use) the system peaks at 126mA, but this is very brief. When the system is sleeping, but the notification LEDs are on, usage is 48.7mA.

In my setup I have the space for, and am using a 4400mAh LiPo battery which is recharged via solar. Even at its peek usage (126mA), the system would run on this cell for +/-24 hours. 

I actually didn't have to be so fussy with minimizing power usage but it was a fun and informative exercise.

The sleep code for the ATTiny85 was lifted from Nick Gammon - https://www.gammon.com.au/power

The ESP8266 (a Sparkfun ESP8266 Thing Dev in my case) is used to get sensor readings and send a mail delivery notification. Most of the code here was pulled from various Arduino examples. 

I didn't want to rely on external services to send notifications. After much finagling, I found that using my own internal SMTP server (Postfix, not publicly exposed, it relays through Gmail) was the simplest. If you've ever sent an email by telnetting into an SMTP server, this will look familiar to you. 

All functionality occurs during setup, the Loop is empty. After running the necessary functions to read sensors, connect to wifi, and send the email, the ESP8266 goes into deepSleep indefinitely. It's woken up by a reset preformed by an (external) ATTiny85.

In testing I found that the ESP8266 has issues re-connecting to wifi after waking up (i.e. being reset). To remedy this, when the ESP8266 wakes up it immediately disconnects from wifi - WiFi.disconnect(); - and then reconnects when necessary. Also, the ESP8266 connects a tiny bit faster when you give it a static IP, vs. waiting for a DHCP lease. A static IP may not be suable for all environments, however.

The ADC in the ESP8266 is tricky (inaccurate?) which is why I map the reading - BatteryPINreading = map(BatteryPINreading, 0, 934, 0, 100); - to 934 to reflect a full battery, rather than closer to the 1023 it should be given that I'm feeding it near the 1v max input. On the Adafruit ESP8266 Huzzah the readings were even more off. This seems to be a known issue with the ESP8266 so the best solution is to read the values your ADC is giving you and parse them apropreatly.
