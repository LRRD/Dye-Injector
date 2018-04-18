// Mason Parrone' 2/4/2018 Fixed button shutdown bug by switching to input pullup
// Parker Savka 05/17/2017 Fixed encoder code for new encoder
// Mason Parrone' 05/01/2017 Changed button inputs to work with solenoids
// Parker Savka 11/29/2016 Added to Github, changed code to work with Grayhill encoder
// James Nation 06/25/2013 I think all of the bugs are now out!
// James Nation 05/30/2013 EmDyeInjector 1.0
#include <LiquidCrystal.h>
LiquidCrystal lcd (9, 8, 4, 5, 6, 7);  // LCD 4-bit interface.

uint32_t dyecounter = 1;
uint32_t SECONDS;
uint32_t INJ_SECONDS;
uint32_t elapsed_ms;
uint32_t injection_ms = 10;
uint8_t switch_event;
uint32_t cumulative_start_ms = 0;
uint32_t cumulative_injection_start_ms = 0;
static uint32_t last_display_update_ms = 0;
uint32_t inject_time_counter;

void reset_counters()
{
  cumulative_start_ms = millis();
  cumulative_injection_start_ms = millis();
  last_display_update_ms = 0;
  dyecounter = 1;
  injection_ms = 0;
}

void reset_injector_counter()
{
  cumulative_injection_start_ms = millis();
}

