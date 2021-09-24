//Dye Injector v2.0
//Mason Parrone' 1/9/2019
//K500 PCB
//Two solenoids
//Modes are Green, Blue, or Alternating
//Button presses inject that color at any time
//Auto injector mode for each of three modes with run time display and cycle time
//Updated timer function so after 99 hrs restarts at 0
//Changed from dye pulse delay to dye pulse interval
//Dye pulse length cannot be greater than dye pulse interval

//Pin Declarations
const uint8_t button = 2;
const uint8_t blueled = 3;				  //LED pin reference
const uint8_t greenled = 5;				  //LED pin reference
const uint8_t encoderswitch = 7;	  //Encoder pin reference
const uint8_t channelB = 6;			    //Encoder pin reference
const uint8_t channelA = 4;			    //Encoder pin reference
const uint8_t bluesol = 9;				  //Dye injector solenoid
const uint8_t greensol = 8;				  //Dye injector solenoid
const uint8_t bluebutton = 10;			//Blue button
const uint8_t greenbutton = A0;			//Green button

//Global Variables
uint8_t menu = 0;					          //Used to control menu
uint8_t lastmenu = 1;				        //Used to refresh menu
bool blue = false;					        //Used to squirt blue dye
bool green = false;					        //Used to squirt green dye
bool lastblue = false;				      //Used to remember last blue setting
bool lastgreen = false;				      //Used to remember last green setting
bool starter = false;				        //Used to toggle automatic dye injection
bool starterstate = false;          //Used to remember starter state
bool laststarter = false;			      //Used to remember starter setting
bool injectorbool = false;			    //Used for mutual exclusion in autoinjector loop
bool colorswap = false;				      //Used to change color in autoinjector loop
uint16_t pulseLength = 1;				    //Length of blue dye pulse
uint16_t lastPulseLength = 0;       //Used to update pulse delay
uint8_t lengthDefault = 3;          //Default dye pulse length
uint16_t pulseDelay = 1;				    //Length of blue dye delay
uint16_t lastPulseDelay = 0;        //Used to update pulse delay
uint8_t delayDefault = 15;          //Default dye pulse delay
uint8_t lastValue = 0;				      //Used to refresh adjustable variables
uint8_t injectorVar = 0;			      //Used to switch between autoinjector modes
uint32_t elapsedMSdelay = 0;		    //Used for autoinjector timing
uint32_t elapsedMSlength = 0;		    //Used for autoinjector timing
uint32_t lastDelay = 0;				      //Used for autoinjector timing
uint32_t lastLength = 0;			      //Used for autoinjector timing
bool pressed = false;               //Button pressed
bool increasing = false;            //Knob rotated CW
bool decreasing = false;            //Knob rotated CCW
bool bootup = true;                 //Instrument just booted
bool edit = false;                  //Toggle edit mode
bool automatic = false;             //Toggle automatic
uint32_t elapsedTimerMS = 0;        //Time since last timer refresh
uint32_t injectStartTime = 0;       //MS at start of auto injector
uint32_t totalSec;                                  //Used for timer
uint16_t hour;                                      //""
uint16_t minute;                                    //""
uint16_t second;                                    //""

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

void checkknob() // Look for encoder rotation
{
  // Get the Gray-code state of the encoder.
  int gray_code = ((digitalRead(channelA) == HIGH) << 1) | (digitalRead(channelB) == HIGH);
  if (gray_code != previous_gray_code)   //Assign current gray code to last gray code
  {
    if (bootup)
    {
      bootup = false;
    }
    else
    {
      //Knob twist CW
      if (gray_code == cw_gray_codes[previous_gray_code])
      {
        increasing = true;
      }
      //Knob twist CW
      else if (gray_code == ccw_gray_codes[previous_gray_code])
      {
        decreasing = true;
      }
    }
  }
  previous_gray_code = gray_code; //Stores current gray code for future comparison
}

