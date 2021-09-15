/*
Battery isolator controller
Application

Author: Andis Jargans

boot| Function    |  NL  | Load    |
----|-------------|------|---------|
SET0|Isol out type| High | Low     |
SET1|Ignc out type| High | Low     |
SET2|Kill sw type | NO   | NC      |
SET3|Relay-fuse   | EN   | Disable |

Revision history:
2021-09-14: Initial version
*/

/**** Hardware configuration **** 
*/

/**** Includes ****/
#include <avr/io.h>
#include <avr/wdt.h>
//#include <avr/interrupt.h>

#include "Drivers/adc_driver.h"
#include "Drivers/bootstrap_driver.h"
#include "Drivers/outputs_driver.h"
#include "Drivers/inputs_driver.h"
#include "Drivers/led_driver.h"

/**** Private definitions ****/
#define SLEEP		0
#define STARTUP		1
#define ACTIVE		2
#define KILLING		3
#define LOCKOUT		4

/**** Aplciation specific configuration ****/
#define DEVELOPMENT
//#define WDT_ENABLED
#define ISOLATOR_DROP_LIMIT		500
#define ISOLATOR_DROP_DELAY		20
#define ISOLATOR_OCP_COOLDOWN	1000
#define ISOLATOR_OCP_DEADTIME	5

#define ALTERNATOR_ACT_VOLTAGE	10000

#define LOCKOUT_TIMEOUT			5000
#define LOCKOUT_LED_TIMEOUT		30000

#define KILL_DELAY_EXTERNAL		100
#define KILL_DELAY_MASTER		100

#define MASTER_DEBOUNCE			10
#define KILL_DEBOUNCE			10

#define IGNC_FAULT_CNT_LIMIT	5

/**** Private variables ****/
static volatile uint8_t sys_state;

static volatile uint8_t master_act = 0;
static volatile uint8_t kill_act = 0;

static volatile uint16_t u_bat = 12000;
static volatile uint16_t u_alt = 0;
static volatile uint16_t u_isol = 0;
static volatile uint16_t u_ignc = 0;
static volatile uint16_t u_relay_drop = 0;

static volatile uint8_t isolator_act = 0;
static volatile uint8_t isolator_act_change = 0;
static volatile uint8_t ignition_act = 0;
static volatile uint8_t alternator_act = 0;

static volatile uint8_t relay_ocp_en = 0;
static volatile uint8_t relay_ocp_deadtime = 0;

/**** Private function declarations ****/
void Init_watchdog(void);
void Init_ReducePower(void);

void DelaySystem(uint16_t cycles);
void DataGathering(uint16_t cycles);
uint8_t Startup_Procedure(void);
uint8_t Kill_Procedure(void);
uint8_t Lockout_Procedure(void);
uint8_t IsolatorOCP(void);

