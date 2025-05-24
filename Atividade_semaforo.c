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
#include "lib/matrizRGB.h"
#include "buzzer.h"
#include "extras/Desenho.h"
#include "math.h"
#include "lib/leds.h"
#include "lib/buttons.h"
#include "lib/display_utils.h"

#define MAX_JOYSTICK_VALUE 4074 // Maior Valor encontrando por meio de debbugação
#define MIN_JOYSTICK_VALUE 11   // Menor Valor encontrado por meio de debbugação
#define BUZZER_PIN 21           // Pino do buzzer
#define DEBOUNCE_TIME_MS 200    // Tempo de debounce em milissegundos
static ssd1306_t display;

const int volatile Max = 8;      // Número máximo de pessoas no laboratório
volatile int current_people = 0; // Contador de pessoas no laboratório

// Flag global para indicar qual botão foi pressionado por último
volatile bool last_button_was_A = true; // true = incrementar, false = decrementar

SemaphoreHandle_t xButtonA_B;
SemaphoreHandle_t xButtonC;
SemaphoreHandle_t xLedsMutex;
SemaphoreHandle_t xDisplayMutex;
SemaphoreHandle_t xBuzzerMutex;

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
    display_init_all(&display);
    inicializar_buzzer(BUZZER_PIN);

    stdio_init_all();
    printf("Iniciando...\n");

    sleep_ms(3000); // tempo para terminal abrir via USB

    printf("Programa iniciado!\n");
    acender_led_rgb_cor(COLOR_BLUE);

    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(BUTTON_C_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    xButtonA_B = xSemaphoreCreateCounting(Max, 0);
    xButtonC = xSemaphoreCreateBinary();
    xLedsMutex = xSemaphoreCreateMutex();
    xDisplayMutex = xSemaphoreCreateMutex();
    xBuzzerMutex = xSemaphoreCreateMutex();

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

                if (xSemaphoreTake(xLedsMutex, portMAX_DELAY) == pdTRUE)
                {
                    if (current_people == Max)
                    {
                        acender_led_rgb_cor(COLOR_RED);
                    }
                    else if (current_people == (Max - 1))
                    {
                        acender_led_rgb_cor(COLOR_YELLOW);
                    }
                    else if (current_people > 0)
                    {
                        acender_led_rgb_cor(COLOR_GREEN);
                    }
                    xSemaphoreGive(xLedsMutex);
                }

                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
                {
                    ssd1306_fill(&display, false);
                    char buffer[20];                               // Aumente o tamanho do buffer para garantir espaço suficiente
                    sprintf(buffer, "Pessoas:%d", current_people); // Forma correta de usar sprintf
                    ssd1306_draw_string(&display, buffer, 0, 0);
                    ssd1306_send_data(&display);
                    xSemaphoreGive(xDisplayMutex); // Não se esqueça de liberar o mutex!
                }

                if (xSemaphoreTake(xBuzzerMutex, portMAX_DELAY) == pdTRUE)
                {
                    if (current_people == Max)
                    {
                        ativar_buzzer_com_intensidade(BUZZER_PIN, 1); // Buzzer ativo quando cheio
                        vTaskDelay(pdMS_TO_TICKS(200));
                        desativar_buzzer(BUZZER_PIN);
                    }

                    xSemaphoreGive(xBuzzerMutex);
                }
            }
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

                if (xSemaphoreTake(xLedsMutex, portMAX_DELAY) == pdTRUE)
                {
                    if (current_people == Max)
                    {
                        acender_led_rgb_cor(COLOR_RED);
                    }
                    else if (current_people == (Max - 1))
                    {
                        acender_led_rgb_cor(COLOR_YELLOW);
                    }
                    else if (current_people > 0)
                    {
                        acender_led_rgb_cor(COLOR_GREEN);
                    }

                    xSemaphoreGive(xLedsMutex);
                }

                if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
                {
                    ssd1306_fill(&display, false);
                    char buffer[20];                               // Aumente o tamanho do buffer para garantir espaço suficiente
                    sprintf(buffer, "Pessoas:%d", current_people); // Forma correta de usar sprintf
                    ssd1306_draw_string(&display, buffer, 0, 0);
                    ssd1306_send_data(&display);
                    xSemaphoreGive(xDisplayMutex); // Não se esqueça de liberar o mutex!
                }

                // Comentado, pois, como o botão B só decrementa, não faz sentido o buzzer para o caso de tentar entrar com a fila cheia
                // if (xSemaphoreTake(xBuzzerMutex, portMAX_DELAY) == pdTRUE)
                // {
                //     if (current_people == Max)
                //     {
                //         ativar_buzzer_com_intensidade(BUZZER_PIN, 1); // Buzzer ativo quando cheio
                //         vTaskDelay(pdMS_TO_TICKS(200));
                //         desativar_buzzer(BUZZER_PIN);
                //     }

                //     xSemaphoreGive(xBuzzerMutex);
                // }
            }
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

            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
            {
                ssd1306_fill(&display, false);
                char buffer[20];                               // Aumente o tamanho do buffer para garantir espaço suficiente
                sprintf(buffer, "Pessoas:%d", current_people); // Forma correta de usar sprintf
                ssd1306_draw_string(&display, buffer, 0, 0);
                ssd1306_send_data(&display);
                xSemaphoreGive(xDisplayMutex); // Não se esqueça de liberar o mutex!
            }

            if (xSemaphoreTake(xLedsMutex, portMAX_DELAY) == pdTRUE)
            {

                acender_led_rgb_cor(COLOR_GREEN);

                xSemaphoreGive(xLedsMutex);
            }

            if (xSemaphoreTake(xBuzzerMutex, portMAX_DELAY) == pdTRUE)
            {

                ativar_buzzer_com_intensidade(BUZZER_PIN, 1); // Buzzer ativo quando cheio
                vTaskDelay(pdMS_TO_TICKS(200));
                desativar_buzzer(BUZZER_PIN);
                vTaskDelay(pdMS_TO_TICKS(200));
                ativar_buzzer_com_intensidade(BUZZER_PIN, 1); // Buzzer ativo quando cheio
                vTaskDelay(pdMS_TO_TICKS(200));
                desativar_buzzer(BUZZER_PIN);

                xSemaphoreGive(xBuzzerMutex);
            }

            printf("Contador resetado. Total: %d\n", current_people);
        }
    }
}
