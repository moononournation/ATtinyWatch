#include <avr/sleep.h>
#include <TinyWireM.h>
#include <EEPROM.h>
#include "ssd1306.h"
#include "WDT_Time.h"

#define TIMEOUT 15000 // 15 seconds
#define UNUSEDPIN 1
#define SETBUTTON 3
#define UPBUTTON  4

// enum
typedef enum {
  normal, sleeping
}  run_status_t;

typedef enum {
  time_mode, debug_mode
}  display_mode_t;

// button field constant
#define NO_FIELD 0
#define YEAR_FIELD 1
#define MONTH_FIELD 2
#define DAY_FIELD 3
#define HOUR_FIELD 4
#define MINUTE_FIELD 5
#define SECOND_FIELD 6
#define FIELD_COUNT 6

// variables
SSD1306 oled;
static uint32_t display_timeout = 0;
static run_status_t run_status = normal;
//#define COMPENSATE_BUTTON_PRESSED_TIME_GAP
#ifdef COMPENSATE_BUTTON_PRESSED_TIME_GAP
  static uint32_t set_button_pressed_time = 0;
  static uint32_t up_button_pressed_time = 0;
  static uint32_t button_last_handled_time = 0;
#endif
static display_mode_t display_mode = time_mode;
static display_mode_t last_display_mode = time_mode;
static bool time_changed = false;
static uint8_t selected_field = NO_FIELD;

void setup() {
  // setup input pins, also pullup unused pin for power saving purpose
  pinMode(UNUSEDPIN, INPUT_PULLUP);
  pinMode(SETBUTTON, INPUT_PULLUP);
  pinMode(UPBUTTON, INPUT_PULLUP);

  // init time
  init_time();

  // init I2C and OLED
  TinyWireM.begin();
  oled.begin();
  oled.fill(0x00); // clear in black

  // init display timeout
  set_display_timeout();
}

void loop() {
  // detect and handle button input
  check_button();

  if (run_status == sleeping) {
    // return to sleep mode after WDT interrupt
    system_sleep();
  } else { // not sleeping
    if (millis() > display_timeout) { // check display timeout
      enter_sleep();
    } else { // normal flow
      readRawVcc();
      readRawTemp();
      draw_oled();
    } // normal flow
  } // not sleeping
}

void enter_sleep() {
  oled.fill(0x00); // clear screen to avoid show old time when wake up
  oled.off();
  delay(2); // wait oled stable

  run_status = sleeping;
}

void wake_up() {
  run_status = normal;

  delay(2); // wait oled stable
  oled.on();

  // update display timeout
  set_display_timeout();
}

void set_display_timeout() {
  display_timeout = millis() + TIMEOUT;
}

/*
 * UI related
 */

void draw_oled() {
  if (display_mode != last_display_mode) {
    oled.fill(0x00);
    last_display_mode = display_mode;
  }
  oled.set_font_size(1);
  if (display_mode == time_mode) {
    // 1st rows: print date
    print_digit(0, 0, year(), (selected_field == YEAR_FIELD));
    oled.write('-');
    print_digit(5 * FONT_WIDTH, 0, month(), (selected_field == MONTH_FIELD));
    oled.write('-');
    print_digit(8 * FONT_WIDTH, 0, day(), (selected_field == DAY_FIELD));

    // top right corner: battery status
    uint32_t vcc = getVcc();
    // show battery bar from 1.8 V to 3.0 V in 8 pixels, (3000 - 1800) / 8 = 150
    uint8_t bat_level = (vcc >= 3000) ? 8 : ((vcc <= 1800) ? 1 : ((vcc - 1800 + 150) / 150));
    oled.draw_pattern(51, 0, 1, 1, 0b00111111);
    oled.draw_pattern(1, 0b00100001);
    oled.draw_pattern(bat_level, 0b00101101);
    oled.draw_pattern(8 + 1 - bat_level, 0b00100001);
    oled.draw_pattern(1, 0b00111111);
    oled.draw_pattern(1, 0b00001100);

    // 2nd-3th rows: print time
    oled.set_font_size(2);
    print_digit(0, 1, hour(), (selected_field == HOUR_FIELD));
    oled.draw_pattern(2 * FONT_2X_WIDTH + 1, 1, 2, 2, 0b00011000);
    print_digit(2 * FONT_2X_WIDTH + 5, 1, minute(), (selected_field == MINUTE_FIELD));
    oled.draw_pattern(4 * FONT_2X_WIDTH + 6, 1, 2, 2, 0b00011000);
    print_digit(4 * FONT_2X_WIDTH + 2 * FONT_WIDTH, 1, second(), (selected_field == SECOND_FIELD));
  } else if (display_mode == debug_mode) { // debug_mode
    print_debug_value(0, 'I', get_wdt_interrupt_count());
    print_debug_value(1, 'M', get_wdt_microsecond_per_interrupt());
    print_debug_value(2, 'V', getVcc());
    print_debug_value(3, 'T', getTemp());
  } // debug_mode
}

