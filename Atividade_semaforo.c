#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdio.h>
#include "pico/time.h"
#include "hardware/timer.h"
#include "lib/ssd1306.h"
#include "lib/matrizRGB.h"
#include "buzzer.h"
#include "extras/Desenho.h"
#include "math.h"
#include "lib/leds.h"

#define ADC_JOYSTICK_X 26
#define ADC_JOYSTICK_Y 27
#define LED_GREEN_PIN 11
#define LED_BLUE_PIN 12
#define LED_RED_PIN 13
#define Button_B 6
#define MAX_JOYSTICK_VALUE 4074 // Maior Valor encontrando por meio de debbugação
#define MIN_JOYSTICK_VALUE 11   // Menor Valor encontrado por meio de debbugação
#define I2C_PORT i2c1     // Porta I2C utilizada
#define I2C_SDA 14        // Pino de dados SDA
#define I2C_SCL 15        // Pino de clock SCL
#define DISPLAY_ADDR 0x3C // Endereço I2C do display OLED
#define BUZZER_PIN 21 // Pino do buzzer

