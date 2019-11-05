/* Demo for the 74HC165 shift register chip that allows multiple INPUT pins (switches)
 * to be used on the standard SPI bus, thus saving pins on the Arduino ATMega328P.
 *
 * Connection details:
 *
 * See video #161 www.youtube.com/ralphbacon which has full details.
 *
 * 74HC165		Arduino Uno Pin
 * ---------	--------------------
 * 	1 Shift/Load see PULSE pin below
 * 	2 CLK		SPI CLK Pin D13
 * 	9 QH		SPI MOSI Pin D12
 * 	16			VCC (5V) Works with 2-5V Absolute MAX 6V
 * 	8			GND
 * 	15			GND or SS pin of your choice (default 10) goes LOW to enable
 *
 * Additionally (MUST be kept in this order)
 * 	11 - 14		DIP Switches A - D
 *  3 - 6		DIP Switches E - H
 *
 *  SPI data will return a single byte with each bit representing a single switch condition.
 *  	76543210 - bits
 *  eg 	00001001 means switches 1 (far right) and 4 (middle) are set (ON)
 *
 */

#include <SPI.h>
#include "sketch.h"
#include "timer.h"
#include <avr/power.h>

// We will pulse the 74HC165 on this pin to load the new config
// When LOW data is loaded from parallel inputs (see data sheet)
// You can use any pin, this is the next one after the SPI pins.
const byte PULSE = 9;

// Hold the values of the DIL switches in a single (8-bit) byte
byte dipSwitches;

// This is what they were last time we read them
byte prevDipSwitches = 255;

// Chip Enable (CE) of the 74HC165
#define SS 2

// Tilt switch
#define tilt 3

// Beeper
#define beeper 6

// LED - careful not to overdrive
#define LED 7

// Total delay (minutes)
unsigned int totDelay;

// 30 second period counter
static unsigned long secsCounter = 0;

// Interim beep required (every 30 secs)
bool interimBeep = false;

// Create timer instance
Timer timer;

// Forward declarations
void playTune();
void beep(unsigned int frequency = 2000, uint32_t duration = 150);
byte getDipValue();
void callBack30secs();
void reminderBeep();
void powerSaving();

// SETUP    SETUP   SETUP
void setup()
{
	// Do Power Savings routine
	powerSaving();

	// Debugging messages
	Serial.begin(9600);

	// Start up SPI
	SPI.begin();

	// This is the pin we're using to make the 74HC165 'load' the values
	pinMode(PULSE, OUTPUT);

	// It's active LOW so switch off for now
	digitalWrite(PULSE, HIGH);

	// Chip Enable
	pinMode(SS, OUTPUT);
	digitalWrite(SS, LOW);

	// Tilt switch (used to suspend timing)
	pinMode(tilt, INPUT_PULLUP);

	// Beeper
	beep(NOTE_A6);

	// LED flasher
	pinMode(LED, OUTPUT);
	digitalWrite(LED, LOW);

	// We're not using the standard SS/CE pin for SPI so just testing it here
	pinMode(10, OUTPUT);
	digitalWrite(10, LOW);

	// Read DIP switches
	getDipValue();

	// Read lower 4-bits for delay period (total)
	totDelay = (dipSwitches & 0b00001111);
	Serial.print("Total delay (30 secs units): ");
	Serial.println(totDelay);

	// Read highest beep for interim beep
	interimBeep = (dipSwitches & 0b10000000);
	Serial.print("Interim beep: ");
	Serial.println(interimBeep ? "true" : "false");

	// Setup the timer (30 secs)
	timer.setInterval(30000);

	// Callback routine
	timer.setCallback(callBack30secs);

	// Start the timer
	timer.start();

	// All done
	Serial.println("\nSet up completed.");
}

// LOOP    LOOP    LOOP    LOOP
void loop()
{
	// Keep the timer updated
	timer.update();

	// Is the tilt switch active?
	while (!digitalRead(tilt)) {
		timer.pause();
		Serial.println("Timer paused");
		//digitalWrite(LED, HIGH);
		//delay(10);
		//digitalWrite(LED, LOW);
		beep();
		delay(990);
	}

	// If we paused the timer (via the tilt) restart it now
	if (timer.isPaused()) {
		timer.start();
		Serial.println("Timer restarted");
	}
}

void callBack30secs() {

	Serial.println("Callback");

	// Another 30 seconds has passed
	secsCounter++;

	// All done?
	if (secsCounter == totDelay) {
		Serial.println("Times up");
		timer.pause();
		playTune();
		timer.setInterval(5000, 10);

		// We can run ANY code as part of the callback, directly
		//timer.setCallback(reminderBeep);
		timer.setCallback([]() {
			beep(NOTE_C7, 50);
			delay(65);
		});
		timer.start();
	} else {
		for (auto cnt = 0; cnt < signed(secsCounter); cnt++) {
			beep(NOTE_C7, 250);
			delay(250);
		}
	}
}

// Unashamedly stolen from the Arduino playground site
// https://www.arduino.cc/en/Tutorial/ToneMelody?from=Tutorial.Tone
// I've just changed the notes a bit to make it more audible using a piezo
void playTune() {
	// notes in the melody:
	int melody[] = { NOTE_C7, NOTE_G6, NOTE_G6, NOTE_A6, NOTE_G6, 0, NOTE_B6, NOTE_C7 };

	// note durations: 4 = quarter note, 8 = eighth note, etc.:
	int noteDurations[] = { 4, 8, 8, 4, 4, 4, 4, 4 };

	// iterate over the notes of the melody:
	for (int thisNote = 0; thisNote < 8; thisNote++) {

		// to calculate the note duration, take one second divided by the note type.
		//e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
		int noteDuration = 1000 / noteDurations[thisNote];
		tone(beeper, melody[thisNote], noteDuration);

		// to distinguish the notes, set a minimum time between them.
		// the note's duration + 30% seems to work well:
		int pauseBetweenNotes = noteDuration * 1.30;
		delay(pauseBetweenNotes);

		// stop the tone playing:
		noTone(beeper);
	}
}

// As I'm using a PASSIVE beeper (the only 3v beeper I have) I have to generate
// a square wave tone to drive it. Luckily, Arduino has a routine for this.
void beep(unsigned int frequency, uint32_t duration) {
	tone(beeper, frequency, duration);
	delay(duration);
}

void reminderBeep() {
	beep(NOTE_C7, 50);
	delay(50);
	beep(NOTE_B6, 50);
}

byte getDipValue() {
	// Tell the 74HC165 to read the values on pins A-H by pulsing the
	digitalWrite(PULSE, LOW);
	delayMicroseconds(5);
	digitalWrite(PULSE, HIGH);

	// Now tell SPI to transfer the data from it to the µC
	dipSwitches = SPI.transfer(0);
	return dipSwitches;
}

void powerSaving() {
	/*
	 * Power saving attempts
	 */

	// disable ADC - this must happen before other power disabling
	ADCSRA = 0;

	// All pins as INPUT PULLUP
	for (byte i = 0; i <= A5; i++) {
		pinMode(i, INPUT_PULLUP);
		digitalWrite(i, LOW);
	}

	// Switch off I2C, SPI etc
	power_adc_disable(); // ADC converter
	// power_spi_disable(); // SPI
	power_usart0_disable();// Serial (USART)
	// delays and millis use Timer 0 do not disable
	// power_timer0_disable();// Timer 0
	power_timer1_disable();// Timer 1
	// tone uses this time, do not disable
	//power_timer2_disable();// Timer 2
	power_twi_disable(); // TWI (I2C)

	/*
	 * End power saving
	 */
}
