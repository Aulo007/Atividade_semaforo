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

const int volatile Max = 9;      // Número máximo de pessoas no laboratório
volatile int current_people = 0; // Contador de pessoas no laboratório

typedef enum
{
    incrementar = 0,
    decrementar = 1,
    desativar = 2
} botao_t; // Estados dos botões

volatile botao_t generic_button_state = desativar;

SemaphoreHandle_t xButtonA_B;
SemaphoreHandle_t xButtonC;
SemaphoreHandle_t xLedsMutex;
SemaphoreHandle_t xDisplayMutex;
SemaphoreHandle_t xBuzzerMutex;
SemaphoreHandle_t xMatrizMutex;

// Variáveis para controle de debounce
volatile uint32_t last_button_a_time = 0;
volatile uint32_t last_button_b_time = 0;
volatile uint32_t last_button_c_time = 0;

void vTaskEntrada(void *pvParameters);
void vTasksaida(void *pvParameters);
void vTaskResetar(void *pvParameters);
void display_exibir();
void matriz_exibir();

void gpio_irq_handler(uint gpio, uint32_t events)
{
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (gpio == BUTTON_A_PIN)
    {
        // Verifica debounce para botão A - Entrada
        if ((current_time - last_button_a_time > DEBOUNCE_TIME_MS))
        {
            last_button_a_time = current_time;
            generic_button_state = incrementar; // Define flag para incrementar
        }
    }
    else if (gpio == BUTTON_B_PIN)
    {
        // Verifica debounce para botão B - Saída
        if ((current_time - last_button_b_time > DEBOUNCE_TIME_MS) && (current_people > 0))
        {
            last_button_b_time = current_time;
            generic_button_state = decrementar; // Define flag para decrementar
        }
    }
    else if (gpio == BUTTON_C_PIN)
    {
        // Verifica debounce para botão C - Reset
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

    // Criação dos semáforos
    xButtonA_B = xSemaphoreCreateCounting(Max, Max); // Inicia com Max tokens (lab vazio)
    xButtonC = xSemaphoreCreateBinary();
    xLedsMutex = xSemaphoreCreateMutex();
    xDisplayMutex = xSemaphoreCreateMutex();
    xBuzzerMutex = xSemaphoreCreateMutex();
    xMatrizMutex = xSemaphoreCreateMutex();

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
        if ((generic_button_state == incrementar) && (current_people < Max))
        {
            // Tenta consumir um token (entrada só é permitida se há tokens disponíveis)
            if (xSemaphoreTake(xButtonA_B, 0) == pdTRUE)
            {
                generic_button_state = desativar;
                current_people++; // Incrementa o contador de pessoas

                if (xSemaphoreTake(xMatrizMutex, portMAX_DELAY) == pdTRUE)
                {
                    matriz_exibir();
                    xSemaphoreGive(xMatrizMutex);
                }

                printf("Entrou uma pessoa. Total: %d\n", current_people);

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
                    display_exibir();
                    xSemaphoreGive(xDisplayMutex);
                }

                if (xSemaphoreTake(xBuzzerMutex, portMAX_DELAY) == pdTRUE)
                {
                    if (current_people == Max)
                    {
                        ativar_buzzer_com_intensidade(BUZZER_PIN, 1);
                        vTaskDelay(pdMS_TO_TICKS(200));
                        desativar_buzzer(BUZZER_PIN);
                    }
                    xSemaphoreGive(xBuzzerMutex);
                }
            }
        }
        else if (generic_button_state == incrementar)
        {
            generic_button_state = desativar;
            if (xSemaphoreTake(xBuzzerMutex, portMAX_DELAY) == pdTRUE)
            {
                if (current_people == Max)
                {
                    ativar_buzzer_com_intensidade(BUZZER_PIN, 1);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    desativar_buzzer(BUZZER_PIN);
                }
                xSemaphoreGive(xBuzzerMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno delay para evitar busy waiting
    }
}

void vTasksaida(void *pvParameters)
{
    // Task que trata apenas saídas (botão B)
    while (1)
    {
        if (generic_button_state == decrementar)
        {
            generic_button_state = desativar;
            current_people--; // Decrementa o contador de pessoas
            printf("Saiu uma pessoa. Total: %d\n", current_people);

            // Libera um token (permite uma nova entrada)
            xSemaphoreGive(xButtonA_B);

            if (xSemaphoreTake(xLedsMutex, portMAX_DELAY) == pdTRUE)
            {
                if (current_people == 0)
                {
                    acender_led_rgb_cor(COLOR_BLUE);
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
                display_exibir();
                xSemaphoreGive(xDisplayMutex);
            }

            if (xSemaphoreTake(xMatrizMutex, portMAX_DELAY) == pdTRUE)
            {
                matriz_exibir();
                xSemaphoreGive(xMatrizMutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Pequeno delay para evitar busy waiting
    }
}

void vTaskResetar(void *pvParameters)
{
    // Task que trata o botão C (resetar)
    while (1)
    {
        if (xSemaphoreTake(xButtonC, portMAX_DELAY) == pdTRUE)
        {
            // Reset do sistema - lab volta a ficar vazio
            current_people = 0;

            // Recria o semáforo com todos os tokens disponíveis
            vSemaphoreDelete(xButtonA_B);
            xButtonA_B = xSemaphoreCreateCounting(Max, Max);

            if (xSemaphoreTake(xMatrizMutex, portMAX_DELAY) == pdTRUE)
            {
                matriz_exibir();
                xSemaphoreGive(xMatrizMutex);
            }

            if (xSemaphoreTake(xDisplayMutex, portMAX_DELAY) == pdTRUE)
            {
                display_exibir();
                xSemaphoreGive(xDisplayMutex);
            }

            if (xSemaphoreTake(xLedsMutex, portMAX_DELAY) == pdTRUE)
            {
                acender_led_rgb_cor(COLOR_BLUE); // Azul para lab vazio
                xSemaphoreGive(xLedsMutex);
            }

            if (xSemaphoreTake(xBuzzerMutex, portMAX_DELAY) == pdTRUE)
            {
                ativar_buzzer_com_intensidade(BUZZER_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                desativar_buzzer(BUZZER_PIN);
                vTaskDelay(pdMS_TO_TICKS(200));
                ativar_buzzer_com_intensidade(BUZZER_PIN, 1);
                vTaskDelay(pdMS_TO_TICKS(200));
                desativar_buzzer(BUZZER_PIN);
                xSemaphoreGive(xBuzzerMutex);
            }

            printf("Contador resetado. Total: %d\n", current_people);
        }
    }
}

void matriz_exibir()
{
    npSetMatrixWithIntensity(caixa_de_numeros[current_people], 1.0);
}

void display_exibir()
{
    ssd1306_fill(&display, false);

    // Linha 1: Título
    ssd1306_draw_string(&display, "=LAB CONTROL=", 0, 0);

    // Linha 2: Ocupação atual
    char buffer1[20];
    sprintf(buffer1, "Pessoas: %d/%d", current_people, Max);
    ssd1306_draw_string(&display, buffer1, 0, 16);

    // Linha 3: Status
    char *status;
    if (current_people == 0)
        status = "VAZIO";
    else if (current_people == Max)
        status = "LOTADO";
    else if (current_people >= Max - 1)
        status = "QUASE LOTADO";
    else
        status = "DISPONIVEL";

    ssd1306_draw_string(&display, status, 0, 32);

    // Linha 4: Barra visual simples
    char barra[17] = "================"; // 16 caracteres
    int ocupados = (current_people * 16) / Max;
    for (int i = ocupados; i < 16; i++)
    {
        barra[i] = '-';
    }
    barra[16] = '\0';
    ssd1306_draw_string(&display, barra, 0, 48);

    ssd1306_send_data(&display);
}