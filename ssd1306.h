/*
 * Ref.:
 * DigisparkOLED: https://github.com/digistump/DigistumpArduino/tree/master/digistump-avr/libraries/DigisparkOLED
 * SSD1306 data sheet: https://www.adafruit.com/datasheets/SSD1306.pdf
 */
#include <TinyWireM.h>
#include "font.h"
#include "font_2x.h"
//#include "font_3x.h"

// custom I2C address by define SSD1306_I2C_ADDR
#ifndef SSD1306_I2C_ADDR
  #define SSD1306_I2C_ADDR 0x3C
#endif

// custom screen resolution by define SCREEN128X64, SCREEN128X32, SCREEN64X48 or SCREED64X32 (default)
//#define SCREEN_128X64
//#define SCREEN_128X32
//#define SCREEN_64X48 // not tested
#define SCREEN_64X32

#ifdef SCREEN_128X64
  #define WIDTH 0x0100
  #define PAGES 0x08
#else
#ifdef SCREEN_128X32
  #define WIDTH 0x0100
  #define PAGES 0x04
#else
#ifdef SCREEN_64X48
  #define WIDTH 0x40
  #define XOFFSET 0x20
  #define PAGES 0x06
#else //SCREED_64X32
  #define WIDTH 0x40
  #define XOFFSET 0x20
  #define PAGES 0x04
#endif
#endif
#endif

class SSD1306 : public Print {

  public:
    virtual size_t write(uint8_t);

    SSD1306(void);
    void begin(void);
    void ssd1306_send_command_start(void);
    void ssd1306_send_command_stop(void);
    void ssd1306_send_command(uint8_t command);
    void ssd1306_send_data_start(void);
    void ssd1306_send_data_stop(void);
    void ssd1306_send_data_byte(uint8_t byte);
    void set_area(uint8_t col, uint8_t page, uint8_t col_range_minus_1, uint8_t page_range_minus_1);
    void v_line(uint8_t col, uint8_t fill);
    void fill(uint8_t fill);
    void set_pos(uint8_t set_col, uint8_t set_page);
    void set_invert_color(bool set_invert);
    void set_font_size(uint8_t set_font_size);

    void draw_pattern(uint8_t width, uint8_t pattern);
    void draw_pattern(uint8_t set_col, uint8_t set_page, uint8_t width, uint8_t height, uint8_t pattern);
    void print_string(uint8_t set_col, uint8_t set_page, const char str[]);

    void off();
    void on();
};

