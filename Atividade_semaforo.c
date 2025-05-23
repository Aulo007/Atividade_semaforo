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
volatile int Center_Joystick_Y = 0;
volatile int Center_Joystick_X = 0;
volatile int cont = 0;

#define I2C_PORT i2c1     // Porta I2C utilizada
#define I2C_SDA 14        // Pino de dados SDA
#define I2C_SCL 15        // Pino de clock SCL
#define DISPLAY_ADDR 0x3C // Endereço I2C do display OLED

#define BUZZER_PIN 21 // Pino do buzzer

volatile int contador = 10;

// Função para controlar estado em que o sistema se encontra.
volatile uint8_t mode = 0;

volatile bool buzzer_ativo = false;

QueueHandle_t xQueueJoystickData;
QueueHandle_t xQueueSensorDataDisplay;
QueueHandle_t xQueueSensorDataMatriz;
QueueHandle_t xQueueSensorDataBuzzer;
QueueHandle_t xQueueSensorLeds;

typedef enum
{
    STATE_STABLE,         // Água e chuva estáveis
    STATE_WATER_OVERFLOW, // Água transbordando
    STATE_HEAVY_RAIN,     // Chuva forte
    STATE_BOTH_CRITICAL   // Água transbordando e chuva forte
} display_state_t;

void gpio_irq_handler(uint gpio, uint32_t events)
{
    reset_usb_boot(0, 0);
}

typedef struct
{
    uint16_t x_pos;
    uint16_t y_pos;
} joystick_data_t;

typedef struct
{
    float water_level;
    float rain_level;
} sensor_data_t;

void vJoystickTask(void *params);
void vConvertTask(void *params);
void vLedControlTask(void *params);
void vDisplayControlTask(void *params);
void vMatrizControlTask(void *params);
void vBuzzerControlTask(void *params);

