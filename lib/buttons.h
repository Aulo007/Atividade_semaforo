#ifndef BUTTONS_H
#define BUTTONS_H

#include "pico/stdlib.h"

// Definições dos pinos dos botões
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define BUTTON_C_PIN 22 // Pino do joystick

// Função para inicializar todos os botões
void buttons_init(void);

#endif // BUTTONS_H