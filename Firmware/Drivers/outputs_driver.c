/*
Battery isolator controller
Half-bridge outputs driver

Author: Andis Jargans

Revision history:
2021-09-14: Initial version
*/

/**** Hardware configuration ****
PB0 - IGNC_N - Ignition control output, low side control, active high
PB1 - ISOL_N - Isolator control output, low side control, active high
PB6 - IGNC_P - Ignition control output, high side control, active high
PB7 - ISOL_P - Isolator control output, high side control, active high
*/

/**** Includes ****/
#include <avr/io.h>
#include "outputs_driver.h"

/**** Private definitions ****/
#define HWOUT_HIZ	0
#define HWOUT_LOW	1
#define HWOUT_HIGH	2

typedef struct ProtectionStruct {
	uint8_t ocp_warning;
	uint8_t ovp_warning;
	uint8_t uvp_warning;
	uint8_t ocp_counter;
	uint16_t cooldown_timer;
	uint8_t ext_fault;
	uint8_t fault;
	uint8_t delay_exec;
	uint8_t ocp_deadtime;
	uint8_t retry_flag;
	uint8_t fault_cnt;
	uint16_t retry_timer;
}ProtectionDef;

typedef struct SatusStruct {
	uint8_t hw;
	uint8_t target;
	uint8_t real;
	uint8_t en;
}SatusDef;

/**** Private variables ****/
static volatile SatusDef igncState;
static volatile outConfigDef igncCfg;
static volatile ProtectionDef igncProt;

static volatile SatusDef isolState;
static volatile outConfigDef isolCfg;
static volatile ProtectionDef isolProt;

/**** Private function declarations ****/
static uint8_t ProcessIgnitionProtection(uint16_t volt_pwrsrc, uint16_t volt_out);
static uint8_t ProcessIsolatorProtection(uint16_t volt_pwrsrc, uint16_t volt_out);
static uint8_t StateToHWLevel(outConfigDef cfg, uint8_t state);
static void HAL_Init(void);
static void HAL_SetIgnition(uint8_t level);
static void HAL_SetIsolator(uint8_t level);

/**** Public function definitions ****/
/**
 * @brief Initializes driver
 * @param [in] pIsolCfg Isolator output configuration
 * @param [in] pIgncCfg Ignition output configuration
 */
void OUTDRV_Init(outConfigDef* pIsolCfg, outConfigDef* pIgncCfg)
{
	HAL_Init();
	
	//Reset ignition variables
	igncState.target = 0;
	igncState.real = 0;
	igncState.en = 0;
	
	igncCfg.type = pIgncCfg->type;
	igncCfg.inv = pIgncCfg->inv;
	igncCfg.ext_fault_en = pIgncCfg->ext_fault_en;
	
	igncProt.ocp_warning = 0;
	igncProt.ovp_warning = 0;
	igncProt.uvp_warning = 0;
	igncProt.ocp_counter = 0;
	igncProt.cooldown_timer = 0;
	igncProt.ext_fault = 0;
	igncProt.fault = 0;
	igncProt.delay_exec = 0;
	igncProt.ocp_deadtime = 0;
	igncProt.retry_flag = 0;
	igncProt.fault_cnt = 0;
	igncProt.retry_timer = 0;
	
	//Reset isolator variables
	isolState.target = 0;
	isolState.real = 0;
	isolState.en = 0;
	
	isolCfg.type = pIsolCfg->type;
	isolCfg.inv = pIsolCfg->inv;
	isolCfg.ext_fault_en = pIsolCfg->ext_fault_en;
	
	isolProt.ocp_warning = 0;
	isolProt.ovp_warning = 0;
	isolProt.uvp_warning = 0;
	isolProt.ocp_counter = 0;
	isolProt.cooldown_timer = 0;
	isolProt.ext_fault = 0;
	isolProt.fault = 0;
	isolProt.delay_exec = 0;
	isolProt.ocp_deadtime = 0;
	isolProt.retry_flag = 0;
	isolProt.fault_cnt = 0;
	isolProt.retry_timer = 0;
}

