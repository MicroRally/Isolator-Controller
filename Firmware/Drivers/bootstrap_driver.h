/*
Battery isolator controller
Bootstrap driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version
*/

#ifndef BOOTS_DRIVER
#define BOOTS_DRIVER

/**** Includes ****/

/**** Public definitions ****/

/**** Public function declarations ****/
//Control functions
void BSDRV_Init(void);
void BSDRV_Latch(uint8_t disable);

//Interrupt and loop functions

//Data retrieve functions
uint8_t BSDRV_GetBootstrap(uint8_t ch);

#endif