static void print_digits(uint32_t value_, uint8_t digits_, char lead_)
{
  // Print leading characters.
  uint32_t power = 10;
  for (uint8_t digit = 1; digit < digits_; digit++) {
    if (value_ < power) {
      lcd.print(lead_);
    }
    power *= 10;
  }
  
  // Print the value.
  lcd.print(value_, DEC);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Beeper handling./////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// If non-zero, the duration of the beep.
uint32_t beep_duration_ms = 0;

// The millisecond counter at the time the beeper was turned on.
uint32_t beep_start_ms = 0;

// Turn on the beeper.  Gough:  Note this is NOT a blocking function; it runs in background

void beep(uint16_t duration_ms_)
{
  // Save the requested duration.
  beep_duration_ms = duration_ms_;

  // Remember when we turned the beeper on.
  beep_start_ms = millis();
  if (beep_start_ms == 0) {
    beep_start_ms = 1;
  }
  
  // Turn the beeper on (SPWM_D10).  A 50/255 duty cycle seems to work well.
  analogWrite(10, 50);  
}

// Check to see if it is time to turn the beeper off.
void check_beeper()
{
  if (beep_duration_ms != 0 && (millis() - beep_start_ms) > beep_duration_ms) {
    
    // Time's up; turn beeper off.
    beep_duration_ms = 0;
    analogWrite(10, 0);
  }
}

//////////////////////////////////////////////////////////////////////////
// Encoder handling.//////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// The encoder count value.
static int16_t encoder_count = 0;

// The sequence of gray codes in the increasing and decreasing directions.
static uint8_t cw_gray_codes[4] = { 2, 0, 3, 1 };
static uint8_t ccw_gray_codes[4] = { 1, 3, 0, 2 };

// The intermediate delta in encoder count (-1, 0, or +1), incremented or decremented halfway between detents.
static int8_t half_ticks = 0;

// The gray code we last read from the encoder.
static uint8_t previous_gray_code = 0;

// Reset the encoder to zero.
static void reset_encoder()
{
  encoder_count = 0;
  half_ticks = 0;
}

// Look for encoder rotation, updating encoder_count as necessary.
static void check_encoder()
{
  // Get the Gray-code state of the encoder.
  // A line is "low" if <= 512; "high" > 512 or above.
  uint8_t gray_code = ((analogRead(3) > 512) << 1) | (analogRead(4) > 512);

/*From Chris "It is a bitwise OR, so it is not limited to 0 and 1.  The <<1 is a multiply by 2, making the first operand either 0 or 2.  So it is basically looking at two signals.  Analog input 3 is the top bit and analog input 4 is the bottom bit.  The >512 is because the signals range from 0..1023, since I am using an analog input as a digital input."
*/
  
  // If the gray code has changed, adjust the intermediate delta. 
  if (gray_code != previous_gray_code) {
    
      // Each transition is half a detent.
      if (gray_code == cw_gray_codes[previous_gray_code]) {
         half_ticks++;
      } 
      else if (gray_code == ccw_gray_codes[previous_gray_code]) {
         half_ticks--;
      }
      
      // If we have accumulated a full tick, update the count.
      if (half_ticks >= 2) {
        half_ticks = 0;
        encoder_count++;
      } 
      else if (half_ticks <= -2) {
        half_ticks = 0;
        encoder_count--;
      }
      
      // Remember the most recently seen code.
      previous_gray_code = gray_code;
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Switch handling.///////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

static uint8_t switch_was_down = 0;

static uint8_t switch_released()
{
  // The switch is depressed if its sense line is low (<512).
  uint8_t switch_is_down = (analogRead(5) > 512);

  // The action takes place when the switch is released.
  uint8_t released = (switch_was_down && !switch_is_down);
  
  // Remember the state of the switch.
  switch_was_down = switch_is_down;
  
  // Was the switch just released?
  return released;
}

////////////////////////////////////////////////////////////////////////////////////
// Menu handling.///////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

struct menu_item_t
{
  const char* text;
  uint8_t code;
};

// Which menu are we working with?   //Creates the struct current_menu
static const struct menu_item_t* current_menu = NULL;

// Zero-based index of the first-displayed item in the menu.
static uint8_t first_displayed_menu_item = 0;

// Zero-based index of the currently selected item in the menu,
static uint8_t selected_menu_item = 0;

// The number of items in the menu.
static uint8_t current_menu_count = 0;

static void draw_menu_item(int line)
{
  lcd.setCursor(0, line);
  lcd.print((first_displayed_menu_item + line) == selected_menu_item ? ">" : " ");
  lcd.print(current_menu[first_displayed_menu_item + line].text);
}

static void draw_menu()
{
  lcd.clear();
  for (int line = 0; line < 2; line++) {
    draw_menu_item(line);
  }
}

static void init_menu(const struct menu_item_t* menu_)
{
  // Remember which menu we are working with.
  current_menu = menu_;
  
  // Start at the top of the menu.
  first_displayed_menu_item = selected_menu_item = 0;
  
  // Reset the encoder counter.
  reset_encoder();
  
  // Count the number of menu items.
  for (current_menu_count = 0; current_menu[current_menu_count].text != NULL; current_menu_count++);
  
  draw_menu();
}

static void run_menu()
{
  // Constrain the counter to the menu length.
  if (encoder_count < 0) {
    encoder_count = 0;
  }
  if (encoder_count >= current_menu_count) {
    encoder_count = current_menu_count - 1;
  }

  // Has the menu selection changed?
  if (encoder_count != selected_menu_item) {

    // If the new selected item is already on the display, just change the selection.
    if ((first_displayed_menu_item <= encoder_count) && (encoder_count <= (first_displayed_menu_item + 1))) {
       selected_menu_item = encoder_count;
    }
    
    // Otherwise, are we moving the selection toward the top of the menu?
    else if (encoder_count < selected_menu_item) {
      
      // Put the new selection at the top of the display.
      first_displayed_menu_item = encoder_count;
      selected_menu_item = encoder_count;
    }
    
    // Otherwise, we are moving the selection toward the bottom of the menu.
    else {
      
      // Put the new selection it at the bottom of the display.
      first_displayed_menu_item = encoder_count - 1;
      selected_menu_item = encoder_count;
    }
    
    // Update the menu.
    draw_menu();
  }
}

/////////////////////////////////////////////////////////////////////////////
// Main menu.////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////

// Main menu items.
enum{
  MAIN_GREEN,
  MAIN_BLUE,
  MAIN_ALTERNATE,
  MAIN_EXIT
};

static const struct menu_item_t main_menu[] =
{
  { "Green",  MAIN_GREEN },
  { "Blue",   MAIN_BLUE },
  { "Alternating", MAIN_ALTERNATE },
  { "Exit", MAIN_EXIT },	
  { NULL,  0 }
};

///////////////////////////////////////////////////////////////////////////////////////////////
///// Buttons /////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

void buttons()
{
  if(injection_ms != 0 && injection_ms != 1 && INJ_SECONDS <= inject_time_counter){  //Had to add to stop the motors from turning off during timed intervulls. 
      if (digitalRead(2) == LOW)
      {
        analogWrite(11,255);
      }
      else{
        analogWrite(11, 0);
      }

      if (digitalRead(12) == LOW)
      {
        analogWrite(3,255);
      }
      else{
        analogWrite(3, 0);
      } 
}
}

///////////////////////////////////////////////////////////////////////////////////////////////
////// Green //////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

  static void init_green(){
    reset_encoder(); 
  }

 static void green_menu(){
 SECONDS = encoder_count; //time inbetween injections.
 if (encoder_count <= 1){
   encoder_count = 1;
 }

 if (SECONDS >= 1800){
   SECONDS = 1800;
 }
 if (SECONDS <= 1){
   SECONDS = 1;
 }  
 elapsed_ms = millis() - last_display_update_ms;
 if ((last_display_update_ms == 0) || (elapsed_ms >= 250)) {
   lcd.clear();
   lcd.print ("Inject green dye");
   lcd.setCursor(0, 1);
   lcd.print("every ");
   print_digits( SECONDS, 2, '0'); 
   lcd.print(" s");
   last_display_update_ms = millis();
 } 
}

static void set_injection_time_green(){
  INJ_SECONDS = encoder_count; // how long its going to inject dye.
   if (encoder_count <= 1){
   encoder_count = 1;
 }
  
   if (INJ_SECONDS >= 1800){
   INJ_SECONDS = 1800;
 }
 if (INJ_SECONDS <= 1){
   INJ_SECONDS = 1;
 }  
  
  elapsed_ms = millis() - last_display_update_ms;
 if ((last_display_update_ms == 0) || (elapsed_ms >= 250)) {
   lcd.clear();
   lcd.print ("Dye pulse");
   lcd.setCursor(0, 1);
   lcd.print("length ");
   print_digits(INJ_SECONDS, 2, '0'); 
   lcd.print(" s");
   last_display_update_ms = millis();
 }
}

static void run_green(){

  uint32_t seconds, hours, minutes;
  
  seconds = (millis() - cumulative_start_ms) / 1000;
  hours = seconds / 3600;
  seconds -= 3600 * hours;
  minutes = seconds / 60;
  seconds -= 60 * minutes;
  
   elapsed_ms = millis() - last_display_update_ms;
  if ((last_display_update_ms == 0) || (elapsed_ms >= 500)) {
     lcd.clear();
     lcd.print(SECONDS);
     lcd.print(" s grn cycle");
     lcd.setCursor(0, 1);
     print_digits(hours, 2, '0');
     lcd.print(":");
     print_digits(minutes, 2, '0');
     lcd.print(":");
     print_digits(seconds, 2, '0');
     last_display_update_ms = millis();
  }
  
  injection_ms = (millis() - cumulative_injection_start_ms) /1000;
  inject_time_counter = (millis() - cumulative_injection_start_ms) /1000;
  if((injection_ms <= 0 )||(INJ_SECONDS >= inject_time_counter)){
    
    analogWrite(11, 255);
    
  }
  else{
    analogWrite(11, 0);
  }
    
  if (injection_ms == (SECONDS)){
    reset_injector_counter();
  }
  
  if (injection_ms == 0){
    beep(100);
  }
  
 
}

///////////////////////////////////////////////////////////////////////////////////////////////
////// Blue //////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

  static void init_blue(){
    reset_encoder(); 
  }

 static void blue_menu(){
  
 SECONDS = encoder_count; //time inbetween injections.
  if (encoder_count <= 1){
   encoder_count = 1;
 }

 if (SECONDS >= 1800){
   SECONDS = 1800;
 }
 if (SECONDS <= 1){
   SECONDS = 1;
 }  

 elapsed_ms = millis() - last_display_update_ms;
 if ((last_display_update_ms == 0) || (elapsed_ms >= 250)) {
   lcd.clear();
   lcd.print ("Inject blue dye");
   lcd.setCursor(0, 1);
   lcd.print("every ");
   print_digits( SECONDS, 2, '0'); 
   lcd.print(" s");
   last_display_update_ms = millis();
 } 
}

static void set_injection_time_blue(){
  INJ_SECONDS = encoder_count; // how long its going to inject dye.
  
   if (encoder_count <= 1){
   encoder_count = 1;
 }
 
 if (INJ_SECONDS >= 1800){
   INJ_SECONDS = 1800;
 }
 if (INJ_SECONDS <= 1){
   INJ_SECONDS = 1;
 } 
  elapsed_ms = millis() - last_display_update_ms;
 if ((last_display_update_ms == 0) || (elapsed_ms >= 250)) {
   lcd.clear();
   lcd.print ("Dye pulse");
   lcd.setCursor(0, 1);
   lcd.print("length ");
   print_digits(INJ_SECONDS, 2, '0'); 
   lcd.print(" s");
   last_display_update_ms = millis();
 }
}


 static void run_blue() {

  uint32_t seconds, hours, minutes;
  
  seconds = (millis() - cumulative_start_ms) / 1000;
  hours = seconds / 3600;
  seconds -= 3600 * hours;
  minutes = seconds / 60;
  seconds -= 60 * minutes;
  
   elapsed_ms = millis() - last_display_update_ms;
  if ((last_display_update_ms == 0) || (elapsed_ms >= 500)) {
     lcd.clear();
     lcd.print(SECONDS);
     lcd.print(" s blue cycle");
     lcd.setCursor(0, 1);
     print_digits(hours, 2, '0');
     lcd.print(":");
     print_digits(minutes, 2, '0');
     lcd.print(":");
     print_digits(seconds, 2, '0');
     last_display_update_ms = millis();
  }
  
  injection_ms = (millis() - cumulative_injection_start_ms) /1000;
  inject_time_counter = (millis() - cumulative_injection_start_ms) /1000;
  if((injection_ms <= 0 )||(INJ_SECONDS >= inject_time_counter)){
    
    analogWrite(3, 255);
    
  }
  else{
    analogWrite(3, 0);
  }
    
  if (injection_ms == (SECONDS)){
    reset_injector_counter();
  }
  
  if (injection_ms == 0){
    beep(100);
  }
  
}

///////////////////////////////////////////////////////////////////////////////////////////////
////// Alternating ////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

  static void init_alternate(){
    reset_encoder(); 
  }
  
  static void alternate_menu(){
    
  SECONDS = encoder_count; //time inbetween injections.
  
   if (encoder_count <= 1){
   encoder_count = 1;
 }
  
  if (SECONDS >= 1800){
    SECONDS = 1800;
  }
  if (SECONDS <= 1){
    SECONDS = 1;
  }  
  
  elapsed_ms = millis() - last_display_update_ms;
  if ((last_display_update_ms == 0) || (elapsed_ms >= 250)) {
    lcd.clear();
    lcd.print ("Alternate dye");
    lcd.setCursor(0, 1);
    lcd.print("every ");
    print_digits( SECONDS, 2, '0'); 
    lcd.print(" s");
    last_display_update_ms = millis();
  } 
  }
  
  
  static void set_injection_time_alternate(){
  INJ_SECONDS = encoder_count; // how long its going to inject dye.
   if (encoder_count <= 1){
   encoder_count = 1;
 }
  
  if (INJ_SECONDS >= 1800){
   INJ_SECONDS = 1800;
 }
 if (INJ_SECONDS <= 1){
   INJ_SECONDS = 1;
 } 
 
  elapsed_ms = millis() - last_display_update_ms;
 if ((last_display_update_ms == 0) || (elapsed_ms >= 250)) {
   lcd.clear();
   lcd.print ("Dye pulse");
   lcd.setCursor(0, 1);
   lcd.print("length ");
   print_digits(INJ_SECONDS, 2, '0'); 
   lcd.print(" s");
   last_display_update_ms = millis();
 }
}


 void run_alternate(){

  uint32_t seconds, hours, minutes;
  
  seconds = (millis() - cumulative_start_ms) / 1000;
  hours = seconds / 3600;
  seconds -= 3600 * hours;
  minutes = seconds / 60;
  seconds -= 60 * minutes;
  
   elapsed_ms = millis() - last_display_update_ms;
  if ((last_display_update_ms == 0) || (elapsed_ms >= 500)) {
     lcd.clear();
     lcd.print(SECONDS);
     lcd.print(" s alt cycle");
     lcd.setCursor(0, 1);
     print_digits(hours, 2, '0');
     lcd.print(":");
     print_digits(minutes, 2, '0');
     lcd.print(":");
     print_digits(seconds, 2, '0');
     last_display_update_ms = millis();
  }
  
  injection_ms = (millis() - cumulative_injection_start_ms) /1000;
  inject_time_counter = (millis() - cumulative_injection_start_ms) /1000;
  if((injection_ms <= 0 )||(INJ_SECONDS >= inject_time_counter)){
    if(dyecounter %2 == 1){
    analogWrite(3,255);
  }
    else {
      analogWrite(11, 255);
    }
  }
  else{
    analogWrite(3, 0);
    analogWrite(11, 0);
  }
  
  if (injection_ms == (SECONDS)){
    reset_injector_counter();
    dyecounter++;
  }
  
  if (injection_ms == 0){
    beep(100);
  }
  
 
}

///////////////////////////////////////////////////////////////////////////////////////////////
///////Main program ///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////

enum {
  MODE_MAIN_MENU,
  MODE_GREEN_MENU,
  MODE_RUN_GREEN,
  MODE_BLUE_MENU,
  MODE_RUN_BLUE,
  MODE_ALTERNATE_MENU,
  MODE_RUN_ALTERNATE,
  MODE_SET_INJECTION_TIME_GREEN,
  MODE_SET_INJECTION_TIME_BLUE,
  MODE_SET_INJECTION_TIME_ALTERNATE,
  HOME_SCREEN
};

// The mode we are currently in (one of the MODE_ constants).
static uint8_t mode;

void setup() 
{
  pinMode(2,INPUT_PULLUP);
  pinMode(12, INPUT_PULLUP);
  pinMode(5, OUTPUT);
  analogWrite(1, 168);
  digitalWrite(A5, HIGH);
  analogWrite(3, 0);
  analogWrite(11, 0);
  TCCR1B = TCCR1B & 0b11111000 | 0x02;  //for the beeper, taken from Alex controller

  lcd.begin(16, 2);
  lcd.clear();
    lcd.print("Emriver");
  lcd.setCursor(0, 1);
  lcd.print("Dye Injector");  // Change to whatever this is going to be called.
       tone(10,1000);	   //bunch of beep/boops put here by Steve // carried over from Alex Controller.			
	 delay(250);
	 noTone(10);
   tone(10,4000);
   delay(250);
   noTone(10);
	delay(1000);
   lcd.clear();
   lcd.print("software version");
   lcd.setCursor(0, 1);
   lcd.print("  ---- 1.0 ----");
   tone(10,1000);
	 delay(100);
	 noTone(10);
   tone(10,4000);
   delay(100);
   noTone(10);
    delay (700);
    
    mode = HOME_SCREEN;
}

void loop()
{

  // checks encoder, switch, and beeper.
  check_encoder();  
  switch_event = switch_released();
  check_beeper();
  
  // switches main program
  switch(mode) {
   
      case HOME_SCREEN:
      
      if(switch_event)
      {
           init_menu(main_menu);
           mode = MODE_MAIN_MENU; 
           analogWrite(3, 0);
           analogWrite(11, 0);
                    
      }
          
      else{
        elapsed_ms = millis() - last_display_update_ms;
        if ((last_display_update_ms == 0) || (elapsed_ms >= 500)) {
        lcd.clear();
        lcd.print("Emriver");
        lcd.setCursor(0, 1);
        lcd.print("Dye Injector");
        last_display_update_ms = millis();
        } 
        
        buttons();
      }
      break;
      
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
      case MODE_GREEN_MENU:
           
      if (switch_event)
      {
           reset_counters();
           reset_injector_counter();
           mode = MODE_SET_INJECTION_TIME_GREEN;  
           encoder_count = 5;          
      }
          
      else{
        green_menu();
        }    
        
      break;
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////    
      case MODE_BLUE_MENU:
      
      if (switch_event)
      {
           reset_counters();
           reset_injector_counter();
           mode = MODE_SET_INJECTION_TIME_BLUE; 
           encoder_count = 5;           
      }
          
      else{
        blue_menu();
        }    
        
      break;
      
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
          
      case MODE_ALTERNATE_MENU:
      
       if (switch_event)
      {
           reset_counters();
           reset_injector_counter();
           mode = MODE_SET_INJECTION_TIME_ALTERNATE;    
           encoder_count = 5;        
      }
          
      else{
        alternate_menu();
        }    
        
      break;
      
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      
     case MODE_RUN_GREEN:
     
     if(switch_event){
       analogWrite(3, 0);
       analogWrite(11, 0);
       encoder_count = 3;
       injection_ms = 1;
       mode = MODE_MAIN_MENU;
     }

     else {
      run_green(); 
      buttons();
     }
     break;
     
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      
     case MODE_RUN_BLUE:
     
     if(switch_event){
       analogWrite(3, 0);
       analogWrite(11, 0);
       encoder_count = 3;
       injection_ms = 1;
       mode = MODE_MAIN_MENU;
     }  
     
     else {
      injection_ms = 0;
      run_blue(); 
      buttons();
     } 
     break;
     
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     
     case MODE_RUN_ALTERNATE:
     
     if(switch_event){     
       analogWrite(3, 0);
       analogWrite(11, 0);
       encoder_count = 3;
       injection_ms = 1;
       mode = MODE_MAIN_MENU;
     }
     
     else {
      injection_ms = 0;
      run_alternate(); 
      buttons();
     }
     break;
     
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     
     case MODE_SET_INJECTION_TIME_GREEN:
     
       if (switch_event)
      {
           reset_counters();
           reset_injector_counter();
           mode = MODE_RUN_GREEN;            
      }
     
     else {
      set_injection_time_green();
     }
     break;
     
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     
     case MODE_SET_INJECTION_TIME_BLUE:
     
       if (switch_event)
      {
           reset_counters();
           reset_injector_counter();
           mode = MODE_RUN_BLUE;            
      }
     
     else {
     set_injection_time_blue();
     }
     break;
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     
     case MODE_SET_INJECTION_TIME_ALTERNATE:
     
       if (switch_event)
      {
           reset_counters();
           reset_injector_counter();
           mode = MODE_RUN_ALTERNATE;            
      }
     
     else {
      set_injection_time_alternate();
     }
     break;
     
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      
     case MODE_MAIN_MENU:
     if(switch_event){
  
        switch (selected_menu_item) 
        {
        
                  case MAIN_GREEN:
                    // Green dye
                    init_green();
                    mode = MODE_GREEN_MENU;
                    encoder_count = 10;
                    break;
                    
                  case MAIN_BLUE:
                    // Blue dye
                    init_blue();
                    mode = MODE_BLUE_MENU;
                    encoder_count = 10;
                    break;
                    
                   case MAIN_ALTERNATE:
                    // alternates blue and green dye
                    init_alternate();
                    mode = MODE_ALTERNATE_MENU;
                    encoder_count = 10;
                    break;
                    
                    case MAIN_EXIT:
                     // Go back to the previous mode.
                    mode = HOME_SCREEN;
                    reset_encoder();
                    injection_ms = 10;
                    inject_time_counter = INJ_SECONDS;
                    break;
        }
     }
     else{
       run_menu();
     }
     break;
          
}
}
