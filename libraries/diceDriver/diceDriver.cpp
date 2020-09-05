/*
 R. J. Tidey 2017/12/17
 Dice driver using ESP-12F
 Controls up to 5 dice each made of 7 LEDs
*/
#include "diceDriver.h"
#include <arduino.h>

static int diceCount = DICE_MAXNUMBER;
static int diceDefaultPower[DICE_MAXNUMBER] = {DICE_POWER};
static int dicePower[DICE_MAXNUMBER] = {DICE_POWER};
static int diceLedPins[4] = {LED_PINS};
static int diceMuxPins[DICE_MAXNUMBER] = {DICE_MUXPINS};
static uint32_t diceMuxGPF[DICE_MAXNUMBER] = {DICE_MUXGPF};
static int diceValue[DICE_MAXNUMBER];
static int diceFlash[DICE_MAXNUMBER];
static int diceDigitsLeds[7] = {LEDS_ZERO,LEDS_ONE,LEDS_TWO,LEDS_THREE,LEDS_FOUR,LEDS_FIVE,LEDS_SIX};
static int diceMux[DICE_MAXNUMBER] = {DICE_ONE,DICE_TWO,DICE_THREE,DICE_FOUR,DICE_FIVE};
static int diceSwitch[DICE_MAXNUMBER];
static int debugInt[DICE_MAXNUMBER];

static int nextPeriod;
static unsigned long espNext = 0; //Holds interrupt next count for timer0

//mux variables
static int digit = 0; //die to process
//state 0 = On period of a digit, 1 = off period
volatile int digitState = 0;
static int diceTimer = 0; // timer to use

static int diceRollMask = 0;
static int diceRollMainTimer = 0;
static int diceRollInterval = 0;
static int flashTicks = 100;
static int flashTickCounter = 0;
static int flashOnOff = 0;
static int diceRollIntervalTimer = 0;
unsigned long diceRollLastProcess = 0;


//Main timer routine for muxing and reading switches
void ICACHE_RAM_ATTR diceDriverIsr() {
	if(digitState == 0) {
		int pin = diceMuxPins[digit];
		diceSwitch[digit] = digitalRead(pin);
		// restore to output mode
		GPC(pin) = (GPC(pin) & (0xF << GPCI)) & ~(1 << GPCD);
		//GPF(pin) &= ~(1 << GPFPU); // Disable  Pullup; Don't use GPF macro as it calls a routine which crashes isr
		ESP8266_REG(diceMuxGPF[digit]) &= ~(1 << GPFPU);
		GPES = (1 << pin); //Enable

		nextPeriod = dicePower[digit];
		if((dicePower[digit] >=  DICE_MINPOWER)  && (!flashOnOff || diceFlash[digit] == 0)) {
			//set leds on
			GPOS = diceDigitsLeds[diceValue[digit]] | diceMux[digit];
		}
		digitState = 1;
	} else if(digitState == 1) {
		nextPeriod = MUX_TICKS - dicePower[digit];
		//set leds off
		GPOC = LEDS_OFF | DICE_NONE;
		digit++;
		if(digit == diceCount) {
			digit = 0;
			flashTickCounter--;
			if(flashTickCounter < 0) {
				flashTickCounter = flashTicks;
				flashOnOff = !flashOnOff;
			}
		}
		int pin	= diceMuxPins[digit];
		// set to input_pullup mode
		GPEC = (1 << pin); //Disable
		GPC(pin) = (GPC(pin) & (0xF << GPCI)) | (1 << GPCD); //SOURCE(GPIO) | DRIVER(OPEN_DRAIN) | INT_TYPE(UNCHANGED) | WAKEUP_ENABLE(DISABLED)
		//GPF(pin) |= (1 << GPFPU);  // Enable  Pullup; Don't use GPF macro as it calls a routine which crashes isr
		ESP8266_REG(diceMuxGPF[digit]) |= (1 << GPFPU);
		digitState = 0;
	}
	if(diceTimer == 0 ) {
		espNext += nextPeriod<<8;
		timer0_write(espNext);
	} else {
		timer1_write(nextPeriod);
		timer1_enable(TIM_DIV265, TIM_EDGE, TIM_SINGLE);
	}
}

