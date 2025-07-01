#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <stdint.h>

// --- Configuração dos Pinos I2C ---
// Pinos baseados no seu código funcional.
#define OLED_I2C_SDA_PIN 14
#define OLED_I2C_SCL_PIN 15

/**
 * @brief Inicializa a comunicação I2C e o display OLED.
 * Deve ser chamada uma vez no início do programa.
 */
void oled_init(void);

/**
 * @brief Limpa o buffer do display (apaga tudo).
 * Após chamar esta função, é preciso chamar oled_render() para efetivar na tela.
 */
void oled_clear_buffer(void);

/**
 * @brief Escreve uma string de texto no buffer do display.
 * * @param x Posição inicial no eixo X (0-127).
 * @param y Posição inicial no eixo Y (0-63). A fonte tem 8 pixels de altura, então use múltiplos de 8 (0, 8, 16, ...).
 * @param str A string a ser escrita.
 */
void oled_write_string(int16_t x, int16_t y, char *str);

/**
 * @brief Envia o conteúdo do buffer para a tela do display.
 * Todas as alterações feitas com oled_write_string e oled_clear_buffer só aparecem após chamar esta função.
 */
void oled_render(void);

#endif // OLED_DRIVER_H
