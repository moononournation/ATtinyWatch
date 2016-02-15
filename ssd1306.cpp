/*
 * Ref.:
 * DigisparkOLED: https://github.com/digistump/DigistumpArduino/tree/master/digistump-avr/libraries/DigisparkOLED
 * SSD1306 data sheet: https://www.adafruit.com/datasheets/SSD1306.pdf
 */
#include <avr/pgmspace.h>
#include <TinyWireM.h>
#include "ssd1306.h"

/*
 * Software Configuration, data sheet page 64
 */

static const uint8_t ssd1306_configuration[] PROGMEM = {

#ifdef SCREEN_128X64
  0xA8, 0x3F,   // Set MUX Ratio, 0F-3F
#else // SCREEN_128X32 / SCREED_64X32
  0xA8, 0x1F,   // Set MUX Ratio, 0F-3F
#endif

  0xD3, 0x00,   // Set Display Offset
  0x40,         // Set Display Start line
  0xA1,         // Set Segment re-map, mirror, A0/A1
  0xC8,         // Set COM Output Scan Direction, flip, C0/C8

#ifdef SCREEN_128X32
  0xDA, 0x02,   // Set COM Pins hardware configuration, Sequential
#else // SCREEN_128X64 / SCREEN_64X32
  0xDA, 0x12,   // Set Com Pins hardware configuration, Alternative
#endif

  0x81, 0x01,   // Set Contrast Control, 01-FF
  0xA4,         // Disable Entire Display On, 0xA4=Output follows RAM content; 0xA5,Output ignores RAM content
  0xA6,         // Set Display Mode. A6=Normal; A7=Inverse
  0xD5, 0x80,   // Set Osc Frequency
  0x8D, 0x14,   // Enable charge pump regulator
  0xAF          // Display ON in normal mode
};

SSD1306::SSD1306(void) {}

void SSD1306::begin(void)
{
  for (uint8_t i = 0; i < sizeof (ssd1306_configuration); i++) {
    ssd1306_send_command(pgm_read_byte_near(&ssd1306_configuration[i]));
  }
}

void SSD1306::ssd1306_send_command_start(void) {
  TinyWireM.beginTransmission(SSD1306_I2C_ADDR);
  TinyWireM.send(0x00); //command
}

void SSD1306::ssd1306_send_command_stop(void) {
  TinyWireM.endTransmission();
}

void SSD1306::ssd1306_send_command(uint8_t command)
{
  ssd1306_send_command_start();
  TinyWireM.send(command);
  ssd1306_send_command_stop();
}

void SSD1306::ssd1306_send_data_start(void)
{
  TinyWireM.beginTransmission(SSD1306_I2C_ADDR);
  TinyWireM.send(0x40); //data
}

void SSD1306::ssd1306_send_data_stop(void)
{
  TinyWireM.endTransmission();
}

void SSD1306::ssd1306_send_data_byte(uint8_t data)
{
  if (TinyWireM.write(data) == 0) {
    // push data if detect buffer used up
    ssd1306_send_data_stop();
    ssd1306_send_data_start();
    TinyWireM.write(data);
  }
}

void SSD1306::set_area(uint8_t col, uint8_t page, uint8_t col_range_minus_1, uint8_t page_range_minus_1)
{
  ssd1306_send_command_start();
  TinyWireM.send(0x20);
  TinyWireM.send(0x01);
  TinyWireM.send(0x21);
#ifdef XOFFSET // SCREEN_SCREEN_64X32
  TinyWireM.send(XOFFSET + col);
  TinyWireM.send(XOFFSET + col + col_range_minus_1);
#else // SCREEN_128_64 / SCREEN_128X32
  TinyWireM.send(col);
  TinyWireM.send(col + col_range_minus_1);
#endif
  TinyWireM.send(0x22);
  TinyWireM.send(page);
  TinyWireM.send(page + page_range_minus_1);
  ssd1306_send_command_stop();
}

