#include <avr/sleep.h>
#include <TinyWireM.h>
#include <EEPROM.h>
#include "ssd1306.h"
#include "WDT_Time.h"
#include "font.h"
#include "font_3x.h"

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
#define FIELD_COUNT 5

// variables
SSD1306 oled;
static uint32_t display_timeout = 0;
static run_status_t run_status = normal;
static uint32_t set_button_pressed_time = 0;
static uint32_t up_button_pressed_time = 0;
static uint32_t button_last_handled_time = 0;
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
  if (display_mode == time_mode) {
    // 1st rows: print date
    oled.set_font_size(1);
    print_2_digit(0, 0, year(), (selected_field == YEAR_FIELD));
    oled.write('-');
    print_2_digit(5 * FONT_WIDTH, 0, month(), (selected_field == MONTH_FIELD));
    oled.write('-');
    print_2_digit(8 * FONT_WIDTH, 0, day(), (selected_field == DAY_FIELD));

    // top right corner: battery status
    uint32_t vcc = readVcc();
    // show battery bar from 1.8 V to 3.0 V in 8 pixels, (3000 - 1800) / 8 = 150 
    uint8_t bat_level = (vcc >= 3000) ? 8 : ((vcc <= 1800) ? 1 : (((vcc - 1800) / 150) + 1));
    oled.draw_pattern(51, 0, 1, 0b00111111);
    oled.draw_pattern(52, 0, 1, 0b00100001);
    oled.draw_pattern(53, 0, bat_level, 0b00101101);
    oled.draw_pattern(53 + bat_level, 0, 8 + 1 - bat_level, 0b00100001);
    oled.draw_pattern(62, 0, 1, 0b00111111);
    oled.draw_pattern(63, 0, 1, 0b00001100);

    // 2nd-4th rows: print time
    oled.set_font_size(3);
    print_2_digit(0, 1, hour(), (selected_field == HOUR_FIELD));
    oled.draw_pattern(2 * FONT_3X_WIDTH + 1, 2, 2, (second() & 1) ? 0 : 0b11000011); // colon
    print_2_digit(2 * FONT_3X_WIDTH + 4, 1, minute(), (selected_field == MINUTE_FIELD));
  } else if (display_mode == debug_mode) { // debug_mode
    oled.set_font_size(1);
    print_debug_value(0, 'I', wdt_get_interrupt_count());
    print_debug_value(1, 'M', wdt_get_wdt_microsecond_per_interrupt());
    print_debug_value(2, 'V', readVcc());
    print_debug_value(3, 'T', readTemp());
  } // debug_mode
}

void print_2_digit(uint8_t col, uint8_t page, int value, bool invert_color) {
  oled.set_pos(col, page);
  if (invert_color) oled.set_invert_color(true);
  if (value < 10) oled.print(0);
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
  if (digitalRead(SETBUTTON) == LOW) { // SET button pressed
    set_button_pressed_time = millis();
  }
  if (digitalRead(UPBUTTON) == LOW) { // SET button pressed
    up_button_pressed_time = millis();
  }

  set_display_timeout(); // extent display timeout while user input
}

void check_button() {
  bool set_button_down = (digitalRead(SETBUTTON) == LOW) || (set_button_pressed_time > button_last_handled_time);
  bool up_button_down = (digitalRead(UPBUTTON) == LOW) || (up_button_pressed_time > button_last_handled_time);

  if (set_button_down || up_button_down) { // button down
    set_display_timeout(); // extent display timeout while user input

    if (run_status == sleeping) {
      // wake_up if button pressed while sleeping
      wake_up();
    } else { // not sleeping
      if (set_button_down) {
        handle_set_button_pressed();
      } else if (set_button_pressed_time > 0) {
        set_button_pressed_time = 0;
      }

      if (up_button_down) {
        handle_up_button_pressed();
      } else if (up_button_pressed_time > 0) {
        up_button_pressed_time = 0;
      }
    } // not sleeping
  } // button down

  button_last_handled_time = millis();
}

void handle_set_button_pressed() {
  selected_field++;
  if (selected_field > FIELD_COUNT) { // finish time adjustment
    selected_field = NO_FIELD;
    if (time_changed) {
      wdt_auto_tune();
      time_changed = false;
    }
  }
  if (selected_field != NO_FIELD) display_mode = time_mode; // always switch to time display mode if adjusting time
}

void handle_up_button_pressed() {
  if (selected_field == NO_FIELD) {
    display_mode = (display_mode == time_mode) ? debug_mode : time_mode; // toggle mode;
  } else {
    int set_year = year();
    int set_month = month();
    int set_day = day();
    int set_hour = hour();
    int set_minute = minute();

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
      set_hour++; // add day
      if (set_hour > 23) set_hour = 0; // loop back
    } else if (selected_field == MINUTE_FIELD) {
      set_minute++; // add day
      if (set_minute > 59) set_minute = 0; // loop back
    }

    setTime(set_hour, set_minute, second(), set_day, set_month, set_year);
    time_changed = true;
  }
}

