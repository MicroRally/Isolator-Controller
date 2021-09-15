/*
Battery isolator controller
User IO driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version
*/

/**** Hardware configuration ****
PD0 - MASTER_UP - Master switch input pull-up/down power
PD1 - EXTKILL_UP - External kill switch pull-up/down power
PD2 - MASTER - Master switch signal
PD3 - EXTKILL - External kill signal
*/

/**** Includes ****/
#include <avr/io.h>
#include "inputs_driver.h"

/**** Private definitions ****/
typedef struct inStateStruct {
	uint8_t level;
	uint8_t changed;
	uint8_t blocked;
	uint8_t dbnc_timer;
}inStateDef;

/**** Private variables ****/
static volatile inStateDef mstr;
static volatile inCfgDef mstr_cfg;

static volatile inStateDef kill;
static volatile inCfgDef kill_cfg;

/**** Private function declarations ****/
static void HAL_Init(void);
static uint8_t HAL_ReadKill(void);
static uint8_t HAL_ReadMaster(void);
static void HAL_SetMasterPull(uint8_t side);
static void HAL_SetKillPull(uint8_t side);

/**** Public function definitions ****/
/**
 * @brief Initializes driver
 * @param [in] pMstrCfg Master switch configuration
 * @param [in] pKillCfg Kill switch configuration
 */
void INDRV_Init(inCfgDef* pMstrCfg,inCfgDef* pKillCfg)
{
	//Initialize hardware
	HAL_Init();
	
	//Set config
	mstr_cfg.act_level = pMstrCfg->act_level;
	mstr_cfg.pull = pMstrCfg->pull;
	mstr_cfg.dbnc_limit = pMstrCfg->dbnc_limit;
	
	kill_cfg.act_level = pKillCfg->act_level;
	kill_cfg.pull = pKillCfg->pull;
	kill_cfg.dbnc_limit = pKillCfg->dbnc_limit;
	
	//Set default values
	if(mstr_cfg.act_level) mstr.level = 0; 
	else mstr.level = 1;
	
	mstr.changed = 0;
	mstr.blocked = 0;
	mstr.dbnc_timer = 0;
	
	//Set default values
	if(kill_cfg.act_level) kill.level = 0; 
	else kill.level = 1;
	
	kill.changed = 0;
	kill.blocked = 0;
	kill.dbnc_timer = 0;
	
	//Apply pull-x config
	HAL_SetMasterPull(mstr_cfg.pull);
	HAL_SetKillPull(kill_cfg.pull);
}

/**
 * @brief Inputs logic processing
 */
void INDRV_ReadAll(void)
{
	uint8_t temp = 0;
	
	//Master switch input
	if(!mstr.blocked)
	{
		if(HAL_ReadMaster()) temp = 1;
		else temp = 0;
		
		if(mstr.level!=temp) mstr.dbnc_timer++;
		else mstr.dbnc_timer = 0;
		
		if(mstr.dbnc_timer>mstr_cfg.dbnc_limit){mstr.level = temp; mstr.changed = 1;};
	};	

	//Kill switch input
	if(!kill.blocked)
	{
		if(HAL_ReadKill()) temp = 1;
		else temp = 0;
		
		if(kill.level!=temp) kill.dbnc_timer++;
		else kill.dbnc_timer = 0;
		
		if(kill.dbnc_timer>kill_cfg.dbnc_limit){kill.level = temp; kill.changed = 1;};
	};
}

/**
 * @brief Put input channel in low power mode
 * @param [in] ch Input channel
 */
