#include <Arduino.h>
#include <avr/eeprom.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <ctype.h>

#define bit9600Delay 84  
#define halfBit9600Delay 42
#define bit4800Delay 188 
#define halfBit4800Delay 94

// These are commands we need to send to HSHT15 to control it
int gTempCmd = 0b00000011;
int gHumidCmd = 0b00000101;

char cmd = 0;
int ack;

// For the clock.
int dateTimePin=10; //chip select 
// For hum. temp.
int htDataPin = 2;
int htClockPin = 3;
// Serial Display.  
int displayRx = 14;
int displayTx = 15;

int loggerRx = 16;
int loggerTx = 17;
int loggerReset = 18;

int fanPin1 = 9;
int heatPin1 = 4;
int heatPin2 = 5;
int coolPin = 6;

double targetTemp = 91;
double tempVariance = .1;

SoftwareSerial serial(displayRx, displayTx);
SoftwareSerial logger(loggerRx, loggerTx);

// Initiates a function command to the display
void serCommand() {
	serial.write(0xFE);
}

// Sets the cursor to the given position
// line 1: 0-15, line 2: 16-31, 31+ defaults back to 0
void goTo(int position) {
	if (position < 16) {
		serCommand(); //command flag
		serial.write((position+128));
	} else if (position < 32) {
		serCommand(); //command flag
		serial.write((position+48+128));
	} else {
		goTo(0);
	}
}

// Scrolls the display left by the number of characters passed in, and waits a given
// number of milliseconds between each step
void scrollLeft(int num, int wait) {
	for (int i=0; i<num; i++) {
		serCommand();
		serial.write(0x18);
		delay(wait);
	}
}

// Scrolls the display right by the number of characters passed in, and waits a given
// number of milliseconds between each step
void scrollRight(int num, int wait) {
	for (int i=0; i<num; i++) {
		serCommand();
		serial.write(0x1C);
		delay(wait);
	}
}

// Starts the cursor at the beginning of the first line (convienence method for goTo(0))
void selectLineOne() { //puts the cursor at line 0 char 0.
	serCommand(); //command flag
	serial.write(128); //position
}

// Starts the cursor at the beginning of the second line (convienence method for goTo(16))
void selectLineTwo() { //puts the cursor at line 0 char 0.
	serCommand(); //command flagb   
	serial.write(192); //position
}

// Resets the display, undoing any scroll and removing all text
void clearLCD() {
	serCommand();
	serial.write(0x01);
}

// Turns the backlight on
void backlightOn() {
	serial.write(124);
	serial.write(157);
}

// Turns the backlight off
void backlightOff() {
	serial.write(124);
	serial.write(128);
}

int times = 0;

int RTC_init() {
	pinMode(dateTimePin, OUTPUT); // chip select
	// start the SPI library:

	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE3); // both mode 1 & 3 should work 
	//set control register 
	digitalWrite(dateTimePin, LOW);
	SPI.transfer(0x8E);
	SPI.transfer(0x60); //60= disable Osciallator and Battery SQ wave @1hz, temp compensation, Alarms disabled
	digitalWrite(dateTimePin, HIGH);
	delay(100);
}

//=====================================
int SetTimeDate(int d, int mo, int y, int h, int mi, int s) {
	int TimeDate [7]= { s, mi, h, 0, d, mo, y };
	for (int i=0; i<=6; i++) {
		if (i==3)
			i++;
		int b= TimeDate[i]/10;
		int a= TimeDate[i]-b*10;
		if (i==2) {
			if (b==2) {
				b=B00000010;
			} else if (b==1) {

				b=B00000001;
			}
		}

		TimeDate[i]= a+(b<<4);

		digitalWrite(dateTimePin, LOW);
		SPI.transfer(i+0x80);
		SPI.transfer(TimeDate[i]);
		digitalWrite(dateTimePin, HIGH);
	}
}

void setup() {

	pinMode(loggerTx, OUTPUT);
	pinMode(loggerRx, INPUT);

	pinMode(fanPin1, OUTPUT);
	pinMode(heatPin1, OUTPUT); // set pin to input
	pinMode(heatPin2, OUTPUT); // set pin to input
	pinMode(coolPin, OUTPUT); // set pin to input

	analogWrite(9, HIGH);

	digitalWrite(heatPin1, HIGH); // turn on pullup resistors (Heat off.)
	digitalWrite(heatPin2, HIGH); // turn on pullup resistors (Heat off.)
	digitalWrite(coolPin, HIGH); // turn on pullup resistors (Cool off.)


	serial.begin(9600);
	logger.begin(9600);
	Serial.begin(9600);

	delay(1000);

	RTC_init();

	//SetTimeDate(2, 13, 13, 17, 28, 0);
	//backlightOn();
	clearLCD();
}

