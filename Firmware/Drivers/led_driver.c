/*
Battery isolator controller
LED driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version
*/

/**** Hardware configuration ****
PB2 - LED_CTRL - LED Control output, active high
*/

/**** Includes ****/
#include <avr/io.h>
#include "led_driver.h"

/**** Private definitions ****/

/**** Private variables ****/
static volatile uint8_t mode = 0;
static volatile uint16_t timer = 0;
static volatile uint16_t flash_t = 0;

/**** Private function declarations ****/
static void HAL_Init(void);
static void HAL_Set(uint8_t state);
static void HAL_Toggle(void);

/**** Public function definitions ****/
/**
 * @brief Initializes driver
 */
void LEDDRV_Init(void)
{
	//Initialize hardware
	HAL_Init();
	timer = 0;
	flash_t = 0;
	mode = LED_OFF;
}

/**
 * @brief Turn off LED
 */
void LEDDRV_Off(void)
{
	mode = LED_OFF;
	timer = 0;
}

/**
 * @brief Turn on LED in solid-on state
 */
void LEDDRV_OnSolid(void)
{
	mode = LED_SOLID;
	timer = 0;
}

/**
 * @brief Set LED in flashing state
 * @param [in] t flashing period
 */
void LEDDRV_Flashing(uint16_t t)
{
	mode = LED_FLASH;
	if(t<2) t=2;
	flash_t  = t/2;
}

/**
 * @brief LED logic processing
 */
void LEDDRV_Process(void)
{
	switch(mode)
	{
		case LED_SOLID:
			HAL_Set(1);
			break;
			
		case LED_FLASH:
			if(timer) timer--;
			else{HAL_Toggle();timer = flash_t;}
			break;
			
		default:
			HAL_Set(0);
			break;
	}
}

/**** Private function definitions ****/
/***** HARDWARE ABSTRACTION LAYER *****/

/**
 * @brief Initializes hardware
 */
void HAL_Init(void)
{
	//Inputs configuration
	PORTB &= ~0x04; //Set low
	DDRB |= 0x04; //Set as output
}

/**
 * @brief Set LED output state
 * @param [in] state LED output state [0-OFF,1-ON]
 */
void HAL_Set(uint8_t state)
{
	if(state)
	{
		PORTB |= 0x04; //Set high
	}
	else
	{
		PORTB &= ~0x04; //Set low
	}
}

/**
 * @brief Toggle LED output
 */
void HAL_Toggle(void)
{
	if(PORTB&0x04)
	{
		PORTB &= ~0x04;
	}
	else
	{
	PORTB |= 0x04;
	}
}