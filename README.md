This is code to muliplex and run games on a LED dice box.
Each die is made up of 7 LEDs and up to 5 die may be multiplexed.

The controller is an ESP-12F module (esp8266).
Full construction details are at 

https://www.instructables.com/id/Rainbow-Dice/

It makes use of the BaseSupport library at

https://github.com/roberttidey/BaseSupport

The diceDriver library must also be installed.

Edit the WifiManager and update passwords in BaseConfig.h