/**
 * @brief Set output ON
 * @param [in] ch Channel
 */
void OUTDRV_SetOutput(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			isolState.target = 1;
			break;
			
		case OUT_IGNC:
			igncState.target = 1;
			break;
			
		default:
			break;
	}
}

/**
 * @brief Set output OFF
 * @param [in] ch Channel
 */
void OUTDRV_ResetOutput(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			isolState.target = 0;
			break;
			
		case OUT_IGNC:
			igncState.target = 0;
			break;
			
		default:
			break;
	}
}

/**
 * @brief Enable output channel
 * @param [in] ch Channel
 */
void OUTDRV_EnableOutput(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			isolState.en = 1;
			break;
			
		case OUT_IGNC:
			igncState.en = 1;
			break;
			
		default:
			break;
	}
}

/**
 * @brief Disable (ForceHiZ) output channel
 * @param [in] ch Channel
 */
void OUTDRV_DisableOutput(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			isolState.en = 0;
			break;
			
		case OUT_IGNC:
			igncState.en = 0;
			break;
			
		default:
			break;
	}
}

/**
 * @brief Get current, real output state
 * @param [in] ch Channel
 * return Output state
 */
uint8_t OUTDRV_GetRealOutput(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			return isolState.real;
			break;
			
		case OUT_IGNC:
			return igncState.real;
			
		default:
			return 0;
	}
}

/**
 * @brief Output logic processing
 */
void OUTDRV_ProcessLogic(void)
{
	if(igncProt.delay_exec)
	{
		igncProt.delay_exec--;
	}
	else if((igncProt.fault)||(igncProt.ext_fault)||(igncState.en==0))
	{
		//Disable output
		HAL_SetIgnition(HWOUT_HIZ);
		igncState.real = 0;
	}
	else
	{
		//Set intended output
		HAL_SetIgnition(StateToHWLevel(igncCfg,igncState.target));
		igncState.real = igncState.target;
	}
	
	if(isolProt.delay_exec)
	{
		isolProt.delay_exec--;
	}
	else if((isolProt.fault)||(isolProt.ext_fault)||(isolState.en==0))
	{
		//Disable output
		HAL_SetIsolator(HWOUT_HIZ);
		isolState.real = 0;
	}
	else
	{
		//Set intended output
		HAL_SetIsolator(StateToHWLevel(isolCfg,isolState.target));
		isolState.real = isolState.target;
	}
}

/**
 * @brief Output protection processing
 * @param [in] u_bat Battery voltage in mV
 * @param [in] u_isol Isolator control output voltage in mV
 * @param [in] u_alt Alternator voltage in mV
 * @param [in] u_ignc Ignition control output voltage in mV
 */
void OUTDRV_ProcessProtection(uint16_t u_bat, uint16_t u_isol, uint16_t u_alt, uint16_t u_ignc)
{
	ProcessIsolatorProtection(u_bat,u_isol);
	ProcessIgnitionProtection(u_alt,u_ignc);
}

/**
 * @brief Get channels fault status
 * @param [in] ch Channel
 * @return Fault status
 */
uint8_t OUTDRV_GetFault(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			if((isolProt.fault)||(isolProt.ext_fault)) return 1;
			else return 0;
			
		case OUT_IGNC:
			if((igncProt.fault)||(igncProt.ext_fault)) return 1;
			else return 0;
			
		default:
			return 0;
	}
}

/**
 * @brief Get channels retry flag
 * @param [in] ch Channel
 * @return Retry flag
 */
uint8_t OUTDRV_GetRetryFlag(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			if(isolProt.retry_flag) return 1;
			else return 0;
			
		case OUT_IGNC:
			if(igncProt.retry_flag) return 1;
			else return 0;
			
		default:
			return 0;
	}
}

/**
 * @brief Reset channels retry flag
 * @param [in] ch Channel
 */
