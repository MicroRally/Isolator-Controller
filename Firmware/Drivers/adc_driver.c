/*
Battery isolator controller
ADC driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version	
*/

/**** Hardware configuration ****
PC0 - BAT_MON  - battery voltage, 20mV/LSB
PC1 - ISOL_MON - isolator control output voltage, 20mV/LSB
PC2 - IGNC_MON - ignition control output voltage, 20mV/LSB
PC3 - ALT_MON - alternator (isolator relay outut) voltage, 20mV/LSB

ADC=Vin*1024/Vref

Fadc = Fcpu/DIV
One conversion = 13.5 Fadc cycles
ADC clock has to be between 50kHz and 200kHz for 10bit values
*/

/**** Includes ****/
#include <avr/io.h>
#include "adc_driver.h"

/**** Private variables ****/
static volatile uint16_t bat_mon = 0;
static volatile uint16_t isol_mon = 0;
static volatile uint16_t ign_mon = 0;
static volatile uint16_t alt_mon = 0;


/**** Public function definitions ****/
/**
 * @brief Initializes ADC hardware
 * @param [in] wake initialize ADC in waked state
 */
void ADCDRV_Init(uint8_t wake)
{
	PRR &= ~0x01; //Enable ADC power
	PORTCR |= 0x04; //Pull-up disable
	DIDR0 |= 0x0F; //Disable digital inputs
	ADMUX = 0x40; //Set AVCC reference
	ADCSRA = 0x03; //ADC Disabled, Single conversion, no IT, 62.5kHz @1MHz
	ADCSRB = 0x00; //no trigger input
	if(wake) ADCSRA |= 0x80;  //Enable ADC
	else PRR |= 0x01;
}

/**
 * @brief Wake up ADC
 */
void ADCDRV_Wake(void)
{
	//Enable ADC power
	PRR &= ~0x01;
	//Enable ADC
	ADCSRA |= 0x80;
}

/**
 * @brief Put ADC to sleep (low power mode)
 */
void ADCDRV_Sleep(void)
{
	//wait to finish
	while(ADCSRA&0x40);
	//Disable ADC
	ADCSRA &= ~0x80;
	//Disable ADC power
	PRR |= 0x01;
}

/**
 * @brief ADC measurement processing
 */
void ADCDRV_MeasureAll(void)
{
	//check if ADC is enabled
	if((PRR&0x01)||(!(ADCSRA&0x80))) return;
	
	ADMUX &= ~0x0F;
	ADMUX |= 0x00;
	ADCSRA |= 0x40;
	while(ADCSRA&0x40); //wait to finish
	bat_mon = ADC;
	
	ADMUX &= ~0x0F;
	ADMUX |= 0x01;
	ADCSRA |= 0x40;
	while(ADCSRA&0x40); //wait to finish
	isol_mon = ADC;
	
	ADMUX &= ~0x0F;
	ADMUX |= 0x02;
	ADCSRA |= 0x40;
	while(ADCSRA&0x40); //wait to finish
	ign_mon = ADC;
	
	ADMUX &= ~0x0F;
	ADMUX |= 0x03;
	ADCSRA |= 0x40;
	while(ADCSRA&0x40); //wait to finish
	alt_mon = ADC;
}

/**
 * @brief ADC measurement processing
 * @param [in] ch - ADC channel to read
 * @return ADC channel value in mV
 */
uint16_t ADCDRV_GetValue(uint8_t ch)
{
	//convert values to mV return
	switch(ch)
	{
		case 0:
		return bat_mon*20;
		
		case 1:
		return isol_mon*20;
		
		case 2:
		return ign_mon*20;
		
		case 3:
		return alt_mon*20;
		
		default:
		return 0;
	}
}