void SSD1306::fill(uint8_t data)
{
  set_area(0, 0, WIDTH - 1, PAGES - 1);
  uint16_t data_size = (WIDTH) * (PAGES);

  ssd1306_send_data_start();
  for (uint16_t i = 0; i < data_size; i++)
  {
    ssd1306_send_data_byte(data);
  }
  ssd1306_send_data_stop();
}

void SSD1306::v_line(uint8_t col, uint8_t data)
{
  set_area(col, 0, 0, PAGES);
  ssd1306_send_data_start();
  for (uint8_t i = 0; i <= PAGES; i++)
  {
    ssd1306_send_data_byte(data);
  }
  ssd1306_send_data_stop();
}

static uint8_t col = 0;
static uint8_t page = 0;
static bool invert_color = false;
static uint8_t font_size = 1;
static uint8_t font_width = FONT_WIDTH;
static uint8_t font_volume = 1 * FONT_WIDTH;
static uint8_t ascii_code_start = FONT_RANGE_START;
static uint8_t ascii_code_end = FONT_RANGE_END;

void SSD1306::set_pos(uint8_t set_col, uint8_t set_page) {
  col = set_col;
  page = set_page;
}

void SSD1306::draw_pattern(uint8_t width, uint8_t pattern) {
  draw_pattern(col, page, width, 1, pattern);
}

void SSD1306::draw_pattern(uint8_t set_col, uint8_t set_page, uint8_t width, uint8_t height, uint8_t pattern) {
  set_area(set_col, set_page, width, height - 1);
  ssd1306_send_data_start();
  for (uint8_t i = 0; i < (width * height); i++) {
    ssd1306_send_data_byte(pattern);
  }
  ssd1306_send_data_stop();

  col = set_col + width;
  page = set_page;
}

void SSD1306::set_invert_color(bool set_invert) {
  invert_color = set_invert;
}

void SSD1306::set_font_size(uint8_t set_size) {
  font_size = set_size;
  if (set_size == 1) {
    font_width = FONT_WIDTH;
    font_volume = 1 * FONT_WIDTH;
    ascii_code_start = FONT_RANGE_START;
    ascii_code_end = FONT_RANGE_END;
#ifdef FONT_2X_WIDTH
  } else if (set_size == 2) {
    font_width = FONT_2X_WIDTH;
    font_volume = 2 * FONT_2X_WIDTH;
    ascii_code_start = FONT_2X_RANGE_START;
    ascii_code_end = FONT_2X_RANGE_END;
#endif
#ifdef FONT_3X_WIDTH
  } else if (set_size == 3) {
    font_width = FONT_3X_WIDTH;
    font_volume = 3 * FONT_3X_WIDTH;
    ascii_code_start = FONT_3X_RANGE_START;
    ascii_code_end = FONT_3X_RANGE_END;
#endif
  }
}

size_t SSD1306::write(uint8_t c) {
  if ((c < ascii_code_start) || (c > ascii_code_end)) return 0;

  set_area(col, page, font_width - 1, font_size - 1);

  uint16_t offset = (c - ascii_code_start) * font_volume;
  uint8_t data;

  ssd1306_send_data_start();
  for (uint8_t i = 0; i < font_volume; i++)
  {
    if (font_size == 1) {
      data = pgm_read_byte_near(&font_bitmap[offset++]);      
#ifdef FONT_2X_WIDTH
    } else if (font_size == 2) {
      data = pgm_read_byte_near(&font_2x_bitmap[offset++]);
#endif
#ifdef FONT_3X_WIDTH
    } else if (font_size == 3) {
      data = pgm_read_byte_near(&font_3x_bitmap[offset++]);
#endif
    }
    if (invert_color) data = ~ data; // invert
    ssd1306_send_data_byte(data);
  }
  ssd1306_send_data_stop();

  // move pos forward
  col += font_width;
  return font_width;
}

void SSD1306::print_string(uint8_t col, uint8_t page, const char str[]) {
    set_pos(col, page);
    print(str);
}

void SSD1306::off(void)
{
  ssd1306_send_command(0xAE);
}

void SSD1306::on(void)
{
  ssd1306_send_command(0xAF);
}

