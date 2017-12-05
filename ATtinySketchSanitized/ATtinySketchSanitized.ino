/*
This is the code for the ATtiny85 portion of my v2.0 mailbox delivery notification system. 

The ATtiny85 was incorporated into the system to control the notification LEDs on the mailbox and to reset (i.e. wake up) the ESP8266 so it would run its email delivery routine.

The ESP8266 is capable of all the functions the ATtiny85 is preforming except that you cannot set a PIN state to HIGH and have it remain HIGH while the ESP8266 is in deepSleep. This is 
untenable because I want to turn on the mailbox notification LEDs and then put the MCU back to sleep to save power. 

Also, the ESP8266 can only be woken up from deepSleep by a watchdog timer (not usable for this purpose) or by reseting the MCU. When in deepSleep there are no external interrupts that can 
be used to wake the MCU, like on the Arduino class of MCUs.

So, I used the ATtiny85 to monitor the delivery/retrieval status, control the notification LEDs, and to reset the ESP8266 when required. The ESP8266 is relegated to the task of reading the 
sensors (battery level, temperature, date/time) and sending the delivery notification email.

The entire setup, including solar charge controller, uses only 0.4mA when sleeping. At its most active (WiFi is in use) the system peaks at 126mA, but this is very brief. When the system is 
sleeping, but the notification LEDs are on, usage is 48.7mA. 

In my setup I have the space for, and am using a 4400mAh LiPo battery which is recharged via solar. Even at its peek usage (126mA), the system would run on this cell for +/-24 hours. I 
actually didn't have to be so fussy with minimizing power usage but it was a fun and informative exercise.

The sleep code was lifted from Nick Gammon - https://www.gammon.com.au/power
*/

// Libraries
#include <avr/sleep.h>
#include <avr/power.h>

// PIN Names
const int DeliverySW = 0;
const int RetrieveSW = 1;
const int ESPrelay = 2;
const int LEDrelay = 3;
const int DomeLED = 4;

bool Delivery = false;

void setup() {
  pinMode(DeliverySW, INPUT);
  pinMode(RetrieveSW, INPUT);
  pinMode(LEDrelay, OUTPUT);
  pinMode(ESPrelay, OUTPUT);
  pinMode(DomeLED, OUTPUT); 
  
  digitalWrite(DeliverySW, HIGH); // Enable the internal pull-up.
  digitalWrite(RetrieveSW, HIGH); // Enable the internal pull-up.
  
  PCMSK  |= bit (PCINT0);  // Enable D0, PIN0 for interrupt.
  PCMSK  |= bit (PCINT1);  // Enable D1, PIN1 for interrupt.
  GIFR   |= bit (PCIF);    // Clear existing interrupts.
  GIMSK  |= bit (PCIE);    // Enable PIN change interrupts.

  // Flash the notification LEDs so we know we're booting ok.
  for (int t = 0; t < 20; t = t + 1) {
    if (t % 2 == 0) {
      digitalWrite(LEDrelay, HIGH);
    }
    else {
      digitalWrite(LEDrelay, LOW);
    }
    delay(250);
  }
}

void loop() {
  F_Sleep();
}


void F_Sleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = 0;            // Turn off the ADC, we don't need it when sleeping.
  power_all_disable ();  // Turn off the ADC, timer 0 and 1, and serial interface.

  sleep_enable();

  sleep_cpu(); // Sleep.

  sleep_disable(); // We resume from here when woken up.

  power_all_enable(); // Turn the the ADC, timer 0 and 1, and serial interface back on.

  F_Process();
}

void F_Process() {
  if (digitalRead(DeliverySW) == HIGH && Delivery == false) { // Only run if mail has not been delivered yet. Otherwise, go back to sleep.
    digitalWrite(LEDrelay, HIGH); // Turn on the external notification LEDs.
    digitalWrite(ESPrelay, HIGH); // Reset the ESP8266 so it'll send the notification email.
    delay(500);
    digitalWrite(ESPrelay, LOW); // Reset the ESP8266.

    Delivery = true; // Set this so we know the mail has been delivered and don't run the delivery sequence again before the retrieve door is opened.
  }

  if (digitalRead(RetrieveSW) == HIGH && Delivery == true) { // Only run if mail has been delivered yet. Otherwise, go back to sleep.
    digitalWrite(LEDrelay, LOW); // Turn OFF the external notification LEDs.
    // Turn ON the dome LED on until the retrieve door is closed so we can see inside the mailbox if it's dark outside.
    while (digitalRead(RetrieveSW) == HIGH) {
      digitalWrite(DomeLED, HIGH);
    }

    digitalWrite(DomeLED, LOW); // Turn the dome LED off.

    Delivery = false; // Set this back to false so we get notified when mail is delivered again.
  }
}

ISR (PCINT0_vect) {
  // Noting to see here.
}

ISR (PCINT1_vect) {
  // Noting to see here.
}