void print_digit(uint8_t col, uint8_t page, int value, bool invert_color) {
  oled.set_pos(col, page);
  if (invert_color) oled.set_invert_color(true);
  if (value < 10) oled.write('0');
  oled.print(value);
  if (invert_color) oled.set_invert_color(false);
}

void print_debug_value(uint8_t page, char initial, uint32_t value) {
  oled.set_pos(0, page);
  oled.write(initial);
  oled.set_pos(14, page);
  oled.print(value);
}

// PIN CHANGE interrupt event function
ISR(PCINT0_vect) {
#ifdef COMPENSATE_BUTTON_PRESSED_TIME_GAP
  if (digitalRead(SETBUTTON) == LOW) { // SET button pressed
    set_button_pressed_time = millis();
  }
  if (digitalRead(UPBUTTON) == LOW) { // SET button pressed
    up_button_pressed_time = millis();
  }
#endif
  set_display_timeout(); // extent display timeout while user input
}

void check_button() {
#ifdef COMPENSATE_BUTTON_PRESSED_TIME_GAP
  bool set_button_down = (digitalRead(SETBUTTON) == LOW) || (set_button_pressed_time > button_last_handled_time);
  bool up_button_down = (digitalRead(UPBUTTON) == LOW) || (up_button_pressed_time > button_last_handled_time);
#else
  bool set_button_down = (digitalRead(SETBUTTON) == LOW);
  bool up_button_down = (digitalRead(UPBUTTON) == LOW);
#endif
  if (set_button_down || up_button_down) { // button down
    set_display_timeout(); // extent display timeout while user input

    if (run_status == sleeping) {
      // wake_up if button pressed while sleeping
      wake_up();
    } else { // not sleeping
      if (set_button_down) {
        handle_set_button_pressed();
#ifdef COMPENSATE_BUTTON_PRESSED_TIME_GAP
      } else if (set_button_pressed_time > 0) {
        set_button_pressed_time = 0;
#endif
      }

      if (up_button_down) {
        handle_up_button_pressed();
#ifdef COMPENSATE_BUTTON_PRESSED_TIME_GAP
      } else if (up_button_pressed_time > 0) {
        up_button_pressed_time = 0;
#endif
      }
    } // not sleeping
  } // button down

#ifdef COMPENSATE_BUTTON_PRESSED_TIME_GAP
  button_last_handled_time = millis();
#endif
}

void handle_set_button_pressed() {
  display_mode = time_mode; // always switch to time display mode while set button pressed

  selected_field++;
  if (selected_field > FIELD_COUNT) { // finish time adjustment
    selected_field = NO_FIELD;
    if (time_changed) {
      wdt_auto_tune();
      time_changed = false;
    } //time changed
  } // finish time adjustment
}

void handle_up_button_pressed() {
  if (selected_field == NO_FIELD) {
    // toggle display_mode if no field selected
    display_mode = (display_mode == time_mode) ? debug_mode : time_mode;
  } else {
    uint16_t set_year = year();
    uint8_t set_month = month();
    uint8_t set_day = day();
    uint8_t set_hour = hour();
    uint8_t set_minute = minute();
    uint8_t set_second = second();

    if (selected_field == YEAR_FIELD) {
      set_year++; // add year
      if (set_year > 2069) set_year = 1970; // loop back
    } else if (selected_field == MONTH_FIELD) {
      set_month++; // add month
      if (set_month > 12) set_month = 1; // loop back
    } else if (selected_field == DAY_FIELD) {
      set_day++; // add day
      if (set_day > getMonthDays(CalendarYrToTm(set_year), set_month)) set_day = 1; // loop back
    } else if (selected_field == HOUR_FIELD) {
      set_hour++; // add hour
      if (set_hour > 23) set_hour = 0; // loop back
    } else if (selected_field == MINUTE_FIELD) {
      set_minute++; // add minute
      if (set_minute > 59) set_minute = 0; // loop back
    } else if (selected_field == SECOND_FIELD) {
      set_second++; // add second
      if (set_second > 59) set_second = 0; // loop back
    }

    setTime(set_hour, set_minute, set_second, set_day, set_month, set_year);
    time_changed = true;
  }
}

