//Pin Declarations
const uint8_t button = 2;
const uint8_t blueled = 3;				//LED pin reference
const uint8_t greenled = 5;				//LED pin reference
const uint8_t encoderswitch = 7;	//Encoder pin reference
const uint8_t channelB = 6;			//Encoder pin reference
const uint8_t channelA = 4;			//Encoder pin reference
const uint8_t bluesol = 8;				//Dye injector solenoid
const uint8_t greensol = 9;				//Dye injector solenoid
const uint8_t bluebutton = 10;			//Blue button
const uint8_t greenbutton = 11;			//Green button

//Global Variables
uint8_t menu = 0;					//Used to control menu
uint8_t lastmenu = 1;				//Used to refresh menu
bool blue = false;					//Used to squirt blue dye
bool green = false;					//Used to squirt green dye
bool lastblue = false;				//Used to remember last blue setting
bool lastgreen = false;				//Used to remember last green setting
bool starter = false;				//Used to toggle automatic dye injection
bool laststarter = false;			//Used to remeember starter setting
bool injectorbool = false;			//Used for mutual exclusion in autoinjector loop
bool colorswap = false;				//Used to change color in autoinjector loop
uint8_t blueLength = 1;				//Length of blue dye pulse
uint8_t greenLength = 1;			//Length of green dye pulse
uint8_t altLength = 1;				//Length of each dye pulse when alternating
uint8_t blueDelay = 1;				//Length of blue dye delay
uint8_t greenDelay = 1;				//Length of green dye delay
uint8_t altDelay = 1;				//Length of each dye delay when alternating
uint8_t lastValue = 0;				//Used to refresh adjustable variables
uint8_t injectorVar = 0;			//Used to switch between autoinjector modes
uint32_t elapsedMSdelay = 0;		//Used for autoinjector timing
uint32_t elapsedMSlength = 0;		//Used for autoinjector timing
uint32_t lastDelay = 0;				//Used for autoinjector timing
uint32_t lastLength = 0;			//Used for autoinjector timing

//Encoder Variables
static uint8_t cw_gray_codes[4] = { 2, 0, 3, 1 }; 	//Gray code sequence
static uint8_t ccw_gray_codes[4] = { 1, 3, 0, 2 };  //Gray code sequence
static uint8_t previous_gray_code = 0; //Last gray code.
uint8_t lastpress = HIGH; //Used for encoder button press timing
uint32_t pressTimeStart = 0; //Used for encoder button press timing

//Functions
// LCD Functions
// turn on display
void displayOn() {
	Serial.write(0xFE);
	Serial.write(0x41);
}

// move the cursor to the home position on line 2
void cursorLine2() {
	Serial.write(0xFE);
	Serial.write(0x45);
	Serial.write(0x40); //Hex code for row 2, column 1
}

// move the cursor to the home position on line 2
void cursorTopRight() {
	Serial.write(0xFE);
	Serial.write(0x45);
	Serial.write(0x0F); //Hex code for row 1, column 16
}

// move the cursor to the home position on line 2
void cursorBottomRight() {
	Serial.write(0xFE);
	Serial.write(0x45);
	Serial.write(0x4F); //Hex code for row 2, column 16
}

// move the cursor to the home position
void cursorHome() {
	Serial.write(0xFE);
	Serial.write(0x46);
}

// clear the LCD
void clearLCD() {
	Serial.write(0xFE);
	Serial.write(0x51);
}

// backspace and erase previous character
void backSpace(uint8_t times) {
	for (uint8_t i = 0; i < times; i++)
	{
		Serial.write(0xFE);
		Serial.write(0x4E);
	}
}

// move cursor left
void cursorLeft(uint8_t times) {
	for (uint8_t i = 0; i < times; i++)
	{
		Serial.write(0xFE);
		Serial.write(0x49);
	}
}

// move cursor right
void cursorRight(uint8_t times) {
	for (uint8_t i = 0; i < times; i++)
	{
		Serial.write(0xFE);
		Serial.write(0x4A);
	}
}

// set LCD contrast
void setContrast(uint8_t contrast) {
	Serial.write(0xFE);
	Serial.write(0x52);
	Serial.write(contrast); //Must be between 1 and 50
}

// turn on backlight
void backlightBrightness(uint8_t brightness) {
	Serial.write(0xFE);
	Serial.write(0x53);
	Serial.write(brightness); //Must be between 1 and 8
}

