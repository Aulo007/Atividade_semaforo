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

// Função para registrar um callback de interrupção para um botão específico
void button_set_irq_callback(uint gpio_pin, gpio_irq_callback_t handler)
{
    // Garante que o manipulador de interrupção global seja configurado apenas uma vez
    // (Pico SDK permite um único manipulador global para todas as GPIOs)
    gpio_set_irq_callback(handler);
    // Habilita a interrupção para a borda de descida (pressão do botão)
    gpio_set_irq_enabled(gpio_pin, GPIO_IRQ_EDGE_FALL, true);
}