/*
 * Latest source:
 * https://github.com/moononournation/ATtinyWatch
 */
#include <avr/sleep.h>
#include <TinyWireM.h>
#include <EEPROM.h>
#include "ssd1306.h"
#include "WDT_Time.h"

#define TIMEOUT 3000 // 3 seconds
#define UNUSEDPINA 1
#define UNUSEDPINB 4
#define BUTTONPIN  3

#define SET_UP_BUTTON_THRESHOLD 100
#define UP_DOWN_BUTTON_THRESHOLD 600
#define PRESSED_BUTTON_THRESHOLD 1000

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
static display_mode_t display_mode = time_mode;
static display_mode_t last_display_mode = time_mode;
static bool time_changed = false;
static uint8_t selected_field = NO_FIELD;

void setup() {
  // setup input pins, also pullup unused pin for power saving purpose
  pinMode(UNUSEDPINA, INPUT_PULLUP);
  pinMode(UNUSEDPINB, INPUT_PULLUP);
  pinMode(BUTTONPIN, INPUT_PULLUP);

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
    // 1st row: print info
    oled.set_pos(0, 0);
    oled.print(getTemp() / 1000);
    oled.draw_pattern(1, 0b00000010);
    oled.draw_pattern(1, 0b00000101);
    oled.draw_pattern(1, 0b00000010);
    oled.write('C');

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

    // 2nd row: print date
    print_digit(7, 1, year(), (selected_field == YEAR_FIELD));
    oled.write('-');
    print_digit(7 + (5 * FONT_WIDTH), 1, month(), (selected_field == MONTH_FIELD));
    oled.write('-');
    print_digit(7 + (8 * FONT_WIDTH), 1, day(), (selected_field == DAY_FIELD));

    // 3rd-4th rows: print time
    oled.set_font_size(2);
    print_digit(0, 2, hour(), (selected_field == HOUR_FIELD));
    oled.draw_pattern(2 * FONT_2X_WIDTH + 1, 2, 2, 2, 0b00011000);
    print_digit(2 * FONT_2X_WIDTH + 5, 2, minute(), (selected_field == MINUTE_FIELD));
    oled.draw_pattern(4 * FONT_2X_WIDTH + 6, 2, 2, 2, 0b00011000);
    print_digit(4 * FONT_2X_WIDTH + 2 * FONT_WIDTH, 2, second(), (selected_field == SECOND_FIELD));
  } else if (display_mode == debug_mode) { // debug_mode
    print_debug_value(0, 'I', get_wdt_interrupt_count());
    print_debug_value(1, 'M', get_wdt_microsecond_per_interrupt());
    print_debug_value(2, 'V', getVcc());
    print_debug_value(3, 'T', getRawTemp());
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
  set_display_timeout(); // extent display timeout while user input
}

void check_button() {
  int buttonValue = analogRead(BUTTONPIN);

  if (buttonValue < PRESSED_BUTTON_THRESHOLD) { // button down
    set_display_timeout(); // extent display timeout while user input

    if (run_status == sleeping) {
      // wake_up if button pressed while sleeping
      wake_up();
    } else { // not sleeping
      if (buttonValue > UP_DOWN_BUTTON_THRESHOLD) { // down button
        handle_adjust_button_pressed(-1);
      } else if (buttonValue > SET_UP_BUTTON_THRESHOLD) { // up button
        handle_adjust_button_pressed(1);
      } else { // set button
        handle_set_button_pressed();
      }
    } // not sleeping
  } // button down
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

void handle_adjust_button_pressed(long value) {
  if (selected_field == NO_FIELD) {
    // toggle display_mode if no field selected
    display_mode = (display_mode == time_mode) ? debug_mode : time_mode;
  } else {
    long adjust_value;
    if (selected_field == YEAR_FIELD) {
      // TODO: handle leap year and reverse value
      adjust_value = value * SECS_PER_DAY * (leapYear(CalendarYrToTm(year())) ? 366 : 365);
    } else if (selected_field == MONTH_FIELD) {
      // TODO: handle leap year and reverse value
      adjust_value = value * SECS_PER_DAY * getMonthDays(CalendarYrToTm(year()), month());
    } else if (selected_field == DAY_FIELD) {
      // TODO: handle leap year and reverse value
      adjust_value = value * SECS_PER_DAY;
    } else if (selected_field == HOUR_FIELD) {
      adjust_value = value * SECS_PER_HOUR;
    } else if (selected_field == MINUTE_FIELD) {
      adjust_value = value * SECS_PER_MIN;
    } else if (selected_field == SECOND_FIELD) {
      adjust_value = value;
    }

    adjustTime(adjust_value);
    time_changed = true;
  }
}