static void check_encoder() // Look for encoder rotation
{
	// Get the Gray-code state of the encoder.
	int gray_code = ((digitalRead(channelA) == HIGH) << 1) | (digitalRead(channelB) == HIGH);
	if (gray_code != previous_gray_code)   //Assign current gray code to last gray code
	{
		//Knob twist CW
		if (gray_code == cw_gray_codes[previous_gray_code])
		{
			if (menu == 0)
			{
				blueLength++;
			}
			else if (menu == 1)
			{
				blueDelay++;
			}
			else if (menu == 2)
			{
				greenLength++;
			}
			else if (menu == 3)
			{
				greenDelay++;
			}
			else if (menu == 4)
			{
				altLength++;
			}
			else if (menu == 5)
			{
				altDelay++;
			}
		}
		//Knob twist CW
		else if (gray_code == ccw_gray_codes[previous_gray_code])
		{
			if (menu == 0)
			{
				blueLength--;
			}
			else if (menu == 1)
			{
				blueDelay--;
			}
			else if (menu == 2)
			{
				greenLength--;
			}
			else if (menu == 3)
			{
				greenDelay--;
			}
			else if (menu == 4)
			{
				altLength--;
			}
			else if (menu == 5)
			{
				altDelay--;
			}
		}
	}
	previous_gray_code = gray_code; //Stores current gray code for future comparison
	constraints(); //Constrain variables being edited
}

void constraints()
{
	blueLength = constrain(blueLength, 1, 60);
	greenLength = constrain(greenLength, 1, 60);
	altLength = constrain(altLength, 1, 60);
	blueDelay = constrain(blueDelay, 1, 60);
	greenDelay = constrain(greenDelay, 1, 60);
	altDelay = constrain(altDelay, 1, 60);
}

// Switch handling
void switchpressed() //Called when encoder button pressed, reads time between falling edge and rising edge of button signal
//Has different press length routines: short and long press
{
	if ((digitalRead(encoderswitch) == LOW) && (lastpress)) //Falling signal edge, happens when button is first pressed
	{
		lastpress = LOW; //Pressed low indicator
		pressTimeStart = millis(); //Start timer
	}
	if ((digitalRead(encoderswitch) == HIGH) && (!lastpress)) //Rising signal edge, happens when button is released
	{
		uint32_t pressTime = millis() - pressTimeStart;
		lastpress = HIGH; //Reset indicator
		if (pressTime < 500) //Short press changes menu
		{
			menu++;
			delay(50);
		}
		else //Long press starts/stops
		{
			if (menu < 2) //Blue controls
			{
				injectorVar = 0;
			}
			else if ((menu >= 2) && (menu < 4)) //Green controls
			{
				injectorVar = 1;
			}
			else //Alternating controls
			{
				injectorVar = 2;
			}
			togglestart();
			delay(50);
		}
	}
}

//Toggle start/stop of auto injection
void togglestart() //Flips start bool when called
{
	if (starter) //Turn everything off
	{
		digitalWrite(bluesol, LOW);
		digitalWrite(greensol, LOW);
		digitalWrite(blueled, LOW);
		digitalWrite(greenled, LOW);
	}
	else
	{
		injectorbool = false;
		colorswap = false;
	}
	starter = !starter; //Toggle starter bool
}

