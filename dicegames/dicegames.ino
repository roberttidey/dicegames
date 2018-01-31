/*
 R. J. Tidey 2017/12/17
 Dice Games using ESP-12F
 Controls up to 5 dice each made of 7 LEDs
 Web software update service included
 WifiManager can be used to config wifi network
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include "diceDriver.h"

/*
Wifi Manager Web set up
If WM_NAME defined then use WebManager
*/
#define WM_NAME "DiceGamesWebSetup"
#define WM_PASSWORD "password"
#ifdef WM_NAME
	WiFiManager wifiManager;
#endif
//uncomment to use a static IP
//#define WM_STATIC_IP 192,168,0,100
//#define WM_STATIC_GATEWAY 192,168,0,1

int timeInterval = 50;
#define WIFI_CHECK_TIMEOUT 30000
#define ROLL_TIME_INTERVAL 1000
unsigned long elapsedTime;
unsigned long wifiCheckTime;
unsigned long lastRollTime;

int dRollMask = 31;
int dRollTime = 5000;
int dRollInterval = 150;
int rollStatus;

//Period for a long press milliseconds
#define LONG_PRESS 1000
int pin16 = 0;
unsigned long pin16PressTime;
int pin16Release = 0;

//Dice switches variables
int lastDiceSwitches = DICE_MASK;
int lastDiceRisingEdges = 0;
int diceFlashInterval = 500;

//Game control variables
#define MAX_GAME 12
int gameSet = 1; //run = 0, 1 = choose
int gameMode = 1; //basic game
int gameState = 0; //state within game if needed
// masks for 1 to 12 dice games choosing which dice needed
int gameMasks[12] = {2,24,7,29,31,31,31,31,31,31,31,31};
//masks for switch numbers
int gameVariableMasks[DICE_MAXNUMBER] = {31,30,28,24,16};
//masks for risk mode - 0-2 sets attack dice, 3-4 sets defence dice
int gameRiskMasks[DICE_MAXNUMBER] = {7,6,4,8,24};
int gameRiskAttackCount;

//For update service
String host = "esp8266-hall";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "password";

//AP definitions for static set up
#define AP_SSID "ssid"
#define AP_PASSWORD "password"
#define AP_MAX_WAIT 10
String macAddr;

#define AP_PORT 80

ESP8266WebServer server(AP_PORT);
ESP8266HTTPUpdateServer httpUpdater;
WiFiClient cClient;
WiFiClientSecure sClient;

void ICACHE_RAM_ATTR  delaymSec(unsigned long mSec) {
	unsigned long ms = mSec;
	while(ms > 100) {
		delay(100);
		ms -= 100;
		ESP.wdtFeed();
	}
	delay(ms);
	ESP.wdtFeed();
	yield();
}

void ICACHE_RAM_ATTR  delayuSec(unsigned long uSec) {
	unsigned long us = uSec;
	while(us > 100000) {
		delay(100);
		us -= 100000;
		ESP.wdtFeed();
	}
	delayMicroseconds(us);
	ESP.wdtFeed();
	yield();
}

/*
  Connect to local wifi with retries
  If check is set then test the connection and re-establish if timed out
*/
int wifiConnect(int check) {
	if(check) {
		if(WiFi.status() != WL_CONNECTED) {
			if((elapsedTime - wifiCheckTime) * timeInterval > WIFI_CHECK_TIMEOUT) {
				Serial.println("Wifi connection timed out. Try to relink");
			} else {
				return 1;
			}
		} else {
			wifiCheckTime = elapsedTime;
			return 0;
		}
	}
	wifiCheckTime = elapsedTime;
#ifdef WM_NAME
	Serial.println("Set up managed Web");
#ifdef WM_STATIC_IP
	wifiManager.setSTAStaticIPConfig(IPAddress(WM_STATIC_IP), IPAddress(WM_STATIC_GATEWAY), IPAddress(255,255,255,0));
#endif
	if(check == 0) {
		wifiManager.setConfigPortalTimeout(180);
		if(!wifiManager.autoConnect(WM_NAME, WM_PASSWORD)) WiFi.mode(WIFI_STA);
	} else {
		WiFi.begin();
	}
#else
	Serial.println("Set up manual Web");
	int retries = 0;
	Serial.print("Connecting to AP");
	#ifdef AP_IP
		IPAddress addr1(AP_IP);
		IPAddress addr2(AP_DNS);
		IPAddress addr3(AP_GATEWAY);
		IPAddress addr4(AP_SUBNET);
		WiFi.config(addr1, addr2, addr3, addr4);
	#endif
	WiFi.begin(AP_SSID, AP_PASSWORD);
	while (WiFi.status() != WL_CONNECTED && retries < AP_MAX_WAIT) {
		delaymSec(1000);
		Serial.print(".");
		retries++;
	}
	Serial.println("");
	if(retries < AP_MAX_WAIT) {
		Serial.print("WiFi connected ip ");
		Serial.print(WiFi.localIP());
		Serial.printf(":%d mac %s\r\n", AP_PORT, WiFi.macAddress().c_str());
		return 1;
	} else {
		Serial.println("WiFi connection attempt failed"); 
		return 0;
	} 
#endif
}

