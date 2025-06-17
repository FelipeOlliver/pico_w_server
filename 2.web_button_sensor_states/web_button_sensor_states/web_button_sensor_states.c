#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/cyw43_arch.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/netif.h" // Para acessar netif_default e IP
#include "onewire_library.h"
#include "ow_rom.h"
#include "ds18b20.h"

// Configurações de Wi-Fi
#define WIFI_SSID "wifi_ssid"
#define WIFI_PASSWORD "wifi_password"

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN
// Definição dos pinos do botões
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define ONEWIRE_PIN 20

// --- Variáveis Globais ---
OW ow; // Variável global onewire
static volatile float latest_temperature = -999.0f; // Salva a ultima temperatura obtida

// --- Função de leitura do DS18B20  ---
float read_ds18b20_temperature() {

    ow_reset(&ow);
    ow_send(&ow, OW_SKIP_ROM);
    ow_send(&ow, DS18B20_CONVERT_T);

    sleep_ms(750);

    ow_reset(&ow);
    ow_send(&ow, OW_SKIP_ROM);
    ow_send(&ow, DS18B20_READ_SCRATCHPAD);
    
    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = ow_read(&ow);
    }

    int16_t temp_raw = (scratchpad[1] << 8) | scratchpad[0];
    return (float)temp_raw / 16.0f;
}

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

    // Cria string do dado de temperatura
    char temp_str[32];
    if (latest_temperature <= -999.0f) {
        snprintf(temp_str, sizeof(temp_str), "Reading...");
    } else if (latest_temperature == -998.0f){
        snprintf(temp_str, sizeof(temp_str), "CRC Error");
    } else {
        snprintf(temp_str, sizeof(temp_str), "%.2f&deg;C", latest_temperature);
    }

    // --- Resposta HTML ---
    char html[1024];
    snprintf(html, sizeof(html),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n"
        "\r\n"
        "<!DOCTYPE html><html><head>"
        "<title>BitDogLab Status</title>"
        "<meta http-equiv=\"refresh\" content=\"2\">"
        "<style>body { font-family: monospace; font-size: 2.5em; line-height: 1.6; margin: 20px; }</style>"
        "</head><body>"
        "BUTTON A STATE: %s<br>"
        "BUTTON B STATE: %s<br>"
        "DS18B20 TEMPERATURE: %s"
        "</body></html>",
        state_a, state_b, temp_str
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

    // --- Inicializa o sensor DS18B20  ---
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &onewire_program);
    if (!ow_init(&ow, pio, offset, ONEWIRE_PIN)) {
        printf("Failed to initialize OneWire driver\n");
    } else {
        // Check for devices
        int num_devs = ow_romsearch(&ow, NULL, 0, OW_SEARCH_ROM);
        printf("Found %d DS18B20 devices\n", num_devs);
        if (num_devs == 0) {
            latest_temperature = -997.0f; // Special code for no device found
        }
    }


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

    // Cria variáveis de controle do tempo de envio do dado de temperatura
    uint32_t last_temp_read_time = 0;
    const uint32_t temp_read_interval_ms = 5000; // Atualiza a cada 5 segundos

    while (true)
    {
        cyw43_arch_poll();
        // Lê periodicamente a temperatura
        if (time_us_32() - last_temp_read_time > temp_read_interval_ms * 1000) {
            last_temp_read_time = time_us_32();
            printf("Reading temperature...\n");
            latest_temperature = read_ds18b20_temperature();
            printf("Temperature is: %.2f C\n", latest_temperature);
        }
    }

    cyw43_arch_deinit();
    return 0;
}
