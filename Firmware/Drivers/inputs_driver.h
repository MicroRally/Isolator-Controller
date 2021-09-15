/*
Battery isolator controller
User IO driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version
*/

#ifndef IN_DRIVER
#define IN_DRIVER

/**** Includes ****/

/**** Public definitions ****/
typedef struct inCfgStruct {
	uint8_t act_level;
	uint8_t dbnc_limit;
	uint8_t pull;
}inCfgDef;

#define IN_MASTER	1
#define IN_KILL		2

#define IN_PULL_NONE	0
#define IN_PULL_DOWN	1
#define IN_PULL_UP		2

#define IN_ACT_LOW	0
#define IN_ACT_HIGH	1

/**** Public function declarations ****/
//Control functions
void INDRV_Init(inCfgDef* pMstrCfg,inCfgDef* pKillCfg);
void INDRV_Sleep(uint8_t ch);
void INDRV_Wake(uint8_t ch);

//Interrupt and loop functions
void INDRV_ReadAll(void);

//Data retrieve functions
uint8_t INDRV_GetInput(uint8_t ch);
uint8_t INDRV_GetInputChange(uint8_t ch);
void INDRV_ResetInputChange(uint8_t ch);

#endif