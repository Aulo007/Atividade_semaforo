#include "buttons.h"
#include "hardware/gpio.h"

// Função para inicializar um único botão
static void button_init_single(uint gpio_pin)
{
    gpio_init(gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_IN);
    gpio_pull_up(gpio_pin); // Ativa o pull-up interno
}

// Função para inicializar todos os botões
void buttons_init(void)
{
    button_init_single(BUTTON_A_PIN);
    button_init_single(BUTTON_B_PIN);
    button_init_single(BUTTON_C_PIN);
}

