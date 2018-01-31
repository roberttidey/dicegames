/*
 R. J. Tidey 2017/12/17
 Dice driver using ESP-12F
 Controls up to 5 dice each made of 7 LEDs
*/

#define DICE_MAXNUMBER 5
#define DICE_MASK 0x01f

//4 output lines to select LEDs within a dice
// Centre,middle row, Diagonal right, Diagonal left
#define LED_PINS 3,0,4,15
//digits
#define LEDS_OFF 0x8019
#define LEDS_ONE 0x0008
#define LEDS_TWO 0x0010
#define LEDS_THREE 0x8008
#define LEDS_FOUR 0x8010
#define LEDS_FIVE 0x8018
#define LEDS_SIX 0x8011

//lines used to select a die
//RED,YELLOW,WHITE,GREEN,BLUE
#define DICE_MUXPINS 5,14,12,13,2
#define DICE_NONE 0x7024
#define DICE_ONE 0x0020
#define DICE_TWO 0x4000
#define DICE_THREE 0x1000
#define DICE_FOUR 0x2000
#define DICE_FIVE 0x0004

//ticks per mux period (tick 3.2uSec)
#define MUX_TICKS 320

//power calibration ticks per die
#define DICE_POWER 200,200,150,120,150
#define DICE_MINPOWER 30
#define DICE_MAXPOWER 290

extern void diceDriver_init(int esTimer);
extern void diceDriver_setDiceCount(int count);
extern void diceDriver_setValue(int dice, int value);
extern void diceDriver_setPower(int dice, int power);
extern void diceDriver_setFlash(int flashMask, int flashTime);
extern void diceDriver_rollStart(int rollMask, int rollTime, int rollInterval);
extern void diceDriver_rollBreak();
extern int diceDriver_getValue(int dice);
extern int diceDriver_rollProcess();
extern int diceDriver_readSwitches();
extern int diceDriver_getDefaultPower(int dice);
