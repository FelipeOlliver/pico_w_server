#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"

// Includes para o MQTT
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"

// Includes para o Joystick (ADC)
#include "hardware/adc.h"
#include <math.h> // Para a função abs()

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

#define MQTT_TOPIC_JOYSTICK_X "joystick/x"
#define MQTT_TOPIC_JOYSTICK_Y "joystick/y"

// --- Constantes de Calibração ---
const int CENTER_X = 2150;
const int CENTER_Y = 1954;
const int DEAD_ZONE = 150;

// --- Estrutura de Estado do Cliente MQTT ---
typedef struct MQTT_CLIENT_STATE_T {
    ip_addr_t      remote_addr;
    mqtt_client_t* mqtt_client;
    bool           connected;
} MQTT_CLIENT_STATE_T;

// --- Callbacks e Conexão MQTT (Reutilizados do projeto anterior) ---
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

// --- Funções Específicas do Joystick ---
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void publish_joystick_data(MQTT_CLIENT_STATE_T* state) {
    if (!state->connected) return;

    // Leitura dos valores brutos (com eixos trocados, como no seu código original)
    adc_select_input(0); // Lê o pino GP26
    uint16_t raw_y = adc_read();

    adc_select_input(1); // Lê o pino GP27
    uint16_t raw_x = adc_read();

    // Calibração dos valores
    long mapped_x, mapped_y;
    if (abs(raw_x - CENTER_X) < DEAD_ZONE) {
        mapped_x = 0;
    } else {
        mapped_x = map(raw_x, 0, 4095, -100, 100);
    }
    if (abs(raw_y - CENTER_Y) < DEAD_ZONE) {
        mapped_y = 0;
    } else {
        mapped_y = map(raw_y, 0, 4095, 100, -100); // Invertido
    }

    // Converte os valores para string para publicar
    char x_str[5];
    char y_str[5];
    snprintf(x_str, sizeof(x_str), "%ld", mapped_x);
    snprintf(y_str, sizeof(y_str), "%ld", mapped_y);

    printf("Publicando -- X: %s, Y: %s\n", x_str, y_str);

    // Publica o valor de X
    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_JOYSTICK_X, x_str, strlen(x_str), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();
    
    sleep_ms(100);

    // Publica o valor de Y
    cyw43_arch_lwip_begin();
    mqtt_publish(state->mqtt_client, MQTT_TOPIC_JOYSTICK_Y, y_str, strlen(y_str), 1, 0, mqtt_pub_request_cb, NULL);
    cyw43_arch_lwip_end();
}

// --- Função Principal ---
int main() {
    stdio_init_all();
    printf("Iniciando Cliente MQTT do Joystick...\n");

    // Inicializa o ADC para os pinos do Joystick
    adc_init();
    adc_gpio_init(VRX_PIN);
    adc_gpio_init(VRY_PIN);

    // Inicializa Wi-Fi
    if (cyw43_arch_init()) { return -1; }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) { return -1; }
    printf("Conectado ao Wi-Fi!\n");
    
    // Conecta ao MQTT
    MQTT_CLIENT_STATE_T* state = calloc(1, sizeof(MQTT_CLIENT_STATE_T));
    if (!ip4addr_aton(MQTT_SERVER, &state->remote_addr)) { return -1; }
    start_mqtt_connection(state);

    // Laço Principal
    while (true) {
        cyw43_arch_poll();
        publish_joystick_data(state);
        sleep_ms(500); // Publica os dados a cada meio segundo
    }
    return 0;
}
