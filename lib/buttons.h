#ifndef BUTTONS_H
#define BUTTONS_H

#include "pico/stdlib.h"

// Definições dos pinos dos botões
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define BUTTON_C_PIN 22 // Pino do joystick

// Função para inicializar todos os botões
void buttons_init(void);

// Função para registrar um callback de interrupção para um botão específico
// button_pin: o pino do botão a ser configurado
// handler: o ponteiro para a função de callback
void button_set_irq_callback(uint gpio_pin, gpio_irq_callback_t handler);

#endif // BUTTONS_H