void OUTDRV_ResetRetryFlag(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			isolProt.retry_flag = 0;
			break;
			
		case OUT_IGNC:
			igncProt.retry_flag = 0;
			break;
			
		default:
			break;;
	}
}

/**
 * @brief Get channels retry fault count
 * @param [in] ch Channel
 * @return Fault Retry count
 */
uint8_t OUTDRV_GetFaultCount(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			return isolProt.fault_cnt;
			
		case OUT_IGNC:
			return igncProt.fault_cnt;
			
		default:
			return 0;
	}
}

/**
 * @brief Set external fault flag
 * @param [in] ch Channel
 */
void OUTDRV_SetExtFault(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			if(isolCfg.ext_fault_en) isolProt.ext_fault = 1; 
			else isolProt.ext_fault = 0;
			break;
			
		case OUT_IGNC:
			if(igncCfg.ext_fault_en) igncProt.ext_fault = 1; 
			else igncProt.ext_fault = 0;
			break;
			
		default:
			break;
	}
}

/**
 * @brief Reset external fault flag
 * @param [in] ch Channel
 */
void OUTDRV_ResetExtFault(uint8_t ch)
{
	switch(ch)
	{
		case OUT_ISOL:
			isolProt.ext_fault = 0;
			break;
			
		case OUT_IGNC:
			igncProt.ext_fault = 0;
			break;
			
		default:
			break;
	}
}

/**
 * @brief Delay Fault execution by N system cycles
 * @param [in] ch Channel
 * @param [in] cycles Cycles count
 */
void OUTDRV_DelayFaultExecution(uint8_t ch, uint8_t cycles)
{
	if(cycles>OUT_FAULT_EXEC_DELAY_LIMIT) cycles = OUT_FAULT_EXEC_DELAY_LIMIT;
	
	switch(ch)
	{
		case OUT_ISOL:
			isolProt.delay_exec = cycles; 
			break;
			
		case OUT_IGNC:
			igncProt.delay_exec = cycles;
			break;
			
		default:
			break;
	}
}

/**** Private function definitions ****/

/**
 * @brief Ignition output protection processing
 * @param [in] volt_pwrsrc Channels power source voltage in mV
 * @param [in] volt_out Channels output voltage in mV
 * @return fault indicator
 */
uint8_t ProcessIgnitionProtection(uint16_t volt_pwrsrc, uint16_t volt_out)
{		
	//Calculate mosfet voltage drop
	uint16_t drop = 0;
	if((igncState.hw==HWOUT_HIGH)&&(volt_pwrsrc>volt_out)) drop = volt_pwrsrc-volt_out;
	else if(igncState.hw==HWOUT_LOW) drop = volt_out;
	else drop = 0;
	
	//Check Over-Voltage warning
	if((volt_pwrsrc>IGNC_OVERVOLATGE_LIMIT)&&(IGNC_OVERVOLATGE_LIMIT!=0)) igncProt.ovp_warning = 1;
	else igncProt.ovp_warning = 0;
	
	//Check Under-Voltage warning
	if((volt_pwrsrc<IGNC_UNDERVOLATGE_LIMIT)&&(IGNC_UNDERVOLATGE_LIMIT!=0)) igncProt.uvp_warning = 1;
	else igncProt.uvp_warning = 0;

	//Check Over-Current warning
	if((drop>IGNC_QDROP_LIMIT)&&(IGNC_QDROP_LIMIT!=0)) igncProt.ocp_warning = 1;
	else igncProt.ocp_warning = 0;
	
		
	//Do delay calculations
	if(igncProt.ocp_deadtime) igncProt.ocp_deadtime--;	
	//OCP delay
	if((igncProt.ocp_warning)&&(!igncProt.ocp_deadtime))
	{
		//Calculate increment
		uint16_t x = drop/IGNC_QDROP_LIMIT;
		if(x>255) x = 255;
		uint8_t inc = (uint8_t)x;
		
		//Saturated add
		uint8_t dtop = 255-igncProt.ocp_counter;
		if(inc>dtop) igncProt.ocp_counter = 255;
		else igncProt.ocp_counter += inc;
	}
	else
	{
		//Saturated subtraction
		if(igncProt.ocp_counter) igncProt.ocp_counter--;
	}
	
	
	//Check fault
	if((igncProt.ovp_warning)||(igncProt.uvp_warning)||(igncProt.ocp_counter>ISOL_OCP_DELAY))
	{
		if((!igncProt.fault)&&(igncProt.fault_cnt<255)) igncProt.fault_cnt++;
		
		igncProt.fault = 1;
		
		if(!igncProt.cooldown_timer)
		{
			igncProt.cooldown_timer = IGNC_FAULT_COOLDOWN_TIME;
		};
	}
	else
	{
		//Wait for fault cooldown time
		if(igncProt.cooldown_timer) igncProt.cooldown_timer--;
		else
		{
			//Fault ended
			if(igncProt.fault)
			{
				igncProt.fault = 0;
				igncProt.retry_flag = 1;
				igncProt.retry_timer = IGNC_FAULT_RETRY_TIMEOUT;
			}
			else
			{
				if(igncProt.retry_timer) igncProt.retry_timer--;
				else igncProt.fault_cnt = 0;
			}
		}
	}
	
	return igncProt.fault;
}

