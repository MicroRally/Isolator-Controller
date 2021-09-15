/*
Battery isolator controller
Half-bridge outputs driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version
*/

#ifndef OUT_DRIVER
#define OUT_DRIVER

/**** Includes ****/

/**** Public definitions ****/
#define OUT_ISOL	1
#define OUT_IGNC	2

#define OUT_TYPE_OD	1
#define OUT_TYPE_OS	2
#define OUT_TYPE_PP	3

typedef struct outConfigStruct {
	uint8_t type;
	uint8_t inv;
	uint8_t ext_fault_en;
}outConfigDef;

/**** Aplciation specific configuration ****/
#define ISOL_OVERVOLATGE_LIMIT		0
#define ISOL_UNDERVOLATGE_LIMIT		0
#define ISOL_QDROP_LIMIT			500
#define ISOL_OCP_DELAY				2
#define ISOL_FAULT_COOLDOWN_TIME	2000
#define ISOL_OCP_DEAD_TIME			0
#define ISOL_FAULT_RETRY_TIMEOUT	2000

#define IGNC_OVERVOLATGE_LIMIT		0
#define IGNC_UNDERVOLATGE_LIMIT		0
#define IGNC_QDROP_LIMIT			500
#define IGNC_OCP_DELAY				2
#define IGNC_FAULT_COOLDOWN_TIME	2000
#define IGNC_OCP_DEAD_TIME			0
#define IGNC_FAULT_RETRY_TIMEOUT	2000

#define OUT_FAULT_EXEC_DELAY_LIMIT	5


/**** Public function declarations ****/
//Control functions
void OUTDRV_Init(outConfigDef* pIsolCfg, outConfigDef* pIgncCfg);
void OUTDRV_SetOutput(uint8_t ch);
void OUTDRV_ResetOutput(uint8_t ch);
void OUTDRV_SetExtFault(uint8_t ch);
void OUTDRV_ResetExtFault(uint8_t ch);
void OUTDRV_EnableOutput(uint8_t ch);
void OUTDRV_DisableOutput(uint8_t ch);
void OUTDRV_DelayFaultExecution(uint8_t ch, uint8_t cycles);

//Interrupt and loop functions
void OUTDRV_ProcessLogic(void);
void OUTDRV_ProcessProtection(uint16_t u_bat, uint16_t u_isol, uint16_t u_alt, uint16_t u_ignc);

//Data retrieve functions
uint8_t OUTDRV_GetFault(uint8_t ch);
uint8_t OUTDRV_GetRealOutput(uint8_t ch);
uint8_t OUTDRV_GetRetryFlag(uint8_t ch);
uint8_t OUTDRV_GetFaultCount(uint8_t ch);
void OUTDRV_ResetRetryFlag(uint8_t ch);

#endif