void auto_injector()
{
	switch (injectorVar)
	{
	case 0:
		//Blue Injector Only
		if (starter)
		{
			elapsedMSdelay = (millis() - lastDelay) / 1000; //Time since last pulse convert ms to s
			elapsedMSlength = (millis() - lastLength) / 1000; //Time since start of dye pulse convert ms to s
			//If time elapsed > blueDelay
			if ((elapsedMSdelay >= blueDelay) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
			{
				digitalWrite(bluesol, HIGH); //Squirt dye
				digitalWrite(blueled, HIGH); //Indicator
				lastLength = millis(); //Started dye pulse, start pulse time
				injectorbool = !injectorbool;
			}
			//if time elapsed > blueLength
			else if ((elapsedMSlength > blueLength) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
			{
				digitalWrite(bluesol, LOW); //Stop dye pulse
				digitalWrite(blueled, LOW); //Indicator
				lastDelay = millis();
				injectorbool = !injectorbool;
			}
		}
		break;

	case 1:
		//Green Injector Only
		if (starter)
		{
			elapsedMSdelay = (millis() - lastDelay) / 1000; //Time since last pulse convert ms to s
			elapsedMSlength = (millis() - lastLength) / 1000; //Time since start of dye pulse convert ms to s
			//If time elapsed > greenDelay
			if ((elapsedMSdelay >= greenDelay) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
			{
				digitalWrite(greensol, HIGH); //Squirt dye
				digitalWrite(greenled, HIGH); //Indicator
				lastLength = millis(); //Started dye pulse, start pulse time
				injectorbool = !injectorbool;
			}
			//if time elapsed > greenLength
			else if ((elapsedMSlength > greenLength) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
			{
				digitalWrite(greensol, LOW); //Stop dye pulse
				digitalWrite(greenled, LOW); //Indicator
				lastDelay = millis();
				injectorbool = !injectorbool;
			}
		}
		break;

	case 2:
		//Alternating Injection cycle
		if (starter)
		{
			elapsedMSdelay = (millis() - lastDelay) / 1000; //Time since last pulse convert ms to s
			elapsedMSlength = (millis() - lastLength) / 1000; //Time since start of dye pulse convert ms to s
			if (!colorswap) //Blue
			{
				//If time elapsed > blueDelay
				if ((elapsedMSdelay >= altDelay) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
				{
					digitalWrite(bluesol, HIGH); //Squirt blue dye
					digitalWrite(blueled, HIGH); //Indicator
					lastLength = millis(); //Started dye pulse, start pulse time
					injectorbool = !injectorbool;
				}
				//if time elapsed > blueLength
				else if ((elapsedMSlength > altLength) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
				{
					digitalWrite(bluesol, LOW); //Stop blue dye pulse
					digitalWrite(blueled, LOW); //Indicator
					lastDelay = millis();
					injectorbool = !injectorbool;
					colorswap = !colorswap;
				}
			}
			else //Green
			{
				//If time elapsed > greenDelay
				if ((elapsedMSdelay >= altDelay) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
				{
					digitalWrite(greensol, HIGH); //Squirt dye
					digitalWrite(greenled, HIGH); //Indicator
					lastLength = millis(); //Started dye pulse, start pulse time
					injectorbool = !injectorbool;
				}
				//if time elapsed > greenLength
				else if ((elapsedMSlength > altLength) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
				{
					digitalWrite(greensol, LOW); //Stop dye pulse
					digitalWrite(greenled, LOW); //Indicator
					lastDelay = millis();
					injectorbool = !injectorbool;
					colorswap = !colorswap;
				}
			}
		}
		break;

	default:
		injectorVar = 0;
		break;
	}
}

//Menu Structure
void menuselect()
{
	switch (menu)
	{
	case 0:
		//Blue dye pulse length
		if ((menu != lastmenu) || (blueLength != lastValue))
		{
			clearLCD();
			cursorHome();
			Serial.print("Blue Dye Pulse");
			cursorLine2();
			Serial.print("Length: ");
			Serial.print(blueLength);
			Serial.print(" Sec");
			lastValue = blueLength;
		}
		check_encoder(); //Check rotation
		switchpressed(); //Check button
		auto_injector(); //Run auto injector sequence
		lastmenu = 0;
		break;

	case 1:
		//Blue dye pulse delay
		if ((menu != lastmenu) || (blueDelay != lastValue))
		{
			clearLCD();
			cursorHome();
			Serial.print("Blue Dye Pulse");
			cursorLine2();
			Serial.print("Delay: ");
			Serial.print(blueDelay);
			Serial.print(" Sec");
			lastValue = blueDelay;
		}
		check_encoder(); //Check rotation
		switchpressed(); //Check button
		auto_injector(); //Run auto injector sequence
		lastmenu = 1;
		break;

	case 2:
		//Green dye pulse length
		if ((menu != lastmenu) || (greenLength != lastValue))
		{
			clearLCD();
			cursorHome();
			Serial.print("Green Dye Pulse");
			cursorLine2();
			Serial.print("Length: ");
			Serial.print(greenLength);
			Serial.print(" Sec");
			lastValue = greenLength;
		}
		check_encoder(); //Check rotation
		switchpressed(); //Check button
		auto_injector(); //Run auto injector sequence
		lastmenu = 2;
		break;

	case 3:
		//Green dye pulse delay
		if ((menu != lastmenu) || (greenDelay != lastValue))
		{
			clearLCD();
			cursorHome();
			Serial.print("Green Dye Pulse");
			cursorLine2();
			Serial.print("Delay: ");
			Serial.print(greenDelay);
			Serial.print(" Sec");
			lastValue = greenDelay;
		}
		check_encoder(); //Check rotation
		switchpressed(); //Check button
		auto_injector(); //Run auto injector sequence
		lastmenu = 3;
		break;

	case 4:
		//Alternating dye pulse length
		if ((menu != lastmenu) || (altLength != lastValue))
		{
			clearLCD();
			cursorHome();
			Serial.print("Alt Dye Pulse");
			cursorLine2();
			Serial.print("Length: ");
			Serial.print(altLength);
			Serial.print(" Sec");
			lastValue = altLength;
		}
		check_encoder(); //Check rotation
		switchpressed(); //Check button
		auto_injector(); //Run auto injector sequence
		lastmenu = 4;
		break;

	case 5:
		//Alternating dye pulse delay
		if ((menu != lastmenu) || (altDelay != lastValue))
		{
			clearLCD();
			cursorHome();
			Serial.print("Alt Dye Pulse");
			cursorLine2();
			Serial.print("Delay: ");
			Serial.print(altDelay);
			Serial.print(" Sec");
			lastValue = altDelay;
		}
		check_encoder(); //Check rotation
		switchpressed(); //Check button
		auto_injector(); //Run auto injector sequence
		lastmenu = 5;
		break;

	default:
		menu = 0;
		break;
	}
}

//Setup
void setup()
{
	pinMode(blueled, OUTPUT);
	pinMode(greenled, OUTPUT);
	pinMode(bluesol, OUTPUT);
	pinMode(greensol, OUTPUT);
	pinMode(bluebutton, INPUT_PULLUP);
	pinMode(greenbutton, INPUT_PULLUP);
	pinMode(encoderswitch, INPUT_PULLUP);
	pinMode(channelB, INPUT_PULLUP); //Changed from input to input_pullup for model #62A11-02-040CH
	pinMode(channelA, INPUT_PULLUP); //Changed from input to input_pullup for model #62A11-02-040CH
	Serial.begin(9600);
	displayOn();
	setContrast(45);
	backlightBrightness(8);
	clearLCD();
	cursorHome();
	Serial.print("Dye Injector");
	cursorLine2();
	Serial.print("Version 2.0");
	delay(1500);
	clearLCD();
	cursorHome();
	Serial.print("Little River R&D");
	cursorLine2();
	Serial.print("www.EMRIVER.com");
	delay(1500);
	clearLCD();
}

//Main Loop
void loop()
{
	menuselect(); //Run menu selection
	uint8_t bluestate = digitalRead(bluebutton);
	uint8_t greenstate = digitalRead(greenbutton);
	if (bluestate == 0) //Low = pressed (input pullup)
	{
		blue = true;
	}
	else //High = not pressed (input pullup)
	{
		blue = false;
	}
	if (greenstate == 0) //Low = pressed (input pullup)
	{
		green = true;
	}
	else //High = not pressed (input pullup)
	{
		green = false;
	}
	//Check for button input
	if ((blue != lastblue) || (green != lastgreen))
	{
		//Pause automatic injection
		starter = false;

		//Detect which changed
		if ((blue) && (!lastblue)) //Blue pressed
		{
			//blue changed
			digitalWrite(bluesol, HIGH);
			digitalWrite(blueled, HIGH);
			lastblue = true;
		}
		else if ((!blue) && (lastblue)) //Blue released
		{
			//blue changed
			digitalWrite(bluesol, LOW);
			digitalWrite(blueled, LOW);
			lastblue = false;
			if (laststarter) //If auto-injector was paused, start it again
			{
				!starter;
			}
		}
		if ((green) && (!lastgreen)) //Green pressed
		{
			//green changed
			digitalWrite(greensol, HIGH);
			digitalWrite(greenled, HIGH);
			lastgreen = true;
		}
		else if ((!green) && (lastgreen)) //Green released
		{
			//green changed
			digitalWrite(greensol, LOW);
			digitalWrite(greenled, LOW);
			lastgreen = false;
			if (laststarter) //If auto-injector was paused, start it again
			{
				!starter;
			}
		}
	}
}