/**** Application ****/
int main(void)
{
	//Initialization
	#ifdef WDT_ENABLED
	wdt_reset();
	Init_watchdog();
	#endif
	Init_ReducePower();
	
	BSDRV_Init();
	LEDDRV_Init();
	ADCDRV_Init(1); //start ADC in waked state
	LEDDRV_OnSolid();
	LEDDRV_Process();

	//Wait for system inputs to stabilize
	DelaySystem(10);
	
	//Read bootstraps
	BSDRV_Latch(1);
	
	if(BSDRV_GetBootstrap(3)) relay_ocp_en = 0;
	else relay_ocp_en = 1;
	
	//***Inputs setup
	inCfgDef mstrSwCfg;
	inCfgDef killSwCfg;
	
	mstrSwCfg.act_level = IN_ACT_LOW;
	mstrSwCfg.pull = IN_PULL_UP;
	mstrSwCfg.dbnc_limit = MASTER_DEBOUNCE;
	 
	//if(BSDRV_GetBootstrap(3)) mstrSwCfg.dbnc_limit = 100; //High filtering, long debounce time
	//else mstrSwCfg.dbnc_limit = 10; //Normal filtering, short debounce time
	
	if(BSDRV_GetBootstrap(2)) killSwCfg.act_level = IN_ACT_HIGH; //Normally closed kill button
	else killSwCfg.act_level = IN_ACT_LOW; //Normally open kill button
	killSwCfg.pull = IN_PULL_UP;
	killSwCfg.dbnc_limit = KILL_DEBOUNCE;
	
	//if(BSDRV_GetBootstrap(3)) killSwCfg.dbnc_limit = 100; //High filtering, long debounce time
	//else killSwCfg.dbnc_limit = 10; //Normal filtering, short debounce time
	
	INDRV_Init(&mstrSwCfg,&killSwCfg);
	
	INDRV_Wake(IN_MASTER);
	INDRV_Wake(IN_KILL);

	//***Outputs setup
	outConfigDef isolCfg;
	outConfigDef igncCfg;
	
	if(BSDRV_GetBootstrap(0)) isolCfg.type = OUT_TYPE_OD; //Active low
	else isolCfg.type = OUT_TYPE_OS; //Active high
	//if(BSDRV_GetBootstrap(0) igncCfg.inv = 1; //Active low
	//else igncCfg.inv = 0; //Active high
	//isolCfg.type = OUT_TYPE_PP;
	isolCfg.inv = 0;
	isolCfg.ext_fault_en = 1;
	
	if(BSDRV_GetBootstrap(1)) igncCfg.type = OUT_TYPE_OD; //Active low
	else igncCfg.type = OUT_TYPE_OS; //Active high
	//if(BSDRV_GetBootstrap(1) igncCfg.inv = 1; //Active low
	//else igncCfg.inv = 0; //Active high
	//igncCfg.type = OUT_TYPE_PP;
	igncCfg.inv = 0;
	igncCfg.ext_fault_en = 0;
	
	OUTDRV_Init(&isolCfg,&igncCfg);
	
	//Set initial target values
	OUTDRV_DisableOutput(OUT_ISOL);
	OUTDRV_DisableOutput(OUT_IGNC);
	OUTDRV_ResetOutput(OUT_ISOL);
	OUTDRV_ResetOutput(OUT_IGNC);
	
	//Apply output states
	OUTDRV_ProcessLogic();
	
	DataGathering(100); //Should be more that debounce time
	
	if((master_act)||(kill_act)) sys_state = LOCKOUT;
	else sys_state = SLEEP;
	
	//Set everything to sleep
	INDRV_Sleep(IN_KILL);
	LEDDRV_Off();

	//main loop
	while(1)
	{
		/******* Input data gathering ***********************************/
		//One system tick is 13.5*4*(1/adc_clock) = 0.864ms
		DataGathering(1);
		
		/******* Output protection processing ***************************/
		OUTDRV_ProcessProtection(u_bat,u_isol,u_alt,u_ignc);
		
		if(isolator_act_change)
		{ 
			//Insert OCP dead time, after output state change
			relay_ocp_deadtime = ISOLATOR_OCP_DEADTIME;
			isolator_act_change = 0;
		};
		
		if(OUTDRV_GetFault(OUT_ISOL)&&(sys_state==ACTIVE))
		{
			//If isolator control OCP, then turn off IGNC and go to killed state
			OUTDRV_DelayFaultExecution(OUT_ISOL,2);
			sys_state = KILLING;
			kill_act = 1;
		};
		
		if(IsolatorOCP()&&(relay_ocp_en)&&(sys_state==ACTIVE))
		{
			//Do kill, force kill-switch active, for fast kill
			sys_state = KILLING;
			kill_act = 1;
		};
		
		if((OUTDRV_GetFaultCount(OUT_IGNC)>IGNC_FAULT_CNT_LIMIT)&&(sys_state==ACTIVE))
		{
			//Do kill, force kill-switch active, for fast kill
			sys_state = KILLING;
			kill_act = 1;
		};
		
		/******* State machine ******************************************/
		switch(sys_state)
		{
			case SLEEP:
				if(master_act) sys_state = STARTUP;
				else sys_state = SLEEP;
				break;
				
			case STARTUP:
				sys_state = Startup_Procedure();
				break;
				
			case ACTIVE:
				if((!master_act)||(kill_act)) sys_state = KILLING;
				else sys_state = ACTIVE;
				break;
				
			case KILLING:
				sys_state = Kill_Procedure();
				break;
				
			case LOCKOUT:
				sys_state = Lockout_Procedure();
				break;
				
			default:
				sys_state = KILLING;
				break;
		}
		
		/******* Output HW processing ***********************************/
		OUTDRV_ProcessLogic();
		
		/******* LED Control processing *********************************/
		LEDDRV_Process();
		
		/******* Wathcdog keep alive  ***********************************/
		#ifdef WDT_ENABLED
		wdt_reset();
		#endif
	}
}

