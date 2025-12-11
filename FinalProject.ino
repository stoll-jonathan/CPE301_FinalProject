/*  Jonathan Stoll
    CPE 301 Final Project
    
    could not complete: vent stepper motor control
    did not attempt: fan control
*/

/*
  Pin Setups
  ----------
  LCD: digital 13, 12, 11, 10, 9, 8
  DHT11: digital 6
  Water Sensor: analog 15
  LEDs (all PB): red 0, yellow 1, green 2, blue 3
  RTC: SDA and SLC (digital 20 and 21)
  Disable Button: digital 2
  Up and down buttons: digital 3, 4
  Stepper motor: digital 42, 44, 46, 48
*/

#include <LiquidCrystal.h>
#include <DHT11.h>
#include <string.h>
#include "RTClib.h"

// buttons
volatile bool DISABLED = false;
volatile unsigned char* ddr_d  = (unsigned char*) 0x2A;
volatile unsigned char* port_d = (unsigned char*) 0x2B;
volatile unsigned char* ddr_e = (unsigned char*) 0x2C;
volatile unsigned char* port_e = (unsigned char*) 0x2D;
volatile unsigned char* ddr_g = (unsigned char*) 0x2E;
volatile unsigned char* port_g = (unsigned char*) 0x2F;
int disabledButtonPin = 2; // pd
int upbutton = 5; // pe
int downbutton = 5; // pg

// LCD pins <--> Arduino pins
const int RS = 13, EN = 12, D4 = 11, D5 = 10, D6 = 9, D7 = 8;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// DHT11 temp & humidity sensor
DHT11 dht11(6);
bool firstReading;
int TemperatureReading, HumidityReading;

// Real Time Clock
RTC_DS1307 rtc;
DateTime timeForNextReading;

// Stepper Motor (all PL)
volatile unsigned char* ddr_l  = (unsigned char*) 0xA0;
volatile unsigned char* port_l = (unsigned char*) 0xA1;
volatile unsigned char* pin_e = (unsigned char*) 0x2C;
volatile unsigned char* pin_g = (unsigned char*) 0x2E;
const int stepperpin1 = 7;
const int stepperpin2 = 5;
const int stepperpin3 = 3;
const int stepperpin4 = 1;
const uint8_t stepperSequence[4] = {
  0b0001, // pin1
  0b0010, // pin2
  0b0100, // pin3
  0b1000  // pin4
};
int stepIndex = 0;

// LEDs (all PB ports)
unsigned char* ddr_b = (unsigned char*) 0x24;
unsigned char* port_b = (unsigned char*) 0x25;
int redPin = 0;
int yellowPin = 1;
int greenPin = 2;
int bluePin = 3;

// UART
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;

// Water Sensor and ADC
#define RDA 0x80
#define TBE 0x20  
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int*  my_ADC_DATA = (unsigned int*) 0x78;
int WaterLevelReading;


