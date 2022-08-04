#include "screen.h"
#include <bcm2835.h>
#include <stdlib.h>
#include <stdio.h>

//Interface-Level primitives
//==========================
int init_if(void){
  if(!bcm2835_init()) {
    printf("Cannot init BCM2835. Is the kernel module installed?\n");
    exit(-1);
  }
  bcm2835_gpio_fsel(RST_PIN, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(DC_PIN, BCM2835_GPIO_FSEL_OUTP);
  bcm2835_gpio_fsel(BUSY_PIN, BCM2835_GPIO_FSEL_INPT);

  if(!bcm2835_spi_begin()){                                         //Start spi interface, set spi pin for the reuse function
    printf("Cannot init SPI interface. Is it activated in raspi-config?\n");
    exit(-1);
  }
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);     //High first transmission
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);                  //spi mode 0
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_128);  //Frequency
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);                     //set CE0
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);     //enable cs0
  return 0;
}

void digital_write(int pin, int value) {
  bcm2835_gpio_write(pin, value);
}

int digital_read(int pin) {
  return bcm2835_gpio_lev(pin);
}

void delay_ms(unsigned int delaytime) {
  bcm2835_delay(delaytime);
}

void spi_transfer(unsigned char data) {
  bcm2835_spi_transfer(data);
}

// Framebuffer painting functions.
//
// Note: these function have been greatly inspired by waveshare's
// epdpaint.cpp.
//=====================

void pclear (int colored, unsigned char* frame_buffer) {
  for (int x = 0; x < EPD_WIDTH; x++) {
    for (int y = 0; y < EPD_HEIGHT; y++) {
        pdraw_absolute_pixel(x, y, colored, frame_buffer);
    }
  }
}

void pdraw_absolute_pixel(int x, int y, int colored, unsigned char* frame_buffer) {
  if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
    return;
  }
  if (IF_INVERT_COLOR) {
    if (colored) {
      frame_buffer[(x + y * EPD_WIDTH) / 8] |= 0x80 >> (x % 8);
    } else {
      frame_buffer[(x + y * EPD_WIDTH) / 8] &= ~(0x80 >> (x % 8));
    }
  } else {
    if (colored) {
      frame_buffer[(x + y * EPD_WIDTH) / 8] &= ~(0x80 >> (x % 8));
    } else {
      frame_buffer[(x + y * EPD_WIDTH) / 8] |= 0x80 >> (x % 8);
    }
  }
}

void pdraw_pixel(int x, int y, int colored, unsigned char* frame_buffer) {
  int point_temp;
  if (sorientation == ROTATE_0) {
    if(x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
      return;
    }
    pdraw_absolute_pixel(x, y, colored, frame_buffer);
  } else if (sorientation == ROTATE_90) {
    if(x < 0 || x >= EPD_HEIGHT || y < 0 || y >= EPD_WIDTH) {
      return;
    }
    point_temp = x;
    x = EPD_WIDTH - y;
    y = point_temp;
    pdraw_absolute_pixel(x, y, colored, frame_buffer);
  } else if (sorientation == ROTATE_180) {
    if(x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
      return;
    }
    x = EPD_WIDTH - x;
    y = EPD_HEIGHT - y;
    pdraw_absolute_pixel(x, y, colored, frame_buffer);
  } else if (sorientation == ROTATE_270) {
    if(x < 0 || x >= EPD_HEIGHT || y < 0 || y >= EPD_WIDTH) {
      return;
    }
    point_temp = x;
    x = y;
    y = EPD_HEIGHT - point_temp;
    pdraw_absolute_pixel(x, y, colored, frame_buffer);
  }
}

