#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/adc.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"


// --- Configuração ---
#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "wifi_password"

// Definição dos pinos do Joystick
#define VRX_PIN 26 // Eixo X (ADC0)
#define VRY_PIN 27 // Eixo Y (ADC1)

// --- Constantes de Calibração ---
// Valores observados trocados para corrigir os eixos
const int CENTER_X = 2150; 
const int CENTER_Y = 1954;
// Tamanho da "zona morta". Valores dentro desta margem do centro serão considerados 0.
const int DEAD_ZONE = 150; 

// --- Variáveis Globais ---
static volatile uint16_t latest_vrx = 0;
static volatile uint16_t latest_vry = 0;

// --- Função Auxiliar de Mapeamento ---
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


// --- Funções do Servidor Web ---
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);

    // --- Calibração dos valores ---
    long mapped_x, mapped_y;

    // Processa o eixo X
    if (abs(latest_vrx - CENTER_X) < DEAD_ZONE) {
        mapped_x = 0;
    } else {
        mapped_x = map(latest_vrx, 0, 4095, -100, 100);
    }

    // Processa o eixo Y (invertemos para que 'para cima' seja positivo)
    if (abs(latest_vry - CENTER_Y) < DEAD_ZONE) {
        mapped_y = 0;
    } else {
        mapped_y = map(latest_vry, 0, 4095, 100, -100); // Invertido
    }

    // --- Cria a resposta HTML ---
    char html[1024];
    snprintf(html, sizeof(html),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html><head>"
        "<title>Posi&ccedil;&atilde;o do Joystick</title>"
        "<meta http-equiv=\"refresh\" content=\"1\">"
        "<style>body { font-family: monospace; font-size: 2.5em; line-height: 1.6; margin: 40px; background-color: #222; color: #0f0; }"
        " .raw { font-size: 0.5em; color: #888; }"
        "</style>"
        "</head><body>"
        "<h1>STATUS DO JOYSTICK (CALIBRADO)</h1>"
        "EIXO X: %ld<br>"
        "EIXO Y: %ld<br>"
        "<div class=\"raw\">(Raw X: %u, Raw Y: %u)</div>"
        "</body></html>",
        mapped_x, mapped_y, latest_vrx, latest_vry
    );

    tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    return tcp_close(tpcb);
}

static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, tcp_server_accept);
    return ERR_OK;
}

// --- Função Principal ---
int main() {
    stdio_init_all();
    printf("Iniciando Servidor Web do Joystick...\n");

    // Inicializa o ADC e os pinos do Joystick
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    // [O resto do código de inicialização do Wi-Fi e do servidor continua aqui...]
    if (cyw43_arch_init()) { return -1; }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) { return -1; }
    printf("Endereço de IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    struct tcp_pcb *server = tcp_new();
    tcp_bind(server, IP_ADDR_ANY, 80);
    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);
    printf("Servidor web escutando na porta 80\n\n");

    // --- Laço Principal ---
    while (true) {
        cyw43_arch_poll();
        
        // ** AQUI ESTÁ A CORREÇÃO **
        // Lemos o canal ADC0 (GP26), mas salvamos na variável do eixo Y
        adc_select_input(0);
        latest_vry = adc_read();

        // Lemos o canal ADC1 (GP27), mas salvamos na variável do eixo X
        adc_select_input(1);
        latest_vrx = adc_read();
        
        sleep_ms(100);
    }
    return 0;
}