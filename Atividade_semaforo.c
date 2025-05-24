#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "hardware/pwm.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
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
#include "lib/buttons.h"

#define MAX_JOYSTICK_VALUE 4074 // Maior Valor encontrando por meio de debbugação
#define MIN_JOYSTICK_VALUE 11   // Menor Valor encontrado por meio de debbugação
#define I2C_PORT i2c1           // Porta I2C utilizada
#define I2C_SDA 14              // Pino de dados SDA
#define I2C_SCL 15              // Pino de clock SCL
#define DISPLAY_ADDR 0x3C       // Endereço I2C do display OLED
#define BUZZER_PIN 21           // Pino do buzzer

const int volatile Max = 0; // Número de pessoas no estabelecimento

void gpio_irq_handler(uint gpio, uint32_t events)
{
    
}

int main(void)
{
    led_init();
    buttons_init();
    npInit(7);

    stdio_init_all();
    sleep_ms(1000); // tempo para terminal abrir via USB

    button_set_irq_callback(BUTTON_A_PIN, &gpio_irq_handler);
    button_set_irq_callback(BUTTON_B_PIN, &gpio_irq_handler);
    button_set_irq_callback(BUTTON_C_PIN, &gpio_irq_handler);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}