/**** Private function definitions ****/
/**
 * @brief Delay system by pointlessly measuring ADC channels
 * @param [in] cycles Cycles count
 */
void DelaySystem(uint16_t cycles)
{
	for(uint16_t i=0; i<cycles; i++)
	{
		ADCDRV_MeasureAll();
	}
}

/**
 * @brief Main data gathering logic, read all input channels
 * @param [in] cycles Cycles count
 */
void DataGathering(uint16_t cycles)
{
	for(uint16_t i=0; i<cycles; i++)
	{
		ADCDRV_MeasureAll();
		u_bat = ADCDRV_GetValue(ADC_BATU);
		u_alt = ADCDRV_GetValue(ADC_ALTU);
		u_isol = ADCDRV_GetValue(ADC_ISOL);
		u_ignc = ADCDRV_GetValue(ADC_IGNC);
		
		INDRV_ReadAll();
		master_act = INDRV_GetInput(IN_MASTER);
		kill_act = INDRV_GetInput(IN_KILL);
		
		uint8_t i  = OUTDRV_GetRealOutput(OUT_ISOL);
		if(isolator_act!=i) isolator_act_change = 1;
		isolator_act = i;
		ignition_act = OUTDRV_GetRealOutput(OUT_IGNC);
		
		//Alternator activity detection
		//uint16_t temp = 0;
		//if(u_bat>100) temp = u_bat-100;
		//else temp = u_bat;
		
		if(u_alt>u_bat)
		{
			u_relay_drop = u_alt-u_bat;
			alternator_act=1;
		}
		else 
		{
			u_relay_drop = u_bat-u_alt;
			alternator_act=0;
		}
	}
}

/**
 * @brief System startup (wake-up) procedure
 * @return Next system state
 */
uint8_t Startup_Procedure(void)
{
	static uint8_t step = 0;
	static uint16_t timeout = 0;
	
	if(step==0)
	{	
		//Wake up inputs, give N ticks for wakeup
		INDRV_Wake(IN_KILL);
		LEDDRV_OnSolid();
		timeout = 100;
		step=1;
	}
	else if(step==1)
	{
		//wait for timeout 
		if(timeout) timeout--;
		else step=2;
	}
	else if(step==2)
	{
		if((!master_act)||(kill_act)){step = 0;return LOCKOUT;};
		
		//Turn on isolator, and give N ticks to catch errors
		OUTDRV_EnableOutput(OUT_ISOL);
		OUTDRV_SetOutput(OUT_ISOL);
		timeout = 200;
		step=3;
	}
	else if(step==3)
	{
		if((!master_act)||(kill_act)||(OUTDRV_GetFault(OUT_ISOL)))
		{
			//abort startup
			OUTDRV_ResetOutput(OUT_ISOL);
			OUTDRV_DisableOutput(OUT_ISOL);
			step = 0;
			return LOCKOUT;
		};
		
		//wait for timeout 
		if(timeout) timeout--;
		else
		{
			//Turn on ignition, and give N ticks to catch errors
			OUTDRV_EnableOutput(OUT_IGNC);
			OUTDRV_SetOutput(OUT_IGNC);
			timeout = 200;
			step=4;
		}
	}
	else if(step==4)
	{
		if((!master_act)||(kill_act)||(OUTDRV_GetFault(OUT_ISOL))||(OUTDRV_GetFault(OUT_IGNC)))
		{
			//abort startup
			OUTDRV_ResetOutput(OUT_ISOL);
			OUTDRV_ResetOutput(OUT_IGNC);
			OUTDRV_DisableOutput(OUT_ISOL);
			OUTDRV_DisableOutput(OUT_IGNC);
			step = 0;
			return LOCKOUT;
		};
		
		//wait for timeout 
		if(timeout) timeout--;
		else{step = 0;return ACTIVE;}
	};
	
	return STARTUP;
}

/**
 * @brief System kill procedure
 * @return Next system state
 */
