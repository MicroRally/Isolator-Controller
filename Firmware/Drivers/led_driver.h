/*
Battery isolator controller
LED driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version
*/

#ifndef LED_DRIVER
#define LED_DRIVER

/**** Includes ****/

/**** Public definitions ****/
#define LED_OFF		0
#define LED_SOLID	1
#define LED_FLASH	2

/**** Public function declarations ****/
//Control functions
void LEDDRV_Init(void);
void LEDDRV_Off(void);
void LEDDRV_OnSolid(void);
void LEDDRV_Flashing(uint16_t t);

//Interrupt and loop functions
void LEDDRV_Process(void);

//Data retrieve functions


#endif