/*
This is the code for the ESP8266 portion of my v2.0 mailbox delivery notification system. 

Note: I've removed all the unnecessary serial output that was used for debugging. 

The ESP8266 (a Sparkfun ESP8266 Thing Dev in my case) is used to get sensor readings and send a mail delivery notification. Most of the code here was pulled from various Arduino examples. 

I didn't want to rely on external services to send notifications. After much finagling, I found that using my own internal SMTP server (Postfix, not publicly exposed, it relays through Gmail) was the simplest. If you've ever sent an email by telnetting into an SMTP server, this will look familiar to you. 

All functionality occurs during setup, the Loop is empty. After running the necessary functions to read sensors, connect to wifi, and send the email, the ESP8266 goes into deepSleep 
indefinitely. It's woken up by a reset preformed by an (external) ATTiny85.

In testing I found that the ESP8266 has issues re-connecting to wifi after waking up (i.e. being reset). To remedy this, when the ESP8266 wakes up it immediately disconnects from 
wifi - WiFi.disconnect(); - and then reconnects when necessary. Also, the ESP8266 connects a tiny bit faster when you give it a static IP, vs. waiting for a DHCP lease. A static IP may not be
suable for all environments, however. 

The entire setup, including solar charge controller, uses only 0.4mA when sleeping. At its most active (WiFi is in use) the system peaks at 126mA, but this is very brief. When the system is 
sleeping, but the notification LEDs are on, usage is 48.7mA.

The ADC in the ESP8266 is tricky (inaccurate?) which is why I map the reading - BatteryPINreading = map(BatteryPINreading, 0, 934, 0, 100); - to 934 to reflect a full battery, rather than 
closer to the 1023 it should be given that I'm feeding it near the 1v max input. On the Adafruit ESP8266 Huzzah the readings were even more off. This seems to be a known issue with the 
ESP8266 so the best solution is to read the values your ADC is giving you and parse them apropreatly. 
*/

// Libraries
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// PIN Names
const int Battery = A0;
const int DSTsw = 14;

// Wireless setup
IPAddress ip(192, 168, 1, 2); // The ESP connects a bit faster with a static IP vs DHCP. Set to match your network.
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 1, 1);
WiFiUDP Udp;
unsigned int localPort = 8888;
WiFiClient client;

const char* SSID = "YOUR SSID HERE";
const char* PASS = "YOUR WIRELESS PASS HERE";
const char SMTPserver[] = "YOUR SMTP SERVER ADDRESS HERE";

// NTP setup
static const char ntpServerName[] = "YOUR NTP SERVER NAME HERE";
int timeZone = -8; // -8 = PST -7 = PDT
time_t getNtpTime();
void sendNTPpacket(IPAddress &address);

// DS18B20 setup
#define tempSensor 2 // PIN the DS18B20 is connected to.
OneWire oneWire(tempSensor);
DallasTemperature sensors(&oneWire); // Pass the OneWire reference to the DS18B20.

// Mis vars
int BatteryPINreading = 0; // Holds the raw reading from the battery monitor pin.
float tempC = 0; // Temp reading in Celsius.
float tempF = 0; // Temp in Fahrenheit.

void setup() {
  WiFi.disconnect(); // In testing I found that the ESP8266 has issues reconnecting to WiFi after waking up (being reset) this solves that.

  pinMode(Battery, INPUT);
  pinMode(DSTsw, INPUT);

  digitalWrite(DSTsw, HIGH); 

  if (digitalRead(DSTsw) == HIGH) {
    timeZone = -7;
  }

  sensors.begin(); // Start the DallasTemperature library.

  // Get the battery level.
  BatteryPINreading = analogRead(Battery);
  BatteryPINreading = map(BatteryPINreading, 0, 934, 0, 100);

  // Get the temp.
  sensors.requestTemperatures(); // Tell the sensor to record the temp to its memory.
  tempC = sensors.getTempCByIndex(0); // Retrieve the temp from the sensor's memory and store it in a var.
  tempF = (tempC * 9.0) / 5.0 + 32.0; // Convert to Fahrenheit.

  // Connect to wifi.
  WiFi.mode( WIFI_STA );
  WiFi.config(ip, gateway, subnet, dns);
  WiFi.begin(SSID, PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Udp.begin(localPort);
  setSyncProvider(getNtpTime);

  byte ret = sendEmail();

  ESP.deepSleep(0);
}

void loop() {
  // Do nothing.
}


byte sendEmail() {
  byte thisByte = 0;
  byte respCode;

  if (client.connect(SMTPserver, 25) == 1) {
  }

  else {
    return 0;
  }

  if (!eRcv()) return 0;
  client.println("EHLO smtp.server.local"); // FQDN of your SMTP server. 
  if (!eRcv()) return 0;
  client.println(F("MAIL From: senderaddress@domain.com"));// Sender address.
  if (!eRcv()) return 0;
  client.println(F("RCPT To: recptaddress@domain.com")); // Who to send it to.
  if (!eRcv()) return 0;
  client.println(F("DATA"));
  if (!eRcv()) return 0;
  client.println(F("From: senderaddress@domain.com"));
  client.println(F("Subject: Snail Mail Delivery Detected!\r\n"));
  client.print(F("Date: "));
  client.print (month());
  client.print ("/");
  client.print (day());
  client.print ("/");
  client.println (year());
  client.print(F("Time: "));
  client.print (hour());
  client.print(":");
  // Add trailing zero to time if necessary.
  if (minute() < 10) {
    client.print ("0");
  }
  client.print (minute());
  client.print(":");
  if (second() < 10) {
    client.print ("0");
  }
  client.println (second());
  client.print(F("Temperature: "));
  client.println (tempF);
  client.print(F("Battery Level: "));
  client.print (BatteryPINreading);
  client.println ("%");
  client.println(F("."));

  if (!eRcv()) return 0;
  client.println(F("QUIT"));
  if (!eRcv()) return 0;
  client.stop();
  return 1;
}

byte eRcv() {
  byte respCode;
  byte thisByte;
  int loopCount = 0;

  while (!client.available()) {
    delay(1);
    loopCount++; // If nothing received for 10 seconds, timeout.

    if (loopCount > 10000) {
      client.stop();
      return 0;
    }
  }

  respCode = client.peek();

  while (client.available()) {
    thisByte = client.read();
    Serial.write(thisByte);
  }

  if (respCode >= '4') {
    return 0; // Fail.
  }
  return 1;
}

// NTP code

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message.
byte packetBuffer[NTP_PACKET_SIZE]; // Buffer to hold incoming & outgoing packets.

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address.

  while (Udp.parsePacket() > 0) ; // Discard any previously received packets.
  WiFi.hostByName(ntpServerName, ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // Read packet into the buffer.
      unsigned long secsSince1900;
      // Convert four bytes starting at location 40 to a long integer.
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  return 0; // Return 0 if unable to get the time.
}

// Send an NTP request to the time server at the given address.
void sendNTPpacket(IPAddress &address)
{
  // Set all bytes in the buffer to 0.
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request.
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode.
  packetBuffer[1] = 0;     // Stratum, or type of clock.
  packetBuffer[2] = 6;     // Polling Interval.
  packetBuffer[3] = 0xEC;  // Peer Clock Precision.
  // 8 bytes of zero for Root Delay & Root Dispersion.
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // All NTP fields have been given values, now.
  // You can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123.
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