void INDRV_Sleep(uint8_t ch)
{
	switch(ch)
	{
		case IN_MASTER:
			//Set not-active level
			if(mstr_cfg.act_level) mstr.level = 0; 
			else mstr.level = 1;
			//Reset values
			mstr.changed = 0;
			mstr.dbnc_timer = 0;
			//Disable pull-x
			HAL_SetMasterPull(IN_PULL_NONE);
			//Set blocked flag
			mstr.blocked = 1;
			break;
		
		case IN_KILL:
			//Set not-active level
			if(kill_cfg.act_level) kill.level = 0; 
			else kill.level = 1;
			//Reset values
			kill.changed = 0;
			kill.dbnc_timer = 0;
			//Disable pull-x
			HAL_SetKillPull(IN_PULL_NONE);
			//Set blocked flag
			kill.blocked = 1;
			break;
		
		default:
			break;
	}
}

/**
 * @brief Wake input channel from low power mode
 * @param [in] ch Input channel
 */
void INDRV_Wake(uint8_t ch)
{
	switch(ch)
	{
		case IN_MASTER:
			//Set not-active level
			if(mstr_cfg.act_level) mstr.level = 0; 
			else mstr.level = 1;
			//Reset values
			mstr.changed = 0;
			mstr.dbnc_timer = 0;
			//Restore pull-x
			HAL_SetMasterPull(mstr_cfg.pull);
			//Reset blocked flag
			mstr.blocked = 0;
			break;
		
		case IN_KILL:
			///Set not-active level
			if(kill_cfg.act_level) kill.level = 0; 
			else kill.level = 1;
			//Reset values
			kill.changed = 0;
			kill.dbnc_timer = 0;
			//Restore pull-x
			HAL_SetKillPull(kill_cfg.pull);
			//Reset blocked flag
			kill.blocked = 0;
			break;
		
		default:
			break;
	}
}

/**
 * @brief Read input channel state
 * @param [in] ch Input channel
 * @return Channel state [0-inactive,1-active]
 */
uint8_t INDRV_GetInput(uint8_t ch)
{
	switch(ch)
	{
		case 1:
			if((!mstr.blocked)&&(mstr.level==mstr_cfg.act_level)) return 1;
			else return 0;;
		
		case 2:
			if((!kill.blocked)&&(kill.level==kill_cfg.act_level)) return 1;
			else return 0;
		
		default:
			return 0;
	}
}

/**
 * @brief Read input channel state change flag
 * @param [in] ch Input channel
 * @return Channel change flag [0-no change,1-changed]
 */
uint8_t INDRV_GetInputChange(uint8_t ch)
{
	switch(ch)
	{	
		case 1:
			return mstr.changed ;
		
		case 2:
			return kill.changed;
		
		default:
			return 0;
	}
}


/**
 * @brief Reset input channel state change flag
 * @param [in] ch Input channel
 */
void INDRV_ResetInputChange(uint8_t ch)
{
	switch(ch)
	{
		case 1:
			mstr.changed = 0;
			break;
		
		case 2:
			kill.changed = 0;
			break;
		
		default:
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
	DDRD &= ~0x0C; //Set as inputs
	PORTD &= ~0x0C; //Disable MCU pull-up
	
	//Pull-x outputs configuration
	DDRD |= 0x03; //Set as outputs
	PORTD &= ~0x03; //Set low
}

/**
 * @brief Reads kill switch input level
 * @return level of kill input
 */
uint8_t HAL_ReadKill(void)
{
	if(PIND&0x08) return 1;
	else return 0;
}

/**
 * @brief Reads master switch input level
 * @return level of master input
 */
uint8_t HAL_ReadMaster(void)
{
	if(PIND&0x04) return 1;
	else return 0;
}

/**
 * @brief Master input pull-x control
 * @param [in] side Pull-x side
 */
void HAL_SetMasterPull(uint8_t side)
{
	if(side==IN_PULL_UP)
	{
		PORTD |= 0x01; //Set pull-x high

	}
	else
	{
		PORTD &= ~0x01; //Set low
	}
}

/**
 * @brief Kill input pull-x control
 * @param [in] side Pull-x side
 */
void HAL_SetKillPull(uint8_t side)
{
	if(side==IN_PULL_UP)
	{
		PORTD |= 0x02; //Set high
	}
	else
	{
		PORTD &= ~0x02; //Set low
	}
}