/**
  Set things up for digit mux and start it up
**/
void diceDriver_init(int esTimer) {
	int i;
	
	diceTimer = esTimer;
	GPOC = LEDS_OFF | DICE_NONE;
	diceValue[0] = 1;
	for(i=0; i < 4; i++) {
		digitalWrite(diceLedPins[i], 0);
		pinMode(diceLedPins[i], OUTPUT);
	}
	for(i=0; i < DICE_MAXNUMBER; i++) {
		digitalWrite(diceMuxPins[i], 0);
		pinMode(diceMuxPins[i], OUTPUT);
		diceValue[i] = 0;
	}
	noInterrupts();
	if(diceTimer == 0) {
		timer0_isr_init();
		timer0_attachInterrupt(diceDriverIsr);
		espNext = ESP.getCycleCount()+10000;
		timer0_write(espNext);
	} else {
		timer1_isr_init();
		timer1_attachInterrupt(diceDriverIsr);
		timer1_enable(TIM_DIV265, TIM_EDGE, TIM_SINGLE);
		timer1_write(1000);
	}
	interrupts();
}

/**
  Set number of dice to drive
**/
void diceDriver_setDiceCount(int count) {
	if ((count > 0) && (count <= DICE_MAXNUMBER)) diceCount = count;
}

/**
  Set value for 1 die
**/
void diceDriver_setValue(int dice, int value) {
	int val = value;
	if(val<0) val = 0;
	if(val>6) val = 6;
	diceValue[dice] = val;
}

/**
  Get value for 1 die
**/
int diceDriver_getValue(int dice) {
	return diceValue[dice];
}

/**
  Set power for 1 die
**/
void diceDriver_setPower(int dice, int power) {
	int val = MUX_TICKS * power / 100;
	if(val >= DICE_MAXPOWER) val = DICE_MAXPOWER;
	if(val < DICE_MINPOWER) val = DICE_MINPOWER-1;
	dicePower[dice] = val;
}

/**
  Get power for 1 die
**/
int diceDriver_getPower(int dice) {
	return dicePower[dice];
}

/**
  Set flash for dice
  flashTime in milliseconds
**/
void diceDriver_setFlash(int flashMask, int flashTime) {
	int i;
	int mask = 1;
	if(flashTime > 0)
		flashTicks = 1000 * flashTime / (1024 * diceCount);
	for(i = 0; i<DICE_MAXNUMBER; i++) {
		diceFlash[i] = (flashMask & mask) ? 1:0;
		mask = mask+mask;
	}
}

/**
  Get flash for 1 die
**/
int diceDriver_getFlash(int dice) {
	return diceFlash[dice];
}


/**
  get default calibration power for 1 die
**/	
int diceDriver_getDefaultPower(int dice) {
	return diceDefaultPower[dice] * 100 / MUX_TICKS;
}

/**
  get debug int
**/	
int diceDriver_getDebugInt(int index) {
	return debugInt[index];
}

/**
  read dice switches
**/	
int diceDriver_readSwitches() {
	int i;
	int switches = 0;
	int mask = 1;
	for(i = 0; i<DICE_MAXNUMBER; i++) {
		if(diceSwitch[i])
			switches = switches | mask;
		mask = mask+mask;
	}
	return switches;
}

/**
  start a dice roll operation
**/	
void diceDriver_rollStart(int rollMask, int rollTime, int rollInterval) {
	if(rollMask) {
		diceRollMask = rollMask;
		diceRollMainTimer = rollTime;
		diceRollInterval = rollInterval;
		diceRollIntervalTimer = 0;
		diceRollLastProcess = millis();
		randomSeed(diceRollLastProcess);
	}
}

/**
  break a dice roll operation
**/	
void diceDriver_rollBreak() {
	diceRollMainTimer = 0;
}

//Update dice values in a roll
void diceRollUpdate() {
	int dice;
	int mask = diceRollMask;
	for(dice=0; dice < diceCount;dice++) {
		if(mask & 1) {
			diceValue[dice] = random(1,7);
		}
		mask = mask >> 1;
	}
}

/**
  process dice roll operation
  returns milliseconds to go or 0 if finished
**/	
int diceDriver_rollProcess() {
	if(diceRollMainTimer > 0) {
		int interval = millis() - diceRollLastProcess;
		diceRollLastProcess = millis();
		diceRollMainTimer -= interval;
		diceRollIntervalTimer -= interval;
		//check whether need to process
		if(diceRollIntervalTimer < 0) {
			diceRollIntervalTimer += diceRollInterval;
			diceRollUpdate();
		}
		return diceRollMainTimer;
	} else {
		return 0;
	}
}

/**
  test function setting dice and value statically
  dice < 0 stop testing and restart interrupt action
  mode 1 disable interrupt action and set IO to value
**/	

void diceDriver_test(int dice, int value) {
	if(dice >= 0 && dice < DICE_MAXNUMBER) {
		digitState = 2;
		nextPeriod = MUX_TICKS >> 1;
		GPOC = LEDS_OFF | DICE_NONE;
		if(value >=0 && value < 7) {
			GPOS = diceDigitsLeds[value] | diceMux[dice];
		}
	} else {
		digitState = 0;
	}
}