/**
 * @brief Isolator output protection processing
 * @param [in] volt_pwrsrc Channels power source voltage in mV
 * @param [in] volt_out Channels output voltage in mV
 * @return fault indicator
 */
uint8_t ProcessIsolatorProtection(uint16_t volt_pwrsrc, uint16_t volt_out)
{		
	//Calculate mosfet voltage drop
	uint16_t drop = 0;
	if((isolState.hw==HWOUT_HIGH)&&(volt_pwrsrc>volt_out)) drop = volt_pwrsrc-volt_out;
	else if(isolState.hw==HWOUT_LOW) drop = volt_out;
	else drop = 0;
	
	//Check Over-Voltage warning
	if((volt_pwrsrc>ISOL_OVERVOLATGE_LIMIT)&&(ISOL_OVERVOLATGE_LIMIT!=0)) isolProt.ovp_warning = 1;
	else isolProt.ovp_warning = 0;
	
	//Check Under-Voltage warning
	if((volt_pwrsrc<ISOL_UNDERVOLATGE_LIMIT)&&(ISOL_UNDERVOLATGE_LIMIT!=0)) isolProt.uvp_warning = 1;
	else isolProt.uvp_warning = 0;

	//Check Over-Current warning
	if((drop>ISOL_QDROP_LIMIT)&&(ISOL_QDROP_LIMIT!=0)) isolProt.ocp_warning = 1;
	else isolProt.ocp_warning = 0;
	
	//Do delay calculations	
	if(isolProt.ocp_deadtime) isolProt.ocp_deadtime--;
	//OCP Delay
	if((isolProt.ocp_warning)&&(!isolProt.ocp_deadtime))
	{
		//Calculate increment
		uint16_t x = drop/ISOL_QDROP_LIMIT;
		if(x>255) x = 255;
		uint8_t inc = (uint8_t)x;
		
		//Saturated add
		uint8_t dtop = 255-isolProt.ocp_counter;
		if(inc>dtop) isolProt.ocp_counter = 255;
		else isolProt.ocp_counter += inc;
	}
	else
	{
		//Saturated subtraction
		if(isolProt.ocp_counter) isolProt.ocp_counter--;
	}
	
	
	//Check fault
	if((isolProt.ovp_warning)||(isolProt.uvp_warning)||(isolProt.ocp_counter>ISOL_OCP_DELAY))
	{
		if((!isolProt.fault)&&(isolProt.fault_cnt<255)) isolProt.fault_cnt++;
		
		isolProt.fault = 1;
		
		if(!isolProt.cooldown_timer)
		{
			isolProt.cooldown_timer = ISOL_FAULT_COOLDOWN_TIME;
		};
	}
	else
	{
		//Wait for fault cooldown time
		if(isolProt.cooldown_timer) isolProt.cooldown_timer--;
		else
		{
			//Fault ended
			if(isolProt.fault)
			{
				isolProt.fault = 0;
				isolProt.retry_flag = 1;
				isolProt.retry_timer = ISOL_FAULT_RETRY_TIMEOUT;
			}
			else
			{
				if(isolProt.retry_timer) isolProt.retry_timer--;
				else isolProt.fault_cnt = 0;
			}
		}
	}
	
	return isolProt.fault;
}

