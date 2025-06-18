#include "pico/stdlib.h"
#include "hardware/adc.h" 
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h" 


// Configurações de Wi-Fi
#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "wifi_password"

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN
// Definição dos pinos
#define VRX_PIN 26
#define VRY_PIN 27

// --- Constantes de Calibração ---
// Use os valores que você observou com o joystick parado
const int CENTER_X = 2150; 
const int CENTER_Y = 1954;
// Tamanho da "zona morta". Valores dentro desta margem do centro serão considerados 0.
const int DEAD_ZONE = 150;
// Limiar para considerar uma direção como "ativa" (de 0 a 100)
const int DIRECTION_THRESHOLD = 40;  

// Estrutura para guardar o estado de cada conexão
typedef struct TCP_SERVER_T_ {
    struct tcp_pcb *client_pcb;
    int sent_chunk; // Qual pedaço acabamos de enviar
} TCP_SERVER_T;

static err_t send_chunk(void *arg); // Protótipo da função

// Função para liberar a memória do nosso estado
static void free_server_state(TCP_SERVER_T *state) {
    if (state) {
        if (state->client_pcb) {
            tcp_close(state->client_pcb);
            state->client_pcb = NULL;
        }
        free(state);
    }
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Callback de erro da LwIP
static void tcp_server_err(void *arg, err_t err) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    printf("TCP error %d\n", err);
    free_server_state(state);
}

// Callback chamado quando o cliente confirma o recebimento de um pedaço
static err_t tcp_server_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    // Incrementa o contador para enviar o próximo pedaço
    state->sent_chunk++; 
    // Envia o próximo pedaço
    return send_chunk(arg);
}

// Callback chamado quando recebemos uma requisição do cliente
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (p == NULL) {
        return ERR_OK; // Conexão fechada pelo cliente
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    // A requisição do cliente inicia o envio do primeiro pedaço
    return send_chunk(arg);
}

