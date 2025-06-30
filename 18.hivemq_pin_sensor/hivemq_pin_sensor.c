#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

// Includes para o MQTT e a nova conexão TLS
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h" 

// Includes para os sensores
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "onewire_library.h"
#include "ow_rom.h"
#include "ds18b20.h"

#ifndef MQTT_SERVER
#error "MQTT_SERVER não está definido."
#endif
#ifndef WIFI_SSID
#error "WIFI_SSID não está definido."
#endif
#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD não está definido."
#endif
#ifndef MQTT_USERNAME
#error "MQTT_USERNAME não está definido."
#endif
#ifndef MQTT_PASSWORD
#error "MQTT_PASSWORD não está definido."
#endif
#ifndef MQTT_PORT
#error "MQTT_PORT não está definido." 
#endif

// --- Definições dos Pinos e Tópicos MQTT ---
#define MONITOR_PIN     22
#define ONEWIRE_PIN     20
#define MQTT_TOPIC_TEMP         "pico/w/temperatura"
#define MQTT_TOPIC_PIN_STATUS   "pico/w/pino_status"

// Variável global para o OneWire
OW ow;

// --- Estrutura de Estado do Cliente MQTT ---
typedef struct MQTT_CLIENT_STATE_T {
    ip_addr_t      remote_addr;
    mqtt_client_t* mqtt_client;
    bool           connected;
} MQTT_CLIENT_STATE_T;


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

static void mqtt_pub_request_cb(void *arg, err_t err) {
    if (err != ERR_OK) {
        printf("Falha ao publicar no MQTT, código do erro: %d\n", err);
    }
}

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

void publish_data(MQTT_CLIENT_STATE_T* state) {
    if (!state->connected) {
        return;
    }

    float temperature = read_ds18b20_temperature();
    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);
    printf("Temperatura: %s C\n", temp_str);
    
    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_TEMP, temp_str, strlen(temp_str), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();

    sleep_ms(50);

    const char *pin_status = gpio_get(MONITOR_PIN) ? "ALTO" : "BAIXO";
    printf("Status do Pino %d: %s\n", MONITOR_PIN, pin_status);

    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_PIN_STATUS, pin_status, strlen(pin_status), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();
}

// --- FUNÇÃO DE CONEXÃO ---
void start_mqtt_connection(MQTT_CLIENT_STATE_T* state) {
    struct mqtt_connect_client_info_t client_info = {0};
    char client_id[20];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));
    
    // 1. Criar um cliente MQTT padrão
    state->mqtt_client = mqtt_client_new();
    if (state->mqtt_client == NULL) {
        printf("Falha ao criar o cliente MQTT.\n");
        return;
    }

    // 2. Criar a configuração TLS
    printf("Criando configuração TLS...\n");
    struct altcp_tls_config *tls_config = altcp_tls_create_config_client(NULL, 0);
    if (tls_config == NULL) {
        printf("Falha ao criar a configuração TLS.\n");
        return;
    }
    
    // 3. Preencher as informações de conexão, incluindo a configuração TLS
    client_info.client_id = client_id;
    client_info.keep_alive = 60;
    client_info.client_user = MQTT_USERNAME;
    client_info.client_pass = MQTT_PASSWORD;
    client_info.will_msg = NULL;
    client_info.tls_config = tls_config;

    // 4. Conectar usando a função padrão. Ela usará TLS por causa da configuração acima.
    err_t err = mqtt_client_connect(state->mqtt_client, &state->remote_addr, MQTT_PORT, mqtt_connection_cb, state, &client_info);
    
    if (err != ERR_OK) {
        printf("mqtt_client_connect retornou o erro: %d\n", err);
    }
}

// --- Aplicação Principal ---
int main() {
    stdio_init_all();

    gpio_init(MONITOR_PIN);
    gpio_set_dir(MONITOR_PIN, GPIO_IN);
    gpio_pull_up(MONITOR_PIN);

    PIO pio = pio0;
    uint offset = pio_add_program(pio, &onewire_program);
    if (!ow_init(&ow, pio, offset, ONEWIRE_PIN)) {
        printf("Falha ao inicializar o driver OneWire\n");
    } else {
        int num_devs = ow_romsearch(&ow, NULL, 0, OW_SEARCH_ROM);
        printf("Encontrados %d dispositivos DS18B20\n", num_devs);
    }
    
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar o Wi-Fi\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);

    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return -1;
    }
    printf("Conectado ao Wi-Fi!\n");
    
    MQTT_CLIENT_STATE_T* state = calloc(1, sizeof(MQTT_CLIENT_STATE_T));
    if (!state || dns_gethostbyname(MQTT_SERVER, &state->remote_addr, NULL, NULL) != ERR_OK) {
         printf("Falha ao resolver o DNS do servidor MQTT ou alocar estado\n");
         return -1;
    }

    start_mqtt_connection(state);
    
    absolute_time_t next_publish_time = make_timeout_time_ms(1000);

    while (true) {
        cyw43_arch_poll();

        if (absolute_time_diff_us(get_absolute_time(), next_publish_time) < 0) {
            publish_data(state);
            printf("---\n");
            next_publish_time = make_timeout_time_ms(1000);
        }
    }

    free(state);
    cyw43_arch_deinit();
    return 0;
}