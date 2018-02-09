/*
 * LEDFire.c
 * This program is targeted towards the ATTINY13A and is designed
 * to simulate a flickering flame using four LEDs, two 5mm RED
 * and two 3mm Orange. This code was taken from 
 * http://fangletronics.blogspot.com/ with the random number
 * generator orginating from http://forums.adafruit.com/viewtopic.php?f=8&t=6976
 *
 * Jay Collett (http://www.jaycollett.com)
 * Update: Modifed code to read PhotoCell and turn off LEDs when light is detected
 * also dropped Fclk to 4.8Mhz with DIV8 fuse set for ~600kHz Fclk frequency.
 * Also lowered delay to account for slower Fclk while keeping same flame flickering effect.
 * Added uC sleep mode to extend battery life.
 */

// lots of includes, but still fits nicely in a ATTINY13A
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <util/delay.h>


// define our consts, want are driving two orange and two red leds (20ma each max)
// ATTINY13A can source 40MA max per pin so we may combine this in a future revision
// PhotoCell is on PINB4 of the ATTINY13A which is hard-coded in the ADC init func.
#define F_CPU 600000UL
#define LED_ORANGE1 PINB0
#define LED_ORANGE2 PINB1
#define LED_RED1 PINB2
#define LED_RED2 PINB3


// variables to control program logic
volatile int f_wdt = 1;
uint16_t randreg = 10;
uint8_t adcValue = 0;
uint8_t i = 0;
uint16_t j = 0;
uint8_t PHOTO_VALUE = 10;
uint8_t flickerLEDs = 0;


// method to put MCU to sleep, deep power saving sleep
void system_sleep(void){
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();

	sleep_mode();

	// system continues execution here when watchdog times out
	sleep_disable();
}

// method to configure WDT
void setup_watchdog(void) {  

  MCUSR &= ~(1<<WDRF);	// reset WDT
  WDTCR |= (1<<WDCE) | (1<<WDE); // set the change enable bit and enable bit
  WDTCR |= (1<<WDP3) | (1<<WDP0) | (1<<WDTIE);	// set the interrupt bit and set timeout to ~8 seconds (1024 cycles)
  sei();	// enable global interrupts
}

// as the name inplies, pseudo random number generator
static uint16_t pseudorandom16(void){

	uint16_t newbit = 0;
	if (randreg == 0) {
		randreg = 1;
	}
	  if (randreg & 0x8000) newbit = 1;
	  if (randreg & 0x4000) newbit ^= 1;
	  if (randreg & 0x1000) newbit ^= 1; 
	  if (randreg & 0x0008) newbit ^= 1;
	 
	  randreg = (randreg << 1) + newbit;

	  return randreg;
}


// read value from configured ADC pin
int ADC_read(void)
{
	int i;
	int ADC_temp;
	
	int ADCr = 0;
	
	ADCSRA = (1<<ADEN); // enable the ADC
	
	//do a dummy readout first
	ADCSRA |= (1<<ADSC); // do single conversion
	
	// wait for conversion done, ADIF flag active
	while(!(ADCSRA & 0x10));
	
	// do the ADC conversion 8 times for better accuracy
	for(i=0;i<8;i++)
	{
		ADCSRA |= (1<<ADSC); // do single conversion
		// wait for conversion done, ADIF flag active
		while(!(ADCSRA & 0x10));

		ADC_temp = ADCL; // read out ADCL register
	
		ADC_temp += (ADCH << 8); // read out ADCH register
	
		// accumulate result (8 samples) for later averaging
		ADCr += ADC_temp;
	}

	ADCr = ADCr >> 3; // average the 8 samples
	ADCSRA = (0<<ADEN); // disable the ADC

	return ADCr;
}

// setup ADC, we'll stop and stop with each read sacrificing performance as the inital read
// is very slow
void ADC_init(void){
	ADMUX = (1<<MUX1); // set PB4 as input
	ADCSRA = (1<<ADPS1)| (1<<ADPS0); // set adc to divide by 8 600Khz / 8 = 75khz
}


// interrupt method for WDT timeout
ISR(WDT_vect){

	f_wdt = 1;
}


// main system method, processes fake fire and handles ADC/WDT logic
int main(void){

	MCUCR = (1<<SE) | (1<<SM1);	// setup sleep mode to deep sleep or PWR_SAVE mode
    
	setup_watchdog();

	while(1){
		// read photocell to see if it's dark out still or not
		if(f_wdt==1){

			ADC_init();
			adcValue = ADC_read(); // read photocell
			if((adcValue) < PHOTO_VALUE){
			// dark out
				flickerLEDs = 1;
			}else{
				flickerLEDs = 0;
			}

			f_wdt=0;	// reset wdt variable
		}
		
		// if it's dark, keep flickering the LEDs
		if(flickerLEDs == 1){	
		
			// enable output pins
			DDRB = (1<<LED_ORANGE1) | (1<<LED_ORANGE2) | (1<<LED_RED1) | (1<<LED_RED2);
			
			j = pseudorandom16();
			for(i=0;i<4;i++){
				if(pseudorandom16() > j){
					PORTB ^= (1<<i);
			    }
			}
			_delay_ms(6); 
		}else{ 

			PORTB = 0x00; 	// set all pins low then set them as input to save power

			DDRB = 0x00;	// set all pins as input to save power

			system_sleep();	// go to sleep
		}
	}
}