void pdraw_char_at(int x, int y, char ascii_char, sFONT* font, int colored, unsigned char* frame_buffer){
  int i, j;
  unsigned int char_offset = (ascii_char - ' ') * font->Height * (font->Width / 8 + (font->Width % 8 ? 1 : 0));
  const unsigned char* ptr = &font->table[char_offset];

  for (j = 0; j < font->Height; j++){
    for (i = 0; i < font->Width; i++){
      if (*ptr & (0x80 >> (i % 8))){
        pdraw_pixel(x + i, y + j, colored, frame_buffer);
      }
      if (i % 8 == 7){
        ptr++;
      }
    }
    if (font->Width % 8 != 0){
      ptr++;
    }
  }
}

void pdraw_string_at(int x, int y, const char* text, sFONT* font, int colored, unsigned char* frame_buffer){
  const char* p_text = text;
  unsigned int counter = 0;
  int refcolumn = x;
  
  /* Send the string character by character on EPD */
  while (*p_text != 0) {
    /* Display one character on EPD */
    pdraw_char_at(refcolumn, y, *p_text, font, colored, frame_buffer);
    /* Decrement the column position by 16 */
    refcolumn += font->Width;
    /* Point on the next character */
    p_text++;
    counter++;
  }
}

void pdraw_line(int x0, int y0, int x1, int y1, int colored, unsigned char* frame_buffer){
  /* Bresenham algorithm */
  int dx = x1 - x0 >= 0 ? x1 - x0 : x0 - x1;
  int sx = x0 < x1 ? 1 : -1;
  int dy = y1 - y0 <= 0 ? y1 - y0 : y0 - y1;
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;

  while((x0 != x1) && (y0 != y1)) {
    pdraw_pixel(x0, y0 , colored, frame_buffer);
    if (2 * err >= dy) {     
      err += dy;
      x0 += sx;
    }
    if (2 * err <= dx) {
      err += dx; 
      y0 += sy;
    }
  }
}

void pdraw_vertical_line(int x, int y, int line_height, int colored, unsigned char* frame_buffer){
  int i;
  for (i = y; i < y + line_height; i++) {
    pdraw_pixel(x, i, colored, frame_buffer);
  }
}

void pdraw_horizontal_line(int x, int y, int line_width, int colored, unsigned char* frame_buffer){
  int i;
  for (i = x; i < x + line_width; i++) {
    pdraw_pixel(x, i, colored, frame_buffer);
  }

}

void pdraw_filled_rectangle(int x0, int y0, int x1, int y1, int colored, unsigned char* frame_buffer){
  int min_x, min_y, max_x, max_y;
  int i;
  min_x = x1 > x0 ? x0 : x1;
  max_x = x1 > x0 ? x1 : x0;
  min_y = y1 > y0 ? y0 : y1;
  max_y = y1 > y0 ? y1 : y0;
  
  for (i = min_x; i <= max_x; i++) {
    pdraw_vertical_line(i, min_y, max_y - min_y + 1, colored, frame_buffer);
  }
}

void pdraw_rectangle(int x0, int y0, int x1, int y1, int colored, unsigned char* frame_buffer) {
  int min_x, min_y, max_x, max_y;
  min_x = x1 > x0 ? x0 : x1;
  max_x = x1 > x0 ? x1 : x0;
  min_y = y1 > y0 ? y0 : y1;
  max_y = y1 > y0 ? y1 : y0;
  
  pdraw_horizontal_line(min_x, min_y, max_x - min_x + 1, colored, frame_buffer);
  pdraw_horizontal_line(min_x, max_y, max_x - min_x + 1, colored, frame_buffer);
  pdraw_vertical_line(min_x, min_y, max_y - min_y + 1, colored, frame_buffer);
  pdraw_vertical_line(max_x, min_y, max_y - min_y + 1, colored, frame_buffer);
}

void pdraw_term(Line* lines, unsigned char* frame_buffer) {
  int i, j;
  char* str = (char*)malloc((cols + 10) * sizeof(char));
  for(i=0; i < rows; i++) {
    for(j=0; j < cols; j++) {
      str[j] = lines[i][j].u;
    }
    pdraw_string_at(0, i * Font16.Height, str, &Font16, COLORED, frame_buffer);
  }
  free(str);
}