uint8_t Kill_Procedure(void)
{
	static uint8_t step = 0;
	static uint16_t timeout = 0;
	
	if(step==0)
	{
		//Turn off ignition
		LEDDRV_Flashing(200);
		OUTDRV_ResetOutput(OUT_IGNC);
		if(kill_act) timeout = KILL_DELAY_EXTERNAL;
		else timeout = KILL_DELAY_MASTER;
		step=1;
	}
	else if(step==1)
	{
		//Wait for alternator rundown
		if(u_alt<ALTERNATOR_ACT_VOLTAGE) step=2;
		
		//If kill activated, then reduce timeout
		if((timeout>KILL_DELAY_EXTERNAL)&&(kill_act)) timeout=KILL_DELAY_EXTERNAL;
		
		//If rundown not detected after timeout, shut down anyway
		if(timeout) timeout--;
		else step=2;
	}
	else if(step==2)
	{
		//Turn off isolator
		OUTDRV_ResetOutput(OUT_ISOL);
		step=3;
		timeout = 100;
	}
	else if(step==3)
	{
		if(timeout) timeout--;
		else
		{
			//Disable outputs
			OUTDRV_DisableOutput(OUT_ISOL);
			OUTDRV_DisableOutput(OUT_IGNC);
			step = 0;
			return LOCKOUT;
		}
	}
	
	return KILLING;
}

/**
 * @brief System lock-out (post-kill) procedure
 * @return Next system state
 */
uint8_t Lockout_Procedure(void)
{
	static uint8_t step = 0;
	static uint16_t timeout = 0;
	static uint16_t led_timeout = 0;
	
	if(step==0)
	{
		//Disable outputs
		OUTDRV_ResetOutput(OUT_ISOL);
		OUTDRV_ResetOutput(OUT_IGNC);
		OUTDRV_DisableOutput(OUT_ISOL);
		OUTDRV_DisableOutput(OUT_IGNC);
		//Set KILL to sleep
		INDRV_Sleep(IN_KILL);
		LEDDRV_Flashing(1000);
		led_timeout = LOCKOUT_LED_TIMEOUT;
		timeout=LOCKOUT_TIMEOUT;
		step=1;
	}
	else
	{
		//LED turns off either when master is off, or after timeout
		if(led_timeout) led_timeout--;
		else LEDDRV_Off();
		
		//Wait for master to be off for N cycles
		if(master_act) timeout=LOCKOUT_TIMEOUT;
		else
		{
			if(timeout) timeout--;
			else
			{
				//Lockout ended
				step = 0;
				led_timeout = 0;
				LEDDRV_Off();
				return SLEEP;
			}
		}
	}
	
	return LOCKOUT;
}

/**
 * @brief Initializes system watchdog
 */
void Init_watchdog(void)
{
	//watchdog timer setup
	WDTCSR |= 0x10; //Change enable
	WDTCSR |= 0x0D; //System reset mode, 0.5s period.
	//use special instruction to reset watchdog timer
}

/**
 * @brief Disables not used system peripherals
 */
void Init_ReducePower(void)
{
	//Disable unnecessary peripherals
	PRR = 0xAC;  //TWI,SPI,TIM0 and TIM1
}

/**
 * @brief Isolator relay over-current protection logic
 * @return Isolator fault status
 */
uint8_t IsolatorOCP(void)
{		
	uint16_t drop = 0;
	uint8_t ocp_warning = 0;
	static uint8_t ocp_fault = 0;
	static uint8_t ocp_counter = 0;
	static uint16_t cooldown_timer = 0;

	//Adjust relay drop
	if(isolator_act) drop = u_relay_drop;
	else drop=0;
	
	//Check Over-Current warning
	if((drop>ISOLATOR_DROP_LIMIT)&&(ISOLATOR_DROP_LIMIT!=0)) ocp_warning = 1;
	else ocp_warning = 0;
	
	//Do delay calculations	
	if(relay_ocp_deadtime) relay_ocp_deadtime--;
	//OCP Delay
	if((ocp_warning)&&(!relay_ocp_deadtime))
	{
		//Calculate increment
		uint16_t x = drop/ISOLATOR_DROP_LIMIT;
		if(x>255) x = 255;
		uint8_t inc = (uint8_t)x;
		
		//Saturated add
		uint8_t dtop = 255-ocp_counter;
		if(inc>dtop) ocp_counter = 255;
		else ocp_counter += inc;
	}
	else
	{
		//Saturated subtraction
		if(ocp_counter) ocp_counter--;
	}
	
	//Check fault
	if(ocp_counter>ISOLATOR_DROP_DELAY)
	{
		ocp_fault = 1;
		if(!cooldown_timer)
		{
			cooldown_timer = ISOLATOR_OCP_COOLDOWN;
		};
	}
	else
	{
		//Wait for fault cooldown time
		if(cooldown_timer) cooldown_timer--;
		else
		{
			//Fault ended
			ocp_fault = 0;
		}
	}
	
	return ocp_fault;
}