#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include "ssd1306.h" // Inclua o cabeçalho da sua biblioteca SSD1306

// Definições de pinos e endereço I2C
#define I2C_PORT_INIT i2c1     // Porta I2C utilizada (renomeada para evitar conflito)
#define I2C_SDA_PIN 14         // Pino de dados SDA
#define I2C_SCL_PIN 15         // Pino de clock SCL
#define DISPLAY_ADDR_INIT 0x3C // Endereço I2C do display OLED

// Definições do tamanho do display
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

void display_init_all(ssd1306_t *display_ssd);

#endif // DISPLAY_UTILS_H