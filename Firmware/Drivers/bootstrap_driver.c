/*
Battery isolator controller
Bootstrap driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version	
*/

/**** Hardware configuration ****
PA0 - BOOT0 - Active low, pull-up
PA1 - BOOT1 - Active low, pull-up
PA2 - BOOT2 - Active low, pull-up
PA3 - BOOT3 - Active low, pull-up
*/

#include <avr/io.h>
#include "bootstrap_driver.h"

/**** Private variables ****/
static uint8_t bootstraps = 0x00;
static uint8_t latched = 0;

/**** Public function definitions ****/
/**
 * @brief Bootstrap driver initialization
 */
void BSDRV_Init(void)
{		
	//GPIO configuration
	DDRA &= ~0x0F; //Set as inputs
	PORTA |= 0x0F; //Enable pull-up
	latched = 0;
}

/**
 * @brief Read and latch bootstrap values
 * @param [in] disable - Put bootstrap pins in low power mode
 */
void BSDRV_Latch(uint8_t disable)
{
	if(latched) return;
	bootstraps = PINA&0x0F; //Read and save values
	if(disable)
	{
		PORTA &= ~0x0F; //Turn-off pull ups to reduce current drain
		latched = 1;	//Set latched flag
	}
}

/**
 * @brief Read and latch bootstrap values
 * @param [in] ch - Bootstrap channel [0 to 3]
 * @return bootstrap pin value [0-Not Load, 1-Load]
 */
uint8_t BSDRV_GetBootstrap(uint8_t ch)
{
	uint8_t ret_val = 0;
	
	switch(ch)
	{
		case 0:
			if(bootstraps&0x01) ret_val = 0;
			else ret_val = 1;
			break;
		
		case 1:
			if(bootstraps&0x02) ret_val = 0;
			else ret_val = 1;
			break;
		
		case 2:
			if(bootstraps&0x04) ret_val = 0;
			else ret_val = 1;
			break;
		
		case 3:
			if(bootstraps&0x08) ret_val = 0;
			else ret_val = 1;
			break;
		
		case 255:
			ret_val = ((~bootstraps)&0x0F);
			break;
		
		default:
			ret_val = 0;
			break;
	}
	
	return ret_val;
}

