#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "hardware/adc.h"
#include <math.h> // Para a função abs()

#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"

// --- Configurações (Definidas no CMakeLists.txt) ---
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

// --- Definições dos Pinos e Tópicos ---
#define VRX_PIN 26 // Eixo X (ADC0)
#define VRY_PIN 27 // Eixo Y (ADC1)

#define MQTT_TOPIC_COMPASS_DIRECTION "compass/direction"
#define MQTT_TOPIC_COMPASS_ANGLE     "compass/angle"

// --- Constantes de Calibração ---
const int CENTER_X = 2150; 
const int CENTER_Y = 1954;
const int DEAD_ZONE = 150;
const int DIRECTION_THRESHOLD = 40;

// --- Estrutura de Estado do Cliente MQTT ---
typedef struct MQTT_CLIENT_STATE_T {
    ip_addr_t      remote_addr;
    mqtt_client_t* mqtt_client;
    bool           connected;
} MQTT_CLIENT_STATE_T;

// --- Callbacks e Conexão MQTT (Reutilizados dos projetos anteriores) ---
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_STATE_T* state = (MQTT_CLIENT_STATE_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("MQTT conectado!\n");
        state->connected = true;
    } else {
        printf("Falha na conexão MQTT, status: %d\n", status);
    }
}

static void mqtt_pub_request_cb(void *arg, err_t err) {
    if (err != ERR_OK) {
        printf("Falha ao publicar no MQTT, código do erro %d\n", err);
    }
}

void start_mqtt_connection(MQTT_CLIENT_STATE_T* state) {
    struct mqtt_connect_client_info_t client_info = {0};
    char client_id[20];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));
    client_info.client_id = client_id;
    client_info.keep_alive = 60;
    client_info.client_user = MQTT_USERNAME;
    client_info.client_pass = MQTT_PASSWORD;
    state->mqtt_client = mqtt_client_new();
    mqtt_client_connect(state->mqtt_client, &state->remote_addr, MQTT_PORT, mqtt_connection_cb, state, &client_info);
}

// --- Funções Específicas da Rosa dos Ventos ---
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void publish_compass_data(MQTT_CLIENT_STATE_T* state) {
    if (!state->connected) return;

    // Leitura dos valores brutos
    adc_select_input(0);
    uint16_t raw_y = adc_read();
    adc_select_input(1);
    uint16_t raw_x = adc_read();

    // Lógica de calibração e direção
    long mapped_x, mapped_y;
    if (abs(raw_x - CENTER_X) < DEAD_ZONE) { mapped_x = 0; } else { mapped_x = map(raw_x, 0, 4095, -100, 100); }
    if (abs(raw_y - CENTER_Y) < DEAD_ZONE) { mapped_y = 0; } else { mapped_y = map(raw_y, 0, 4095, 100, -100); }
    
    const char *direction_str = "Repouso";
    int rotation_angle = 0; // O ângulo será usado pela página web

    if (mapped_y > DIRECTION_THRESHOLD && mapped_x > DIRECTION_THRESHOLD) { direction_str = "Nordeste"; rotation_angle = 45; } 
    else if (mapped_y > DIRECTION_THRESHOLD && mapped_x < -DIRECTION_THRESHOLD) { direction_str = "Noroeste"; rotation_angle = 315; } 
    else if (mapped_y < -DIRECTION_THRESHOLD && mapped_x > DIRECTION_THRESHOLD) { direction_str = "Sudeste"; rotation_angle = 135; } 
    else if (mapped_y < -DIRECTION_THRESHOLD && mapped_x < -DIRECTION_THRESHOLD) { direction_str = "Sudoeste"; rotation_angle = 225; } 
    else if (mapped_y > DIRECTION_THRESHOLD) { direction_str = "Norte"; rotation_angle = 0; } 
    else if (mapped_y < -DIRECTION_THRESHOLD) { direction_str = "Sul"; rotation_angle = 180; } 
    else if (mapped_x > DIRECTION_THRESHOLD) { direction_str = "Leste"; rotation_angle = 90; } 
    else if (mapped_x < -DIRECTION_THRESHOLD) { direction_str = "Oeste"; rotation_angle = 270; }

    // Converte o ângulo para string para publicar
    char angle_str[4];
    snprintf(angle_str, sizeof(angle_str), "%d", rotation_angle);

    printf("Publicando -- Direção: %s, Ângulo: %s\n", direction_str, angle_str);

    // Publica a string da direção
    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_COMPASS_DIRECTION, direction_str, strlen(direction_str), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();
    
    sleep_ms(100);

    // Publica o ângulo
    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_COMPASS_ANGLE, angle_str, strlen(angle_str), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();
}

// --- Função Principal ---
int main() {
    stdio_init_all();
    printf("Iniciando Cliente MQTT - Rosa dos Ventos...\n");

    // Inicializa o ADC
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    // Inicializa Wi-Fi e conecta ao MQTT
    if (cyw43_arch_init()) { return -1; }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) { return -1; }
    printf("Conectado ao Wi-Fi!\n");
    
    MQTT_CLIENT_STATE_T* state = calloc(1, sizeof(MQTT_CLIENT_STATE_T));
    if (!ip4addr_aton(MQTT_SERVER, &state->remote_addr)) { return -1; }
    start_mqtt_connection(state);

    // Laço Principal
    while (true) {
        cyw43_arch_poll();
        publish_compass_data(state);
        sleep_ms(500); // Publica os dados a cada meio segundo
    }
    return 0;
}