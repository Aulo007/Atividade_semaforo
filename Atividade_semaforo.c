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
#define DEBOUNCE_TIME_MS 200    // Tempo de debounce em milissegundos

const int volatile Max = 8;      // Número máximo de pessoas no laboratório
volatile int current_people = 0; // Contador de pessoas no laboratório

// Flag global para indicar qual botão foi pressionado por último
volatile bool last_button_was_A = true; // true = incrementar, false = decrementar

SemaphoreHandle_t xButtonA_B;
SemaphoreHandle_t xButtonC;

// Variáveis para controle de debounce
volatile uint32_t last_button_a_time = 0;
volatile uint32_t last_button_b_time = 0;
volatile uint32_t last_button_c_time = 0;

void vTaskEntrada(void *pvParameters);
void vTasksaida(void *pvParameters);
void vTaskResetar(void *pvParameters);

void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (gpio == BUTTON_A_PIN)
    {
        // Verifica debounce para botão A
        if (current_time - last_button_a_time > DEBOUNCE_TIME_MS)
        {
            // printf("Botão A pressionado!\n"); // para debbuging
            last_button_a_time = current_time;
            last_button_was_A = true; // Define flag para incrementar
            xSemaphoreGiveFromISR(xButtonA_B, &xHigherPriorityTaskWoken);
        }
    }
    else if (gpio == BUTTON_B_PIN)
    {
        // Verifica debounce para botão B
        if (current_time - last_button_b_time > DEBOUNCE_TIME_MS)
        {
            // printf("Botão B pressionado!\n"); // para debbuging
            last_button_b_time = current_time;
            last_button_was_A = false; // Define flag para decrementar
            xSemaphoreGiveFromISR(xButtonA_B, &xHigherPriorityTaskWoken);
        }
    }
    else if (gpio == BUTTON_C_PIN)
    {
        // Verifica debounce para botão C
        if (current_time - last_button_c_time > DEBOUNCE_TIME_MS)
        {
            last_button_c_time = current_time;
            xSemaphoreGiveFromISR(xButtonC, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int main(void)
{
    led_init();
    buttons_init();
    npInit(7);

    stdio_init_all();
    sleep_ms(3000); // tempo para terminal abrir via USB

    printf("Iniciando...\n");

    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_C_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    xButtonA_B = xSemaphoreCreateCounting(Max, 0);
    xButtonC = xSemaphoreCreateBinary();

    // Cria ambas as tasks como requerido
    xTaskCreate(vTaskEntrada, "TaskEntrada", 256, NULL, 1, NULL);
    xTaskCreate(vTasksaida, "TaskSaida", 256, NULL, 1, NULL);
    xTaskCreate(vTaskResetar, "TaskResetar", 256, NULL, 1, NULL);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}

void vTaskEntrada(void *pvParameters)
{
    // Task que trata apenas entradas (botão A)
    while (1)
    {
        if (xSemaphoreTake(xButtonA_B, portMAX_DELAY) == pdTRUE)
        {
            if (last_button_was_A) // Verifica se foi o botão A
            {
                if (current_people < Max)
                {
                    current_people++;
                    printf("Entrou uma pessoa. Total: %d\n", current_people);
                }
                else
                {
                    printf("Laboratório cheio!\n");
                }
            }
            // Se não foi botão A, libera o semáforo para a outra task
            else
            {
                xSemaphoreGive(xButtonA_B);
            }
        }
    }
}

void vTasksaida(void *pvParameters)
{
    // Task que trata apenas saídas (botão B)
    while (1)
    {
        if (xSemaphoreTake(xButtonA_B, portMAX_DELAY) == pdTRUE)
        {
            if (!last_button_was_A) // Verifica se foi o botão B
            {
                if (current_people > 0)
                {
                    current_people--;
                    printf("Saiu uma pessoa. Total: %d\n", current_people);
                }
                else
                {
                    printf("Laboratório vazio!\n");
                }
            }
            // Se não foi botão B, libera o semáforo para a outra task
            else
            {
                xSemaphoreGive(xButtonA_B);
            }
        }
    }
}

void vTaskResetar(void *pvParameters)
{
    // Task que trata o botão C (resetar)
    while (1)
    {
        if (xSemaphoreTake(xButtonC, portMAX_DELAY) == pdTRUE)
        {
            current_people = 0;
            printf("Contador resetado. Total: %d\n", current_people);
        }
    }
}