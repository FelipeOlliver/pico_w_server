#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h" // Para acessar netif_default e IP

// Configurações de Wi-Fi
#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "wifi_password"

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN
// Definição dos pinos do botões
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

// Função de callback para processar requisições HTTP
static err_t tcp_server_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    // Se p for NULL, significa que o cliente fechou a conexão
    if (p == NULL) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    if (err != ERR_OK) {
        pbuf_free(p);
        return err;
    }

    // Reconhece que dados foram recebidos
    tcp_recved(tpcb, p->tot_len);

    // Não estamos processando o conteúdo da solicitação, então limpamos o buffer imediatamente
    pbuf_free(p);

    // --- Leitura dos status dos botões ---
    // Com pull-ups, gpio_get() é 'false' quando pressionado. Invertemos para "ON".
    const char *state_a = !gpio_get(BUTTON_A_PIN) ? "ON" : "OFF";
    const char *state_b = !gpio_get(BUTTON_B_PIN) ? "ON" : "OFF";

    // --- Resposta HTML ---
    char html[512];
    snprintf(html, sizeof(html),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n" // Informa ao navegador que estamos fechando a conexão
        "\r\n"
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "<title>Button Status</title>\n"
        // Esta meta tag atualiza a página automaticamente a cada segundo
        "<meta http-equiv=\"refresh\" content=\"1\">\n"
        "<style>\n"
        "  body { font-family: monospace; font-size: 2.5em; line-height: 1.6; margin: 20px; }\n"
        "</style>\n"
        "</head>\n"
        "<body>\n"
        "BUTTON A STATE: %s<br>\n"
        "BUTTON B STATE: %s\n"
        "</body>\n"
        "</html>",
        state_a, state_b
    );

    // Escreva a resposta HTML ao client
    err_t write_err = tcp_write(tpcb, html, strlen(html), TCP_WRITE_FLAG_COPY);
    if (write_err != ERR_OK) {
        printf("Error writing to TCP stream: %d\n", write_err);
        return write_err;
    }

    // Agora, fechamos a conexão. O LwIP garantirá que os dados sejam enviados antes que o fechamento seja concluído.
    return tcp_close(tpcb);
}


// Função de callback ao aceitar conexões TCP
static err_t tcp_server_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    tcp_recv(newpcb, tcp_server_recv);
    return ERR_OK;
}

// Função principal
int main()
{
    stdio_init_all();

    // --- Inicialização dos Botões ---
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);


    // cyw43_arch_deinit(); // Desativa o Wi-Fi

    while (cyw43_arch_init())
    {
        printf("Falha ao inicializar Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    cyw43_arch_gpio_put(LED_PIN, 0);
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi...\n");
    while (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 20000))
    {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
        return -1;
    }

    printf("Conectado ao Wi-Fi\n");

    if (netif_default)
    {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    // Configura o servidor TCP
    struct tcp_pcb *server = tcp_new();
    if (!server)
    {
        printf("Falha ao criar servidor TCP\n");
        return -1;
    }

    if (tcp_bind(server, IP_ADDR_ANY, 80) != ERR_OK)
    {
        printf("Falha ao associar servidor TCP à porta 80\n");
        return -1;
    }

    server = tcp_listen(server);
    tcp_accept(server, tcp_server_accept);

    printf("Servidor ouvindo na porta 80\n");

    while (true)
    {
        cyw43_arch_poll();
    }

    cyw43_arch_deinit();
    return 0;
}