/*
  Set dicePower from web
*/
void webSetDicePower() {
	int dice = server.arg("dice").toInt();
	int power = server.arg("power").toInt();
	diceDriver_setPower(dice, power);
	server.send(200, "text/html", "Dice power updated");
}

/*
  Set diceFlash from web
*/
void webSetDiceFlash() {
	int mask = server.arg("mask").toInt();
	int interval = server.arg("interval").toInt();
	diceDriver_setFlash(mask, interval);
	server.send(200, "text/html", "Dice flash updated");
}

/*
  Set diceValue from web
*/
void webSetDiceValue() {
	int dice = server.arg("dice").toInt();
	int value = server.arg("value").toInt();
	diceDriver_setValue(dice, value);
	server.send(200, "text/html", "Dice value updated");
}

/*
  Set roll parameters from web
*/
void webSetRollParameters() {
	dRollMask = server.arg("mask").toInt();
	dRollTime = server.arg("time").toInt();
	dRollInterval = server.arg("interval").toInt();
	server.send(200, "text/html", "Dice roll started");
}

/*
  Get dice status to web
*/
void webGetDiceStatus() {
	String status = "Dice values ";
	int d;
	for(d=0; d < DICE_MAXNUMBER; d++) {
		status += String(diceDriver_getValue(d)) + " ";
	}
	status += "<BR> gameMode gameSet 16Release RisingSwitches " + String(gameMode) + " " + String(gameSet) + " " + String(pin16Release) + " " + String(lastDiceRisingEdges);
	server.send(200, "text/html", status);
}

/*
  Set dice powers on and off by mask
*/
void enableDice(int mask) {
	int d;
	int m = mask;
	for(d=0; d < DICE_MAXNUMBER;d++) {
		if(m & 1) {
			diceDriver_setPower(d, diceDriver_getDefaultPower(d));
		} else {
			diceDriver_setPower(d, 0);
		}
		m = m >> 1;
	}
}

/*
  Dim dice by mask
*/
void dimDice(int mask) {
	int d;
	int m = mask;
	for(d=0; d < DICE_MAXNUMBER;d++) {
		if(m & 1) {
			diceDriver_setPower(d, DICE_MINPOWER);
		} else {
			diceDriver_setPower(d, diceDriver_getDefaultPower(d));
		}
		m = m >> 1;
	}
}

/*
  Set all dice values
*/
void setAllDice(int value) {
	int d;
	for(d=0; d < DICE_MAXNUMBER;d++) {
		diceDriver_setValue(d, value);
	}
}

/*
  Show the game mode on first two die
*/
void showGameMode() {
	int mode = gameMode;
	int d =0;
	diceDriver_setValue(1,0);
	if(mode>6) {
		enableDice(0x03);
		diceDriver_setValue(0,6);
		d = 1;
		mode = mode - 6;
	} else {
		enableDice(0x01);
	}
	diceDriver_setValue(d,mode);
}

/*
  Check pin16 and set pin16Release when button released
*/
void check_pin16() {
	int pin = digitalRead(16);
	unsigned long period;
	if(pin == 1 && pin16 == 0){
		pin16PressTime = elapsedTime;
	} else if(pin == 0 && (pin16 == 1)) {
		period = (elapsedTime - pin16PressTime) * timeInterval;
		pin16Release = (period>LONG_PRESS) ? 2:1;
	}
	pin16 = pin;
}

/*
  Check for Roll dice by switches
*/
int checkSwitchRoll() {
	int d;
	int gMask = 0;
	int mask = 1;
	for(d=0; d < DICE_MAXNUMBER;d++) {
		if(lastDiceRisingEdges & mask) {
			gMask |= mask;
		}
		mask = mask + mask;
	}
	lastDiceRisingEdges = 0;
	return gMask & gameMasks[gameMode-1];
}