/**
 * @brief Convert logic level output state to HW level output
 * @param [in] cfg Channel configuration data
 * @param [in] state State to set [0-off,1-on]
 * @return HW level to set
 */
uint8_t StateToHWLevel(outConfigDef cfg, uint8_t state)
{	
	uint8_t level = HWOUT_HIZ;
	
	switch (cfg.type)
	{
		case OUT_TYPE_PP :
			//Push-pull output
			if(state)
			{
				if(cfg.inv) level = HWOUT_LOW;
				else level = HWOUT_HIGH;
			}
			else
			{
				if(cfg.inv) level = HWOUT_HIGH;
				else level = HWOUT_LOW;
			}
			break;
			
		case OUT_TYPE_OD :
			//Open-drain output
			if(state) level = HWOUT_LOW;
			else level = HWOUT_HIZ;
			break;
			
		case OUT_TYPE_OS :
			//Open-source output
			if(state) level = HWOUT_HIGH;
			else level = HWOUT_HIZ;
			break;
			
		default :
			//Disabled/not connected output
			level = HWOUT_HIZ;
			break;
	}
	
	return level;
}

/***** HARDWARE ABSTRACTION LAYER *****/

/**
 * @brief Initializes hardware
 */
void HAL_Init(void)
{	
	//Disable pull-ups on PORTB
	PORTCR |= 0x02;
	//Brake-Before-make on PORTB
	PORTCR |= 0x20;
	
	//GPIO configuration, set HiZ output
	PORTB &= ~0xC3; //Set low
	DDRB |= 0xC3;   //Set as output
	
	isolState.hw = HWOUT_HIZ;
	igncState.hw = HWOUT_HIZ;
}

/**
 * @brief Ignition output low level control and logic protection
 * @param [in] level Output level [0-HiZ/1-low/2-high]
 */
void HAL_SetIgnition(uint8_t level)
{
	if(level!=igncState.hw) igncProt.ocp_deadtime = IGNC_OCP_DEAD_TIME;
	
	if(level==HWOUT_HIGH)
	{
		PORTB &= ~0x01; //Reset low side
		PORTB |= 0x40;  //Set high side
		igncState.hw = HWOUT_HIGH;
	}
	else if(level==HWOUT_LOW)
	{
		PORTB &= ~0x40; //Reset high side
		PORTB |= 0x01;  //Set low side
		igncState.hw = HWOUT_LOW;
	}
	else
	{
		PORTB &= ~0x41; //Reset high & low side
		igncState.hw = HWOUT_HIZ;
	}
}

/**
 * @brief Isolator output low level control and logic protection
 * @param [in] level Output level [0-HiZ/1-low/2-high]]
 */
void HAL_SetIsolator(uint8_t level)
{
	if(level!=isolState.hw) isolProt.ocp_deadtime = ISOL_OCP_DEAD_TIME;
	
	if(level==HWOUT_HIGH)
	{
		PORTB &= ~0x02; //Reset low side
		PORTB |= 0x80;  //Set high side
		isolState.hw = HWOUT_HIGH;
	}
	else if(level==HWOUT_LOW)
	{
		PORTB &= ~0x80; //Reset high side
		PORTB |= 0x02;  //Set low side
		isolState.hw = HWOUT_LOW;
	}
	else
	{
		PORTB &= ~0x82; //Reset high & low side
		isolState.hw = HWOUT_HIZ;
	}
}