void setup() {
  // setup the UART
  U0init(9600);

  // setup the ADC
  adc_init();

  // setup the DHT11 and water sensor
  firstReading = true;
  TemperatureReading = 0;
  HumidityReading = 0;
  WaterLevelReading = 0;

  // setup the LCD
  lcd.begin(16, 2); // cols, rows
  lcd.clear();

  // setup output pins
  *ddr_b |= (1 << redPin);
  *ddr_b |= (1 << yellowPin);
  *ddr_b |= (1 << greenPin);
  *ddr_b |= (1 << bluePin);
  *ddr_l |= (1 << stepperpin1);
  *ddr_l |= (1 << stepperpin2);
  *ddr_l |= (1 << stepperpin3);
  *ddr_l |= (1 << stepperpin4);

  // setup input pins
  *ddr_d &= ~(1 << disabledButtonPin);
  *ddr_e &= ~(1 << upbutton);
  *ddr_g &= ~(1 << downbutton);

  // set all LEDs to off by default
  setBlue(0);
  setGreen(0);
  setYellow(0);
  setRed(0);

  // setup RTC
  if (!rtc.begin()) {
    U0print("RTC FAIL\n");
  }
  if (!rtc.isrunning()) {
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // Setup enable/disable ISR
  attachInterrupt(digitalPinToInterrupt(2), toggleDisabled, FALLING);
}

void loop() {
  char state = ' ';
  static int dht11Result = 0;

  static unsigned long lastButton = 0;
  unsigned long now = millis();
  if(now - lastButton > 200) {
    if (isUpPressed()) { 
      stepStepper(true); 
      lastButton = now; 
    }
    else if (isDownPressed()) { 
      stepStepper(false); 
      lastButton = now; 
    }
  }


  // for convienience, print state of the system every second
  static unsigned long lastSerialTime = 0;
  if (now - lastSerialTime >= 1000) {  // 1000 ms = 1 second
    lastSerialTime = now;
    WaterLevelReading = adc_read(15);
    printStatusToSerial(WaterLevelReading);
  }

  if (DISABLED) {
    fanMotor(false);

    setBlue(0);
    setGreen(0);
    setYellow(1);
    setRed(0);

    lcd.setCursor(0, 0);
    lcd.print("System Disabled ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
  }
  else {
    WaterLevelReading = adc_read(15);

    // Check water level
    if (WaterLevelReading < 15) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Water is too low");
      lcd.setCursor(0, 1);
      lcd.print("Water Level: "); lcd.print(WaterLevelReading);
      state = 'E';
    }
    else {
      // Read DHT11 if time
      if (rtc.now() >= timeForNextReading || firstReading) {
        firstReading = false;
        
        int result = dht11.readTemperatureHumidity(TemperatureReading, HumidityReading);
        if (result == 0) { // success
          dht11Result = 0;
          timeForNextReading = rtc.now() + TimeSpan(0, 0, 1, 0); // 1 min from now
        }
        else {
          dht11Result = 1; // error
        }
      }

      // Update LCD
      lcd.clear();
      if (dht11Result != 0) {
        lcd.setCursor(0, 0);
        lcd.print("DHT11 error");
        state = 'E';
      } 
      else {
        lcd.setCursor(0, 0);
        lcd.print("Temperature: "); lcd.print(TemperatureReading);
        lcd.setCursor(0, 1);
        lcd.print("Humidity: "); lcd.print(HumidityReading);
      }

      
      if (TemperatureReading >= 25) {
        state = 'R';
      }
      else {
        state = 'I';
      }
    }
  }

  // Set LEDs
  if (state == 'R') { // running
    setBlue(1);
    setGreen(0);
    setYellow(0);
    setRed(0);
    
    fanMotor(true);
  }
  else if (state == 'I') { // idle
    setBlue(0);
    setGreen(1);
    setYellow(0);
    setRed(0);

    fanMotor(false);
  }
  else if (state == 'E') { // error
    setBlue(0);
    setGreen(0);
    setYellow(0);
    setRed(1);

    fanMotor(false);
  }

}

void fanMotor(bool a) {
  if (a) {
    // turn on
  }
  else {
    //turn off
  }
}

void toggleDisabled() {
  static unsigned long last = 0;
  unsigned long now = millis();

  // debouncing: ignore events <200 ms apart
  if (now - last > 200) {
    DISABLED = !DISABLED;
    last = now;
  }
}

void stepStepper(bool forward) {
    if(forward){
        stepIndex = (stepIndex + 1) % 4;
    } 
    else {
        stepIndex = (stepIndex + 3) % 4; // -1 modulo 4
    }
    uint8_t step = stepperSequence[stepIndex];
    // Set stepper pins
    if(step & 0b0001) *port_l |= (1 << stepperpin1); else *port_l &= ~(1 << stepperpin1);
    if(step & 0b0010) *port_l |= (1 << stepperpin2); else *port_l &= ~(1 << stepperpin2);
    if(step & 0b0100) *port_l |= (1 << stepperpin3); else *port_l &= ~(1 << stepperpin3);
    if(step & 0b1000) *port_l |= (1 << stepperpin4); else *port_l &= ~(1 << stepperpin4);
}

bool isUpPressed() {
    return !(*pin_e & (1 << upbutton));
}

bool isDownPressed() {
    return !(*pin_g & (1 << downbutton));
}

void setBlue(int on) {
  if (on == 1)
    *port_b |= (1 << bluePin);
  else
    *port_b &= ~(1 << bluePin);
}

void setGreen(int on) {
  if (on == 1)
    *port_b |= (1 << greenPin);
  else
    *port_b &= ~(1 << greenPin);
}

void setYellow(int on) {
  if (on == 1)
    *port_b |= (1 << yellowPin); // turn LED on
  else
    *port_b &= ~(1 << yellowPin);  // turn LED off
}

void setRed(int on) {
  if (on == 1)
    *port_b |= (1 << redPin);
  else
    *port_b &= ~(1 << redPin);
}

void printStatusToSerial(int WaterLevelReading) {

  U0printInt(rtc.now().hour()); U0putchar(':'); U0printInt(rtc.now().minute()); U0putchar(':'); U0printInt(rtc.now().second());
  U0print("\nNext Reading at ");
  U0printInt(timeForNextReading.hour()); U0putchar(':'); U0printInt(timeForNextReading.minute()); U0putchar(':'); U0printInt(timeForNextReading.second());

  U0print("\nwater level: ");
  U0printInt(WaterLevelReading);
  U0print("\ntemperature: ");
  U0printInt(TemperatureReading);
  U0print("\nhumidity: ");
  U0printInt(HumidityReading);
  U0putchar('\n');
  U0putchar('\n');

}

void U0printInt(int num) {
  char buffer[10];
  sprintf(buffer, "%d", num);
  U0print(buffer);
}

void U0print(char *str) {
  while (*str != '\0') {   // loop until end of string
    U0putchar(*str);       // send each character
    str++;                 // move to next character
  }
}

void adc_init() {
  // setup the A register
  *my_ADCSRA |= 0b10000000;
  *my_ADCSRA &= 0b11011111;
  *my_ADCSRA &= 0b11110111;
  *my_ADCSRA &= 0b11111000;


  // setup the B register
  *my_ADCSRB &= 0b11110111;
  *my_ADCSRB &= 0b11111000;


  // setup the MUX Register
  *my_ADMUX &= 0b11011111;
  *my_ADMUX &= 0b11100000;
  *my_ADMUX &= 0b01111111;
  *my_ADMUX |= 0b01000000;
}

unsigned int adc_read(unsigned char adc_channel) {
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX &= 0b11100000;

  // clear the channel selection bits (MUX 5) hint: it's not in the ADMUX register
  *my_ADCSRB &= 0b11110111;
 
  // set the channel number
  if (adc_channel > 7) {
    *my_ADCSRB |= 0b00001000;  // Set MUX5 for channels 8-15
    adc_channel -= 8;
  }
  *my_ADMUX |= (adc_channel & 0b00000111);  // Set MUX4:0

  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0b01000000;

  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);

  // return the result in the ADC data register and format the data based on right justification (check the lecture slide)
  unsigned int val = *my_ADC_DATA;

  return val;
}

void U0init(unsigned long U0baud) {
  unsigned long FCPU = 16000000;
  unsigned int tbaud;
  tbaud = (FCPU / 16 / U0baud - 1);
  // Same as (FCPU / (16 * U0baud)) - 1;
  *myUCSR0A = 0x20;
  *myUCSR0B = 0x18;
  *myUCSR0C = 0x06;
  *myUBRR0  = tbaud;
}

unsigned char U0kbhit() {
  return *myUCSR0A & RDA;
}

unsigned char U0getchar() {
  return *myUDR0;
}

void U0putchar(unsigned char U0pdata) {
  while((*myUCSR0A & TBE) == 0); // wait until tx buffer is empty (bit 5 of UCSR0A is HIGH)
  *myUDR0 = U0pdata;
}