//=====================================
String ReadTimeDate() {
	String time = "";
	int TimeDate [7]; //second,minute,hour,null,day,month,year		
	for (int i=0; i<=6; i++) {
		if (i==3)
			i++;
		digitalWrite(dateTimePin, LOW);
		SPI.transfer(i+0x00);
		unsigned int n = SPI.transfer(0x00);
		digitalWrite(dateTimePin, HIGH);

		//delay(100);

		int a = n & B00001111;
		if (i==2) {
			int b=(n & B00110000)>>4; //24 hour mode
			if (b==B00000010)
				b=20;
			else if (b==B00000001)
				b=10;
			TimeDate[i]=a+b;
		} else if (i==4) {
			int b=(n & B00110000)>>4;
			TimeDate[i]=a+b*10;
		} else if (i==5) {
			int b=(n & B00010000)>>4;
			TimeDate[i]=a+b*10;
		} else if (i==6) {
			int b=(n & B11110000)>>4;
			TimeDate[i]=a+b*10;
		} else {
			int b=(n & B01110000)>>4;
			TimeDate[i]=a+b*10;
		}
	}
	time.concat("20") ;
	time.concat(TimeDate[6]);
	time.concat("-") ;
	if (TimeDate[5] < 10) {
		time.concat("0") ;
	}
	time.concat(TimeDate[5]);
	time.concat("-") ;

	if (TimeDate[4] < 10) {
		time.concat("0") ;
	}

	time.concat(TimeDate[4]);

	time.concat(" ");

	if (TimeDate[2] < 10) {
		time.concat(" ") ;
	}
	time.concat(TimeDate[2]);
	time.concat(":") ;
	if (TimeDate[1] < 10) {
		time.concat("0") ;
	}

	time.concat(TimeDate[1]);

	return (time);
}

int shiftIn(int dataPin, int clockPin, int numBits) {
	int ret = 0;
	int i;

	for (i=0; i<numBits; ++i) {
		digitalWrite(clockPin, HIGH);
		delay(10); // I don't know why I need this, but without it I don't get my 8 lsb of temp
		ret = ret*2 + digitalRead(dataPin);
		digitalWrite(clockPin, LOW);
	}

	return (ret);
}

void sendCommandSHT(int command, int dataPin, int clockPin) {
	int ack;

	// Transmission Start
	pinMode(dataPin, OUTPUT);
	pinMode(clockPin, OUTPUT);
	digitalWrite(dataPin, HIGH);
	digitalWrite(clockPin, HIGH);
	digitalWrite(dataPin, LOW);
	digitalWrite(clockPin, LOW);
	digitalWrite(clockPin, HIGH);
	digitalWrite(dataPin, HIGH);
	digitalWrite(clockPin, LOW);

	// The command (3 msb are address and must be 000, and last 5 bits are command)
	shiftOut(dataPin, clockPin, MSBFIRST, command);

	// Verify we get the coorect ack
	digitalWrite(clockPin, HIGH);
	pinMode(dataPin, INPUT);
	ack = digitalRead(dataPin);
	if (ack != LOW)
		serial.println("Ack Error 0");
	digitalWrite(clockPin, LOW);
	ack = digitalRead(dataPin);
	if (ack != HIGH)
		serial.println("Ack Error 1");
}

void waitForResultSHT(int dataPin) {
	int i;
	int ack;

	pinMode(dataPin, INPUT);

	for (i= 0; i < 100; ++i) {
		delay(10);
		ack = digitalRead(dataPin);

		if (ack == LOW)
			break;
	}

	if (ack == HIGH)
		serial.println("Ack Error 2");
}