// Switch handling
void checkbutton() //Called when encoder button pressed, reads time between falling edge and rising edge of button signal
//Has different press length routines: short and long press
{
  if ((digitalRead(encoderswitch) == LOW) && (lastpress)) //Falling signal edge, happens when button is first pressed
  {
    lastpress = LOW; //Pressed low indicator
  }
  if ((digitalRead(encoderswitch) == HIGH) && (!lastpress)) //Rising signal edge, happens when button is released
  {
    lastpress = HIGH;
    pressed = true;
    delay(10);
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
  delay(10);
}

void auto_injector()
{
  switch (injectorVar)
  {
    case 0:
      //Blue Injector Only
      if (starter)
      {
        elapsedMSdelay = millis() - lastDelay; //Time since last pulse convert ms to s
        elapsedMSlength = millis() - lastLength; //Time since start of dye pulse convert ms to s
        //If time elapsed > pulseDelay
        if ((elapsedMSdelay >= ((pulseDelay - pulseLength) * 1000)) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
        {
          digitalWrite(bluesol, HIGH); //Squirt dye
          digitalWrite(blueled, HIGH); //Indicator
          lastLength = millis(); //Started dye pulse, start pulse time
          injectorbool = !injectorbool;
        }
        //if time elapsed > pulseLength
        else if ((elapsedMSlength >= (pulseLength * 1000)) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
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
        elapsedMSdelay = millis() - lastDelay; //Time since last pulse convert ms to s
        elapsedMSlength = millis() - lastLength; //Time since start of dye pulse convert ms to s
        //If time elapsed > pulseDelay
        if ((elapsedMSdelay >= ((pulseDelay - pulseLength) * 1000)) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
        {
          digitalWrite(greensol, HIGH); //Squirt dye
          digitalWrite(greenled, HIGH); //Indicator
          lastLength = millis(); //Started dye pulse, start pulse time
          injectorbool = !injectorbool;
        }
        //if time elapsed > pulseLength
        else if ((elapsedMSlength >= (pulseLength * 1000)) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
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
        elapsedMSdelay = millis() - lastDelay; //Time since last pulse convert ms to s
        elapsedMSlength = millis() - lastLength; //Time since start of dye pulse convert ms to s
        if (!colorswap) //Blue
        {
          //If time elapsed > pulseDelay
          if ((elapsedMSdelay >= ((pulseDelay - pulseLength) * 1000)) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
          {
            digitalWrite(bluesol, HIGH); //Squirt blue dye
            digitalWrite(blueled, HIGH); //Indicator
            lastLength = millis(); //Started dye pulse, start pulse time
            injectorbool = !injectorbool;
          }
          //if time elapsed > pulseLength
          else if ((elapsedMSlength >= (pulseLength * 1000)) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
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
          //If time elapsed > pulseDelay
          if ((elapsedMSdelay >= ((pulseDelay - pulseLength) * 1000)) && (!injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
          {
            digitalWrite(greensol, HIGH); //Squirt dye
            digitalWrite(greenled, HIGH); //Indicator
            lastLength = millis(); //Started dye pulse, start pulse time
            injectorbool = !injectorbool;
          }
          //if time elapsed > pulseLength
          else if ((elapsedMSlength >= (pulseLength * 1000)) && (injectorbool)) //Injector bool creates mutual exclusion, either it is looking at blue delay, or blue length
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

void adjustment() //Adjust pulse length and pulse delay
{
  //Pulse delay adjustment
  clearLCD();
  cursorHome();
  Serial.print(F("Dye Injection"));
  cursorLine2();
  Serial.print(F("Interval "));
  Serial.print(pulseDelay);
  Serial.print(F(" s"));
  edit = true;
  while (edit)
  {
    checkknob();
    checkbutton();
    manual();
    if (increasing)
    {
      increasing = false;
      pulseDelay++;
    }
    else if (decreasing)
    {
      decreasing = false;
      pulseDelay--;
    }
    pulseDelay = constrain(pulseDelay, 1, 600);
    if (pressed)
    {
      pressed = false;
      edit = false;
    }
    if (pulseDelay != lastPulseDelay)
    {
      cursorLine2();
      Serial.print(F("                "));
      cursorLine2();
      Serial.print(F("Interval "));
      Serial.print(pulseDelay);
      Serial.print(F(" s"));
      lastPulseDelay = pulseDelay;
    }
  }

  //Pulse length adjustment
  clearLCD();
  cursorHome();
  Serial.print(F("Dye Pulse"));
  cursorLine2();
  Serial.print(F("Length "));
  Serial.print(pulseLength);
  Serial.print(F(" s"));
  edit = true;
  while (edit)
  {
    checkknob();
    checkbutton();
    manual();
    if (increasing)
    {
      increasing = false;
      pulseLength++;
    }
    else if (decreasing)
    {
      decreasing = false;
      pulseLength--;
    }
    pulseLength = constrain(pulseLength, 1, pulseDelay); //Length of dye pulse cannot be longer than the pulse delay/interval
    if (pressed)
    {
      pressed = false;
      edit = false;
      starter = true;
      automatic = true;
    }
    if (pulseLength != lastPulseLength)
    {
      cursorLine2();
      Serial.print(F("                "));
      cursorLine2();
      Serial.print(F("Length "));
      Serial.print(pulseLength);
      Serial.print(F(" s"));
      lastPulseLength = pulseLength;
    }
  }
  clearLCD();
  cursorHome();
  Serial.print(pulseDelay);
  Serial.print(F(" s Cycle"));
  injectStartTime = millis();       //Time at start of auto injection
}

//////////////////////////////////////////////////////////////////////////////
/// Timing Function  /////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void timer()
{
  totalSec = (millis() - injectStartTime) / 1000;   //Time is has been auto-injecting for
  hour = totalSec / 3600;                           //Seconds in an hour
  uint16_t remainder = totalSec % 3600;             //Remainder
  minute = remainder / 60;                          //Seconds in a minute
  remainder = remainder % 60;                       //Remainder
  second = remainder;                               //Seconds

  if (hour > 99)
  {
    injectStartTime = millis();
    timer();
  }
}

void printtimer()
{
  cursorLine2();                              //Set cursor to line 2
  if (hour < 10)
  {
    Serial.print(F("0"));
    Serial.print(hour);
  }
  else
  {
    Serial.print(hour);
  }
  Serial.print(F(":"));
  if (minute < 10)
  {
    Serial.print(F("0"));
    Serial.print(minute);
  }
  else
  {
    Serial.print(minute);
  }
  Serial.print(F(":"));
  if (second < 10)
  {
    Serial.print(F("0"));
    Serial.print(second);
  }
  else
  {
    Serial.print(second);
  }
}


void manual()     //Manual operation pauses auto injector
//Manual overrides autoinjector, autoinjector resusmes after both buttons are released
{
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
    //Detect which changed
    if ((blue) && (!lastblue)) //Blue pressed
    {
      //blue changed
      digitalWrite(bluesol, HIGH);
      digitalWrite(blueled, HIGH);
      lastblue = true;
      starter = false;
    }
    else if ((!blue) && (lastblue)) //Blue released
    {
      //blue changed
      digitalWrite(bluesol, LOW);
      digitalWrite(blueled, LOW);
      lastblue = false;
    }
    if ((green) && (!lastgreen)) //Green pressed
    {
      //green changed
      digitalWrite(greensol, HIGH);
      digitalWrite(greenled, HIGH);
      lastgreen = true;
      starter = false;
    }
    else if ((!green) && (lastgreen)) //Green released
    {
      //green changed
      digitalWrite(greensol, LOW);
      digitalWrite(greenled, LOW);
      lastgreen = false;
    }
  } 
  if ((!blue) && (!green))
  {
    starter = true;
  }
}

//Menu Structure
void menuselect()
{
  switch (menu)
  {
    case 0:
      clearLCD();
      cursorHome();
      Serial.print(F(">Green Dye"));
      cursorLine2();
      Serial.print(F(" Blue Dye"));
      while (menu == 0)
      {
        checkbutton();
        checkknob();
        manual();
        if (increasing)
        {
          increasing = false;
          menu++;
        }
        else if (decreasing)
        {
          decreasing = false;
        }
        if (pressed)
        {
          pressed = false;
          adjustment();
          injectorVar = 1;      //Green mode
          cursorLine2();
          cursorRight(11);
          Serial.print(F("Green"));
          while (automatic)
          {
            checkbutton();
            if (pressed)
            {
              pressed = false;
              starter = true;
              togglestart();
              automatic = false;
              clearLCD();
              cursorHome();
              Serial.print(F(">Green Dye"));
              cursorLine2();
              Serial.print(F(" Blue Dye"));
            }
            auto_injector();
            manual();
            if ((millis() - elapsedTimerMS) > 1000)
            {
              cursorLine2();
              Serial.print(F("        "));  //Erase old time
              cursorLine2();
              timer();                      //Print new time
              printtimer();
              elapsedTimerMS = millis();
            }
          }
        }
      }
      break;

    case 1:
      clearLCD();
      cursorHome();
      Serial.print(F(" Green Dye"));
      cursorLine2();
      Serial.print(F(">Blue Dye"));
      while (menu == 1)
      {
        checkbutton();
        checkknob();
        manual();
        if (increasing)
        {
          increasing = false;
          menu++;
        }
        else if (decreasing)
        {
          decreasing = false;
          menu --;
        }
        if (pressed)
        {
          pressed = false;
          adjustment();
          injectorVar = 0;      //Blue mode
          cursorLine2();
          cursorRight(12);
          Serial.print(F("Blue"));
          while (automatic)
          {
            checkbutton();
            if (pressed)
            {
              pressed = false;
              starter = true;
              togglestart();
              automatic = false;
              clearLCD();
              cursorHome();
              Serial.print(F(" Green Dye"));
              cursorLine2();
              Serial.print(F(">Blue Dye"));
            }
            auto_injector();
            manual();
            if ((millis() - elapsedTimerMS) > 1000)
            {
              cursorLine2();
              Serial.print(F("        "));  //Erase old time
              cursorLine2();
              timer();                      //Print new time
              printtimer();
              elapsedTimerMS = millis();
            }
          }
        }
      }
      break;

    case 2:
      clearLCD();
      cursorHome();
      Serial.print(F(" Blue Dye"));
      cursorLine2();
      Serial.print(F(">Alternating Dye"));
      while (menu == 2)
      {
        checkbutton();
        checkknob();
        manual();
        if (increasing)
        {
          increasing = false;
        }
        else if (decreasing)
        {
          decreasing = false;
          menu --;
        }
        if (pressed)
        {
          pressed = false;
          adjustment();
          injectorVar = 2;      //Alternate mode
          cursorLine2();
          cursorRight(12);
          Serial.print(F("Alt."));
          while (automatic)
          {
            checkbutton();
            if (pressed)
            {
              pressed = false;
              starter = true;
              togglestart();
              automatic = false;
              clearLCD();
              cursorHome();
              Serial.print(F(" Blue Dye"));
              cursorLine2();
              Serial.print(F(">Alternating Dye"));
            }
            auto_injector();
            manual();
            if ((millis() - elapsedTimerMS) > 1000)
            {
              cursorLine2();
              Serial.print(F("        "));  //Erase old time
              cursorLine2();
              timer();                      //Print new time
              printtimer();
              elapsedTimerMS = millis();
            }
          }
        }
      }
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
  setContrast(40);
  backlightBrightness(6);
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
  pulseDelay = delayDefault;
  pulseLength = lengthDefault;
}

//Main Loop
void loop()
{
  menuselect(); //Run menu selection
  manual();     //Check for manual injection
}