/*
  Get dice switches and process rising edges
*/
void updateDiceSwitches() {
	int sw1;
	int mask;
	int switches = diceDriver_readSwitches();
	int changes = switches ^ lastDiceSwitches;
	lastDiceRisingEdges ^= (changes & switches);
	lastDiceSwitches = switches;
	//find first switch
	mask = 1;
	if(lastDiceRisingEdges) {
		for(sw1 = 0; sw1 < DICE_MAXNUMBER; sw1++) {
			if(lastDiceRisingEdges & mask)
				break;
			mask = mask + mask;
		}
		switch(gameMode) {
			case 6:
				dRollMask = gameVariableMasks[sw1];
				enableDice(dRollMask);
				lastDiceRisingEdges = 0;
				break;
			case 7:
				//yahtzee flash
				diceDriver_setFlash(lastDiceRisingEdges, diceFlashInterval);
				break;
			case 8:
				//yahtzee dim
				dimDice(lastDiceRisingEdges);
				break;
			case 9:
				//risk
				if(sw1<3) {
					//attack
					setAllDice(1);
					dRollMask = gameRiskMasks[sw1];
					enableDice(dRollMask);
					gameRiskAttackCount = sw1;
				} else {
					//defence
					//limit number of defence dice
					if(gameRiskAttackCount == 2) sw1 = 3;
					mask = gameRiskMasks[sw1];
					enableDice(dRollMask | mask);
					dRollMask = mask;
				}
				lastDiceRisingEdges = 0;
				break;
		}
	}
}

/*
  Run the selected game
*/
void runGame() {
	if(pin16Release == 2) {
		diceDriver_rollBreak();
		pin16Release = 0;
		gameSet = 1;
		showGameMode();
	} else if(rollStatus == 0 && pin16Release &&(elapsedTime-lastRollTime)*timeInterval > ROLL_TIME_INTERVAL) {
		lastRollTime = elapsedTime;
		pin16Release = 0;
		switch(gameMode) {
			//1-5 is simple roll of that number of dice
			case 1:
			case 2:
			case 3:
			case 4:
			case 5:
				dRollMask = gameMasks[gameMode-1];
				enableDice(dRollMask);
				break;
			case 6:
				//6 variable number of device based on which switch clicked
				break;
			case 7:
				//7 and 8 shows 5 dice and allows selection of dice to reroll
				//none selected rolls all the dice
			case 8:
				dRollMask = checkSwitchRoll();
				if(dRollMask == 0) dRollMask = gameMasks[gameMode-1];
				enableDice(gameMasks[gameMode-1]);
				break;
			case 9:
				//Risk mode
				//Switches 1,2,3 show attack dice, 4,5 show defence dice
				break;
			case 10:
				break;
			case 11:
				break;
			case 12:
				break;
		}
		diceDriver_setFlash(0,0);
		diceDriver_rollStart(dRollMask, dRollTime, dRollInterval);
	}
}

/*
  Update game selected
*/
void doGameSet() {
	if(pin16Release == 2) {
		gameSet = 0;
		pin16Release = 0;
		setAllDice(1);
		enableDice(gameMasks[gameMode-1]);
		lastDiceRisingEdges = 0;
	} else if(pin16Release) {
		pin16Release = 0;
		gameMode++;
		if(gameMode > MAX_GAME) gameMode = 1;
		showGameMode();
	}
}

/*
  Set up basic wifi, collect config from flash/server, initiate update server
*/
void setup() {
	Serial.begin(115200);
	Serial.printf("\r\nChipId %6X\r\n",ESP.getFlashChipId());
	Serial.printf("ChipSize %d\r\n",ESP.getFlashChipRealSize());
	Serial.println("Set up Web update service");
	wifiConnect(0);
	macAddr = WiFi.macAddress();
	macAddr.replace(":","");
	Serial.println(macAddr);

	//Update service
	MDNS.begin(host.c_str());
	httpUpdater.setup(&server, update_path, update_username, update_password);
	server.on("/setpower", webSetDicePower);
	server.on("/setflash", webSetDiceFlash);
	server.on("/setdice", webSetDiceValue);
	server.on("/parameters", webSetRollParameters);
	server.on("/status", webGetDiceStatus);
	server.begin();
	
	pinMode(16, INPUT_PULLDOWN_16);

	MDNS.addService("http", "tcp", 80);
	Serial.println("Set up complete");
	diceDriver_init(1);
	showGameMode();
}

/*
  Main loop
*/
void loop() {
	delaymSec(timeInterval);
	elapsedTime++;
	server.handleClient();
	check_pin16();
	if(gameSet == 0) {
		updateDiceSwitches();
		runGame();
		rollStatus = diceDriver_rollProcess();
	} else {
		doGameSet();
	}
}
