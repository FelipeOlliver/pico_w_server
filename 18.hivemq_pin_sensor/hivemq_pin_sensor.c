#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

// Includes para o MQTT
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"

// Includes para os sensores
#include "hardware/gpio.h"
#include "onewire_library.h"
#include "ow_rom.h"
#include "ds18b20.h"

// --- Configurações  ---
#ifndef MQTT_SERVER
#error "MQTT_SERVER não está definido."
#endif
#ifndef MQTT_PORT
#error "MQTT_PORT não está definido."
#endif
#ifndef WIFI_SSID
#error "WIFI_SSID não está definido."
#endif
#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD não está definido."
#endif

// --- Definições dos Pinos e Tópicos MQTT ---
#define MONITOR_PIN     22
#define ONEWIRE_PIN     20

// MUDANÇA: Tópicos mais únicos para o broker público
#define MQTT_TOPIC_TEMP         "bitdoglab/temperature"
#define MQTT_TOPIC_PIN_STATUS   "bitdoglab/pin22_status"

// --- Estrutura de Estado do Cliente MQTT ---
typedef struct MQTT_CLIENT_STATE_T {
    ip_addr_t      remote_addr;
    mqtt_client_t* mqtt_client;
    bool           connected;
} MQTT_CLIENT_STATE_T;

// Variável global para o OneWire
OW ow;

// --- Callbacks do MQTT ---
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_STATE_T* state = (MQTT_CLIENT_STATE_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT conectado com sucesso!\n");
        state->connected = true;
    } else {
        printf("Falha na conexão MQTT, status: %d\n", status);
        state->connected = false;
    }
}

// --- Função de Conexão MQTT ---
void start_mqtt_connection(MQTT_CLIENT_STATE_T* state) {
    struct mqtt_connect_client_info_t client_info = {0};
    char client_id[20];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));
    
    client_info.client_id = client_id;
    client_info.keep_alive = 60;

    client_info.client_user = NULL;
    client_info.client_pass = NULL;

    state->mqtt_client = mqtt_client_new();
    if (state->mqtt_client == NULL) {
        printf("Falha ao criar novo cliente mqtt\n");
        return;
    }

    err_t err = mqtt_client_connect(state->mqtt_client, &state->remote_addr, MQTT_PORT, mqtt_connection_cb, state, &client_info);
    if (err != ERR_OK) {
        printf("mqtt_client_connect retornou o erro: %d\n", err);
    }
}

// --- Função de Leitura do Sensor DS18B20 ---
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

// --- Função para Publicar Dados ---
void publish_data(MQTT_CLIENT_STATE_T* state) {
    if (!state->connected) {
        return;
    }

    char buffer[20];

    // Leitura e Publicação da Temperatura
    float temperature = read_ds18b20_temperature();
    snprintf(buffer, sizeof(buffer), "%.2f", temperature);
    printf("Publicando Temperatura: %s C\n", buffer);
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_TEMP, buffer, strlen(buffer), 1, 0, NULL, NULL);

    sleep_ms(50); 

    // Leitura e Publicação do Status do Pino
    const char *pin_status = gpio_get(MONITOR_PIN) ? "ALTO" : "BAIXO";
    snprintf(buffer, sizeof(buffer), "%s", pin_status);
    printf("Publicando Status do Pino %d: %s\n", MONITOR_PIN, buffer);
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_PIN_STATUS, buffer, strlen(buffer), 1, 0, NULL, NULL);
}

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    MQTT_CLIENT_STATE_T* state = (MQTT_CLIENT_STATE_T*)callback_arg;

    if (ipaddr != NULL) {
        printf("DNS resolvido para %s: %s\n", name, ipaddr_ntoa(ipaddr));
        state->remote_addr = *ipaddr;
        start_mqtt_connection(state);
    } else {
        printf("Falha ao resolver o DNS (callback)\n");
    }
}

// --- Aplicação Principal ---
int main() {
    stdio_init_all();

    // Inicialização dos Pinos
    gpio_init(MONITOR_PIN);
    gpio_set_dir(MONITOR_PIN, GPIO_IN);
    gpio_pull_up(MONITOR_PIN);

    // Inicializa o sensor DS18B20
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &onewire_program);
    if (!ow_init(&ow, pio, offset, ONEWIRE_PIN)) {
        printf("Falha ao inicializar o driver OneWire\n");
    } else {
        printf("Driver OneWire inicializado.\n");
    }
    
    // Conexão Wi-Fi
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return -1;
    }
    printf("Conectado ao Wi-Fi!\n");
    sleep_ms(1000); // Pausa para estabilizar a rede


    MQTT_CLIENT_STATE_T* state = calloc(1, sizeof(MQTT_CLIENT_STATE_T));
    printf("Resolvendo DNS para %s...\n", MQTT_SERVER);
    err_t err = dns_gethostbyname(MQTT_SERVER, &state->remote_addr, dns_found_cb, state);
    if (err == ERR_OK) {
        printf("DNS resolvido imediatamente para %s\n", ipaddr_ntoa(&state->remote_addr));
        start_mqtt_connection(state);
    } else if (err == ERR_INPROGRESS) {
        printf("Resolução DNS em andamento...\n");
        // A conexão será iniciada automaticamente no callback
    } else {
        printf("Erro ao iniciar resolução DNS: %d\n", err);
    }
    
    // Loop Principal
    while (true) {
        cyw43_arch_poll(); // Mantém o Wi-Fi funcionando

        // MUDANÇA: Publica os dados a cada 1 segundo
        if(state->connected) {
            publish_data(state);
        }
        printf("---\n");
        sleep_ms(1000);
    }
    return 0;
}