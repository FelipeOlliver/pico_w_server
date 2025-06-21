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

// --- Configurações ---
// Estes valores serão definidos no seu arquivo CMakeLists.txt
#ifndef MQTT_SERVER
#error "MQTT_SERVER não está definido. Por favor, defina-o no seu arquivo CMakeLists.txt"
#endif

#ifndef WIFI_SSID
#error "WIFI_SSID não está definido. Por favor, defina-o no seu arquivo CMakeLists.txt"
#endif

#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD não está definido. Por favor, defina-o no seu arquivo CMakeLists.txt"
#endif

#ifndef MQTT_USERNAME
#error "MQTT_USERNAME não está definido. Por favor, defina-o no seu arquivo CMakeLists.txt"
#endif

#ifndef MQTT_PASSWORD
#error "MQTT_PASSWORD não está definido. Por favor, defina-o no seu arquivo CMakeLists.txt"
#endif


// --- Definições dos Pinos e Tópicos MQTT ---
#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6
#define ONEWIRE_PIN  20

#define MQTT_TOPIC_TEMP    "pico/temperatura"
#define MQTT_TOPIC_BUTTON_A "pico/botao_a"
#define MQTT_TOPIC_BUTTON_B "pico/botao_b"


// --- Estrutura de Estado do Cliente MQTT ---
typedef struct MQTT_CLIENT_STATE_T {
    ip_addr_t      remote_addr;
    mqtt_client_t* mqtt_client;
    bool           connected;
} MQTT_CLIENT_STATE_T;

// Variável global para o OneWire
OW ow;

// --- Callbacks do MQTT (iguais ao código anterior) ---

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_STATE_T* state = (MQTT_CLIENT_STATE_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT conectado!\n");
        state->connected = true;
    } else {
        printf("Falha na conexão MQTT, status: %d\n", status);
        state->connected = false;
    }
}

static void mqtt_pub_request_cb(void *arg, err_t err) {
    if (err != ERR_OK) {
        printf("Falha ao publicar no MQTT, código do erro %d\n", err);
    }
}

// --- Função de Conexão MQTT (igual ao código anterior) ---
void start_mqtt_connection(MQTT_CLIENT_STATE_T* state) {
    struct mqtt_connect_client_info_t client_info = {0};
    char client_id[20];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));
    
    client_info.client_id = client_id;
    client_info.keep_alive = 60;
    client_info.client_user = MQTT_USERNAME;
    client_info.client_pass = MQTT_PASSWORD;

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
        return; // Não faz nada se não estiver conectado
    }

    // --- Leitura e Publicação da Temperatura ---
    float temperature = read_ds18b20_temperature();
    char temp_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);
    printf("Lendo Temperatura: %s C\n", temp_str);
    
    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_TEMP, temp_str, strlen(temp_str), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();

    sleep_ms(100); // Pequena pausa entre publicações

    // --- Leitura e Publicação do Botão A ---
    const char *state_a = !gpio_get(BUTTON_A_PIN) ? "ON" : "OFF";
    printf("Lendo Botao A: %s\n", state_a);

    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_BUTTON_A, state_a, strlen(state_a), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();

    sleep_ms(100); // Pequena pausa

    // --- Leitura e Publicação do Botão B ---
    const char *state_b = !gpio_get(BUTTON_B_PIN) ? "ON" : "OFF";
    printf("Lendo Botao B: %s\n", state_b);

    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_BUTTON_B, state_b, strlen(state_b), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();
}


// --- Aplicação Principal ---
int main() {
    stdio_init_all();

    // --- Inicialização dos Botões ---
    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_init(BUTTON_B_PIN);
    gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_B_PIN);

    // --- Inicializa o sensor DS18B20 ---
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &onewire_program);
    if (!ow_init(&ow, pio, offset, ONEWIRE_PIN)) {
        printf("Falha ao inicializar o driver OneWire\n");
    } else {
        int num_devs = ow_romsearch(&ow, NULL, 0, OW_SEARCH_ROM);
        printf("Encontrados %d dispositivos DS18B20\n", num_devs);
    }
    
    // --- Conexão Wi-Fi e MQTT ---
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
    if (!state || !ip4addr_aton(MQTT_SERVER, &state->remote_addr)) {
        printf("Falha ao inicializar o estado do MQTT\n");
        return -1;
    }

    start_mqtt_connection(state);
    
    // --- Loop Principal ---
    while (true) {
        cyw43_arch_poll(); // Mantém o Wi-Fi funcionando

        // Publica os dados a cada 15 segundos
        publish_data(state);
        printf("---\n");
        sleep_ms(15000);
    }

    // Limpeza (nunca será alcançado no loop infinito)
    free(state);
    cyw43_arch_deinit();
    return 0;
}