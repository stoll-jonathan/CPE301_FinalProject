/* TODO: make temp and humidity update every minute, add vent stepper motor control, add fan control, add LEDs
*/

// Jonathan Stoll
// CPE 301 Final Project

/*
  Pin Setups
  ----------
  LCD: digital 13, 12, 11, 10, 9, 8
  DHT11: digital 53
  Water Sensor: analog 15
  
*/

#include <LiquidCrystal.h>
#include <DHT11.h>
#include <I2C_RTC.h>
#include <string.h>

// LCD pins <--> Arduino pins
const int RS = 13, EN = 12, D4 = 11, D5 = 10, D6 = 9, D7 = 8;
LiquidCrystal lcd(RS, EN, D4, D5, D6, D7);

// DHT11 temp & humidity sensor
DHT11 dht11(53); // digital pin 53

// Real Time Clock
static DS1307 RTC;

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


void setup() {
  // setup the UART
  U0init(9600);

  // setup the ADC
  adc_init();

  // setup the LCD
  lcd.begin(16, 2); // cols, rows
  lcd.clear();
}

void loop() {
  char state;

  // Gather data
  int WaterLevelReading = adc_read(15); // analog pin 15
  int TemperatureReading = 0;
  int HumidityReading = 0;
  int dht11Result = dht11.readTemperatureHumidity(TemperatureReading, HumidityReading); // writes readings to TemperatureReading and HumidityReading and stores return status in dht11Result

  // Output water level to serial monitor for debugging
  U0printInt(WaterLevelReading);
  U0putchar('\n');

  // Output data to LCD
  if (WaterLevelReading < 15) {
    lcd.setCursor(0, 0);
    lcd.print("Water is too low");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    state = 'e'; // error
  }
  else if (dht11Result == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: "); lcd.print(TemperatureReading);

    lcd.setCursor(0, 1);
    lcd.print("Humidity: "); lcd.print(HumidityReading);

    // if temp good set to idle 'i' and set fan to off
    // if temp no good set to running 'r' and set fan to on
  }
  else {
    lcd.print(DHT11::getErrorString(dht11Result));
    state = 'e'; // error
  }


  
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