int getData16SHT(int dataPin, int clockPin) {
	int val;

	// Get the most significant bits
	pinMode(dataPin, INPUT);
	pinMode(clockPin, OUTPUT);
	val = shiftIn(dataPin, clockPin, 8);
	val *= 256;

	// Send the required ack
	pinMode(dataPin, OUTPUT);
	digitalWrite(dataPin, HIGH);
	digitalWrite(dataPin, LOW);
	digitalWrite(clockPin, HIGH);
	digitalWrite(clockPin, LOW);

	// Get the lest significant bits
	pinMode(dataPin, INPUT);
	val |= shiftIn(dataPin, clockPin, 8);

	return val;
}

void skipCrcSHT(int dataPin, int clockPin) {
	// Skip acknowledge to end trans (no CRC)
	pinMode(dataPin, OUTPUT);
	pinMode(clockPin, OUTPUT);

	digitalWrite(dataPin, HIGH);
	digitalWrite(clockPin, HIGH);
	digitalWrite(clockPin, LOW);
}

double readTemp() {
	int val;
	double temp;
	sendCommandSHT(gTempCmd, htDataPin, htClockPin);
	waitForResultSHT(htDataPin);
	val = getData16SHT(htDataPin, htClockPin);
	skipCrcSHT(htDataPin, htClockPin);
	temp = -40.0 + 0.018 * (float)val;
	return temp;
}

void printDouble(double val, unsigned int precision) {
	// prints val with number of decimal places determine by precision
	// NOTE: precision is 1 followed by the number of zeros for the desired number of decimial places
	// example: printDouble( 3.1415, 100); // prints 3.14 (two decimal places)

	Serial.print(int(val)); //prints the int part
	Serial.print("."); // print the decimal point
	unsigned int frac;
	if (val >= 0)
		frac = (val - int(val)) * precision;
	else
		frac = (int(val)- val ) * precision;
	Serial.println(frac, DEC) ;
}

int readHumidity() {

	int val;
	int humid;

	sendCommandSHT(gHumidCmd, htDataPin, htClockPin);
	waitForResultSHT(htDataPin);
	val = getData16SHT(htDataPin, htClockPin);
	skipCrcSHT(htDataPin, htClockPin);

	humid = -4.0 + 0.0405 * val + -0.0000028 * val * val;

	return humid;
}

bool cool= false;
bool coolOn= false;
bool heat= false;
bool heatOn= false;
bool fanOn= false;

void turnHeatOn() {
	coolOn = false;
	if (heatOn) {
		return;
	}

	digitalWrite(coolPin, HIGH);

	delay(500);

	digitalWrite(heatPin2, LOW);
	digitalWrite(heatPin1, LOW);

	fanOn = true;

	heatOn = true;

}

void turnCoolOn() {
	heatOn = false;
	if (coolOn) {
		return;
	}
	digitalWrite(heatPin1, HIGH);
	digitalWrite(heatPin2, HIGH);

	//	delay(500);
	//	digitalWrite(coolPin, LOW);
	//	digitalWrite(heatPin1, LOW);
	fanOn = true;
	coolOn = true;

}

void turnHeatAndCoolOff() {
	if (heatOn) {
		digitalWrite(heatPin1, HIGH);
		digitalWrite(heatPin2, HIGH);
		heatOn = false;
	}
	if (coolOn) {
		digitalWrite(coolPin, HIGH);
		coolOn = false;
	}

	if (fanOn) {
		//
		fanOn = false;
	}

}

void loop() {

	String dateTime = ReadTimeDate();
	double temperature = readTemp();
	int humidity = readHumidity();

	Serial.println(dateTime);

	delay(50);
	selectLineOne();
	serial.print(dateTime);
	delay(50);

	logger.print(dateTime);
	logger.print("\t");
	logger.print(times);
	logger.print("\t");
	logger.print(temperature);
	logger.print("\t");
	logger.print(String(humidity, DEC));
	logger.println("\n");

	selectLineTwo();

	serial.print(" ");

	serial.print(temperature);

	serial.print("F ");

	serial.print(String(humidity, DEC));

	double targetMin = targetTemp - tempVariance;
	double targetMax = targetTemp + tempVariance;

	if (temperature > targetMax) {
		turnCoolOn();

		serial.print(" OFF ");
		//serial.print(" COOL");

	}

	if (temperature < targetMin) {

		turnHeatOn();
		serial.print(" HEAT");

	}

	if (temperature > (targetMin + tempVariance) && temperature < (targetMax)) {

		turnHeatAndCoolOff();
		serial.print(" OFF ");

	}

	times ++;
	delay(500);

}

