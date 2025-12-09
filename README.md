This project was completed entirely by Jonathan Stoll (Group 12).

This project is a Swamp Cooler which senses the environment around it and uses fans and vents to cool the surroundings. The cooler was built using an Arduino microcontroller and several 
additional modules included in the Arduino kit. The core functionality of this system is controlled by two sensors: the DHT11 humidity and temperature sensor, and a simple water sensor. 
The system continually outputs humidity and temperature readings to the LCD display. When the temperature spikes above the desired range, the Arduino powers a fan to cool down. When there 
is not enough water available, the Arduino displays an error screen on the LCD. The user can change the vent angle anytime through two input buttons (one for higher, one for lower) and 
can disable the system temporarily with a third button. This cooler has four states as indicated by the included LEDs: Running – fan is enabled and water level is sufficient, Idle – fan 
is not running, Error – water level is too low, and Disabled – user has disabled the system. 