// Função principal que envia um pedaço de cada vez, baseada no estado
static err_t send_chunk(void *arg) {
    TCP_SERVER_T *state = (TCP_SERVER_T*)arg;
    err_t err = ERR_OK;

    // --- Lógica de Calibração e Direção ---
    static volatile uint16_t latest_vrx, latest_vry;
    adc_select_input(1); latest_vrx = adc_read();
    adc_select_input(0); latest_vry = adc_read();

    long mapped_x, mapped_y;
    if (abs(latest_vrx - CENTER_X) < DEAD_ZONE) { mapped_x = 0; } else { mapped_x = map(latest_vrx, 0, 4095, -100, 100); }
    
    if (abs(latest_vry - CENTER_Y) < DEAD_ZONE) { mapped_y = 0; } else { mapped_y = map(latest_vry, 0, 4095, -100, 100); }
    
    const char *direction_str = "Repouso";
    int rotation_angle = 0;

    if (mapped_y > DIRECTION_THRESHOLD && mapped_x > DIRECTION_THRESHOLD) { direction_str = "Nordeste"; rotation_angle = 45; } 
    else if (mapped_y > DIRECTION_THRESHOLD && mapped_x < -DIRECTION_THRESHOLD) { direction_str = "Noroeste"; rotation_angle = 315; } 
    else if (mapped_y < -DIRECTION_THRESHOLD && mapped_x > DIRECTION_THRESHOLD) { direction_str = "Sudeste"; rotation_angle = 135; } 
    else if (mapped_y < -DIRECTION_THRESHOLD && mapped_x < -DIRECTION_THRESHOLD) { direction_str = "Sudoeste"; rotation_angle = 225; } 
    else if (mapped_y > DIRECTION_THRESHOLD) { direction_str = "Norte"; rotation_angle = 0; } 
    else if (mapped_y < -DIRECTION_THRESHOLD) { direction_str = "Sul"; rotation_angle = 180; } 
    else if (mapped_x > DIRECTION_THRESHOLD) { direction_str = "Leste"; rotation_angle = 90; } 
    else if (mapped_x < -DIRECTION_THRESHOLD) { direction_str = "Oeste"; rotation_angle = 270; }

    const char *arrow_visibility = ""; 
    if (strcmp(direction_str, "Repouso") == 0) {
        arrow_visibility = " visibility=\"hidden\"";
    }

    switch(state->sent_chunk) {
        case 0: { // Pedaço 1: Cabeçalhos
            const char *part1 = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n"
                                "<!DOCTYPE html><html><head><title>Rosa dos Ventos Gr&aacute;fica</title>"
                                "<meta http-equiv=\"refresh\" content=\"1\">"
                                "<style>"
                                " body { font-family: monospace; text-align: center; margin-top: 30px; background-color: #1a1a1a; color: #eee; }"
                                " h1 { color: #0f0; margin-bottom: 20px; }"
                                " .direction-text { font-size: 2.5em; font-weight: bold; color: #ff0; height: 50px; }"
                                " .compass-svg { margin-top: 20px; }"
                                "</style></head><body><h1>ROSA DOS VENTOS</h1>";
            err = tcp_write(state->client_pcb, part1, strlen(part1), 0);
            break;
        }
        case 1: { // Pedaço 2: SVG Estático
            const char *part2 = "<svg width=\"300\" height=\"300\" viewBox=\"0 0 200 200\" class=\"compass-svg\">"
                                "<circle cx=\"100\" cy=\"100\" r=\"95\" fill=\"#222\" stroke=\"#444\" stroke-width=\"2\" />"
                                "<line x1=\"100\" y1=\"10\" x2=\"100\" y2=\"190\" stroke=\"#555\" stroke-width=\"1\"/>"
                                "<line x1=\"10\" y1=\"100\" x2=\"190\" y2=\"100\" stroke=\"#555\" stroke-width=\"1\"/>"
                                "<line x1=\"30\" y1=\"30\" x2=\"170\" y2=\"170\" stroke=\"#444\" stroke-width=\"1\"/>"
                                "<line x1=\"30\" y1=\"170\" x2=\"170\" y2=\"30\" stroke=\"#444\" stroke-width=\"1\"/>"
                                "<text x=\"95\" y=\"22\" font-size=\"12\" fill=\"#0f0\">N</text>"
                                "<text x=\"95\" y=\"188\" font-size=\"12\" fill=\"#0f0\">S</text>"
                                "<text x=\"180\" y=\"104\" font-size=\"12\" fill=\"#0f0\">L</text>"
                                "<text x=\"10\" y=\"104\" font-size=\"12\" fill=\"#0f0\">O</text>";
            err = tcp_write(state->client_pcb, part2, strlen(part2), 0);
            break;
        }
        case 2: { // Pedaço 3: SVG Dinâmico (Seta)
            static char buffer[150];
            snprintf(buffer, sizeof(buffer), "<g transform=\"rotate(%d 100 100)\"%s><polygon points=\"100,15 110,45 90,45\" fill=\"#f00\" /></g></svg>",
                     rotation_angle, arrow_visibility);
            err = tcp_write(state->client_pcb, buffer, strlen(buffer), 0);
            break;
        }
        case 3: { // Pedaço 4: Final do HTML
            static char buffer[128];
            snprintf(buffer, sizeof(buffer), "<div class=\"direction-text\">%s</div></body></html>", direction_str);
            err = tcp_write(state->client_pcb, buffer, strlen(buffer), 0);
            break;
        }
        default: { // Todos os dados foram enviados
            free_server_state(state);
            return ERR_OK;
        }
    }
    if (err != ERR_OK) {
        printf("Error writing chunk %d: %d\n", state->sent_chunk, err);
        free_server_state(state);
        return err;
    }
    return tcp_output(state->client_pcb);
}

// Callback chamado para aceitar uma nova conexão
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    // Aloca memória para o nosso estado
    TCP_SERVER_T *state = calloc(1, sizeof(TCP_SERVER_T));
    if (!state) {
        return ERR_MEM;
    }
    state->client_pcb = newpcb;
    state->sent_chunk = 0;

    // Associa nosso estado à conexão e define os callbacks
    tcp_arg(newpcb, state);
    tcp_recv(newpcb, tcp_server_recv);
    tcp_sent(newpcb, tcp_server_sent);
    tcp_err(newpcb, tcp_server_err);

    return ERR_OK;
}



// --- Função Principal ---
int main() {
    stdio_init_all();
    printf("Iniciando Servidor Web - Rosa dos Ventos (Versão Final)...\n");
    
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar.\n");
        return 1;
    }
    printf("Conectado! IP: %s\n", ipaddr_ntoa(&netif_default->ip_addr));

    struct tcp_pcb *pcb = tcp_new();
    tcp_bind(pcb, IP_ADDR_ANY, 80);
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, tcp_server_accept);
    printf("Servidor escutando na porta 80\n");

    while (true) {
        cyw43_arch_poll();
    }
    return 0;
}