int main()
{
    // Ativa BOOTSEL via botão
    gpio_init(Button_B);
    gpio_set_dir(Button_B, GPIO_IN);
    gpio_pull_up(Button_B);
    gpio_set_irq_enabled_with_callback(Button_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    stdio_init_all();
    sleep_ms(1000); // tempo para terminal abrir via USB

    // Cria a fila para compartilhamento de valor do joystick
    xQueueJoystickData = xQueueCreate(10, sizeof(joystick_data_t));
    xQueueSensorDataDisplay = xQueueCreate(10, sizeof(sensor_data_t));
    xQueueSensorDataMatriz = xQueueCreate(10, sizeof(sensor_data_t));
    xQueueSensorDataBuzzer = xQueueCreate(10, sizeof(sensor_data_t));
    xQueueSensorLeds = xQueueCreate(10, sizeof(sensor_data_t));

    // Criação das tasks
    xTaskCreate(vJoystickTask, "Joystick Task", 256, NULL, 1, NULL);
    xTaskCreate(vConvertTask, "Convert Task", 256, NULL, 1, NULL);
    xTaskCreate(vDisplayControlTask, "Display Control Task", 256, NULL, 1, NULL);
    xTaskCreate(vMatrizControlTask, "Matriz Control Task", 256, NULL, 1, NULL);
    xTaskCreate(vBuzzerControlTask, "Buzzer Control Task", 256, NULL, 1, NULL);
    xTaskCreate(vLedControlTask, "Led Control Task", 256, NULL, 1, NULL);

    // Inicia o agendador
    vTaskStartScheduler();
    panic_unsupported();
}

void vJoystickTask(void *params)
{
    adc_gpio_init(ADC_JOYSTICK_X);
    adc_gpio_init(ADC_JOYSTICK_Y);
    adc_init();

    joystick_data_t joystick_data;

    // Fase de inicialização - aguarda 1 segundo descartando leituras
    printf("Inicializando ADC - aguardando estabilização (1 segundo)...\n");

    absolute_time_t end_time = make_timeout_time_ms(2000); // 2 segundos

    while (!time_reached(end_time))
    {
        // Continua fazendo leituras e descartando durante o período de aquecimento
        adc_select_input(0);
        uint16_t dummy_y = adc_read();
        adc_select_input(1);
        uint16_t dummy_x = adc_read();
        printf("Descartando leitura: X=%u, Y=%u\n", dummy_x, dummy_y);
        vTaskDelay(pdMS_TO_TICKS(50)); // Pequenas pausas para permitir outras tarefas
        Center_Joystick_Y = Center_Joystick_Y + dummy_y;
        Center_Joystick_X = Center_Joystick_X + dummy_x;
        cont++;
    }

    Center_Joystick_Y = Center_Joystick_Y / cont;
    Center_Joystick_X = Center_Joystick_X / cont;

    printf("Inicialização completa - iniciando leituras reais\n");

    // Operação normal
    while (true)
    {
        adc_select_input(0);
        joystick_data.y_pos = adc_read();
        adc_select_input(1);
        joystick_data.x_pos = adc_read();

        // Debbugando valores do joystick
        // printf("[Tarefa: %s] Leitura do joystick - X: %u, Y: %u\n",
        //        pcTaskGetName(NULL),
        //        joystick_data.x_pos,
        //        joystick_data.y_pos);

        xQueueSend(xQueueJoystickData, &joystick_data, portMAX_DELAY);
    }
}

float map_joystick_to_percentage(int value, int min, int center, int max)
{
    if (value < center)
        return ((float)(value - min) / (center - min)) * 50.0f;
    else
        return 50.0f + ((float)(value - center) / (max - center)) * 50.0f;
}

void vConvertTask(void *params)
{

    sensor_data_t sensor_data;

    while (true)
    {
        joystick_data_t joystick_data;
        xQueueReceive(xQueueJoystickData, &joystick_data, portMAX_DELAY);
        sensor_data.water_level = map_joystick_to_percentage(joystick_data.y_pos, MIN_JOYSTICK_VALUE, Center_Joystick_Y, MAX_JOYSTICK_VALUE);
        sensor_data.rain_level = map_joystick_to_percentage(joystick_data.x_pos, MIN_JOYSTICK_VALUE, Center_Joystick_X, MAX_JOYSTICK_VALUE);
        printf("[Tarefa: %s] Nível de água: %.2f%%, Nível de chuva: %.2f%%\n",
               pcTaskGetName(NULL),
               sensor_data.water_level,
               sensor_data.rain_level);
        xQueueSend(xQueueSensorDataDisplay, &sensor_data, portMAX_DELAY);
        xQueueSend(xQueueSensorDataMatriz, &sensor_data, portMAX_DELAY);
        xQueueSend(xQueueSensorDataBuzzer, &sensor_data, portMAX_DELAY);
        xQueueSend(xQueueSensorLeds, &sensor_data, portMAX_DELAY);
    }
}

void vLedControlTask(void *params)
{
    led_init();
    sensor_data_t sensor_data;
    while (xQueueReceive(xQueueSensorLeds, &sensor_data, portMAX_DELAY) == pdPASS)
    {
        if (sensor_data.water_level >= 70.0f || sensor_data.rain_level >= 80.0f)
        {
            acender_led_rgb(255 * (sensor_data.water_level / 100), 0, 0);
        }
        else
        {
            turn_off_leds();
        }
    }
}

void vDisplayControlTask(void *params)
{
    // Inicialização do I2C
    i2c_init(I2C_PORT, 400 * 1000); // Configuração para 400kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Inicialização do display OLED
    ssd1306_t display;
    ssd1306_init(&display, WIDTH, HEIGHT, false, DISPLAY_ADDR, I2C_PORT);
    ssd1306_config(&display);
    ssd1306_fill(&display, false); // Limpa o display na inicialização
    ssd1306_send_data(&display);

    // Variáveis de controle
    sensor_data_t sensor_data;
    display_state_t current_state = STATE_STABLE;
    display_state_t last_state = STATE_STABLE;
    float last_water_level = -1.0f; // Valor inicial inválido para forçar primeira atualização
    float last_rain_level = -1.0f;
    char buffer_info[64];
    absolute_time_t repeat_time = make_timeout_time_ms(50);

    while (true)
    {
        // Recebe dados da fila
        if (xQueueReceive(xQueueSensorDataDisplay, &sensor_data, portMAX_DELAY) == pdPASS)
        {
            // Determina o estado atual
            if (sensor_data.water_level >= 70.0f && sensor_data.rain_level >= 80.0f)
                current_state = STATE_BOTH_CRITICAL;
            else if (sensor_data.water_level >= 70.0f)
                current_state = STATE_WATER_OVERFLOW;
            else if (sensor_data.rain_level >= 80.0f)
                current_state = STATE_HEAVY_RAIN;
            else
                current_state = STATE_STABLE;

            // Verifica se é hora de atualizar o display
            if (time_reached(repeat_time))
            {
                repeat_time = make_timeout_time_ms(50);

                // Verifica se houve mudança de estado ou valores significativos
                bool update_needed = (current_state != last_state ||
                                      fabs(sensor_data.water_level - last_water_level) >= 0.1f ||
                                      fabs(sensor_data.rain_level - last_rain_level) >= 0.1f);

                if (update_needed)
                {
                    // Limpa o display apenas se o estado mudou
                    if (current_state != last_state)
                        ssd1306_fill(&display, false);

                    // Atualiza valores numéricos (água e chuva)
                    sprintf(buffer_info, "Agua: %.1f%%", sensor_data.water_level);
                    ssd1306_draw_string(&display, buffer_info, 0, 0);

                    sprintf(buffer_info, "Chuva: %.1f%%", sensor_data.rain_level);
                    ssd1306_draw_string(&display, buffer_info, 0, 20);

                    // Exibe mensagens de estado
                    switch (current_state)
                    {
                    case STATE_STABLE:
                        ssd1306_draw_string(&display, "Agua estavel", 0, 30);
                        ssd1306_draw_string(&display, "Chuva estavel", 0, 50);
                        break;
                    case STATE_WATER_OVERFLOW:
                        ssd1306_draw_string(&display, "Transbordando", 0, 30);
                        ssd1306_draw_string(&display, "Chuva estavel", 0, 50);
                        break;
                    case STATE_HEAVY_RAIN:
                        ssd1306_draw_string(&display, "Agua estavel", 0, 30);
                        ssd1306_draw_string(&display, "Chuva forte", 0, 50);
                        break;
                    case STATE_BOTH_CRITICAL:
                        ssd1306_draw_string(&display, "Transbordando", 0, 30);
                        ssd1306_draw_string(&display, "Chuva forte", 0, 50);
                        break;
                    }

                    // Envia dados para o display
                    ssd1306_send_data(&display);

                    // Atualiza valores e estado anteriores
                    last_water_level = sensor_data.water_level;
                    last_rain_level = sensor_data.rain_level;
                    last_state = current_state;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(15)); // Pequena pausa para outras tarefas
    }
}

void vMatrizControlTask(void *params)
{
    // Inicializa a matriz RGB
    npInit(7);

    sensor_data_t sensor_data;

    while (xQueueReceive(xQueueSensorDataMatriz, &sensor_data, portMAX_DELAY) == pdPASS)
    {
        if (sensor_data.water_level >= 70.0f || sensor_data.rain_level >= 80.0f)
        {
            npSetMatrixWithIntensity(caixa_de_desenhos[contador], 1); // Exibe imagem de alerta

            contador++;
            if (contador >= 22)
                contador = 10;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        else
        {
            npClear();
        }
    }
}

void vBuzzerControlTask(void *params)
{
    inicializar_buzzer(BUZZER_PIN); // Inicializa o buzzer no pino especificado
    sensor_data_t sensor_data;
    absolute_time_t next_water_buzz = get_absolute_time(); // Próximo tempo para buzinar (nível de água)
    absolute_time_t next_rain_buzz = get_absolute_time();  // Próximo tempo para buzinar (nível de chuva)
    absolute_time_t buzzer_off_time = nil_time;            // Quando desativar o buzzer
    bool buzzer_ativo = false;                             // Estado do buzzer

    while (true)
    {
        // Recebe dados da fila (bloqueia até receber)
        if (xQueueReceive(xQueueSensorDataBuzzer, &sensor_data, portMAX_DELAY) == pdPASS)
        {
            // Verifica se as condições para ativar o buzzer são atendidas
            if (sensor_data.water_level >= 70.0f || sensor_data.rain_level >= 80.0f)
            {
                // Verifica se é hora de ativar o buzzer para nível de água
                if (sensor_data.water_level >= 70.0f &&
                    absolute_time_diff_us(get_absolute_time(), next_water_buzz) <= 0 &&
                    !buzzer_ativo)
                {
                    ativar_buzzer_com_intensidade(BUZZER_PIN, sensor_data.water_level / 100.0f);
                    buzzer_ativo = true;
                    buzzer_off_time = make_timeout_time_ms(50);  // Buzzer ativo por 50ms
                    next_water_buzz = make_timeout_time_ms(100); // Próximo buzz em 100ms
                }
                // Verifica se é hora de ativar o buzzer para nível de chuva
                if (sensor_data.rain_level >= 80.0f &&
                    absolute_time_diff_us(get_absolute_time(), next_rain_buzz) <= 0 &&
                    !buzzer_ativo)
                {
                    ativar_buzzer_com_intensidade(BUZZER_PIN, sensor_data.rain_level / 100.0f);
                    buzzer_ativo = true;
                    buzzer_off_time = make_timeout_time_ms(50); // Buzzer ativo por 50ms
                    next_rain_buzz = make_timeout_time_ms(200); // Próximo buzz em 200ms
                }
            }
            else
            {
                // Desativa o buzzer se as condições não são atendidas
                desativar_buzzer(BUZZER_PIN);
                buzzer_ativo = false;
            }
        }

        // Desativa o buzzer após o tempo definido (50ms)
        if (buzzer_ativo && absolute_time_diff_us(get_absolute_time(), buzzer_off_time) <= 0)
        {
            desativar_buzzer(BUZZER_PIN);
            buzzer_ativo = false;
        }
    }
}
