#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "oled_driver.h" 
#include "secrets.h"     
#include <string.h>
#include <stdlib.h>

// Tópico MQTT para se inscrever
#define MQTT_TOPIC_SUB "bitdoglab/led"

// Pinos dos LEDs
#define LED_RED   13
#define LED_GREEN 11
#define LED_BLUE  12

// Estrutura para manter o estado do cliente MQTT
typedef struct MQTT_CLIENT_STATE_T {
    ip_addr_t remote_addr;
    mqtt_client_t* mqtt_client;
    bool connected;
} MQTT_CLIENT_STATE_T;

MQTT_CLIENT_STATE_T* mqtt_state = NULL;

// Buffers para armazenar o texto a ser exibido no OLED
char oled_line1[20] = "Inicializando...";
char oled_line2[20] = "";
char oled_line3[20] = "";
char oled_line4[20] = "";

// Função para atualizar a tela do OLED com o conteúdo dos buffers
void update_display() {
    oled_clear_buffer();
    oled_write_string(0, 0, oled_line1);
    oled_write_string(0, 16, oled_line2);
    oled_write_string(0, 32, oled_line3);
    oled_write_string(0, 48, oled_line4);
    oled_render();
}

// --- Callbacks MQTT ---

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    printf("Msg no topico: %s\n", topic);
}

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char payload[64];
    snprintf(payload, sizeof(payload), "%.*s", len, data);
    printf("Payload: %s\n", payload);

    // Atualiza a linha 4 do display com a mensagem recebida
    snprintf(oled_line4, sizeof(oled_line4), "Rec: %s", payload);
    update_display();

    // Controla os LEDs
    if (strstr(payload, "RED ON")) gpio_put(LED_RED, 1);
    if (strstr(payload, "RED OFF")) gpio_put(LED_RED, 0);
    if (strstr(payload, "GREEN ON")) gpio_put(LED_GREEN, 1);
    if (strstr(payload, "GREEN OFF")) gpio_put(LED_GREEN, 0);
    if (strstr(payload, "BLUE ON")) gpio_put(LED_BLUE, 1);
    if (strstr(payload, "BLUE OFF")) gpio_put(LED_BLUE, 0);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("Conectado ao broker MQTT!\n");
        snprintf(oled_line3, sizeof(oled_line3), "MQTT: Conectado");
        update_display();
        
        mqtt_state->connected = true;
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);
        mqtt_subscribe(client, MQTT_TOPIC_SUB, 1, NULL, NULL);
    } else {
        printf("Erro na conexao MQTT: %d\n", status);
        snprintf(oled_line3, sizeof(oled_line3), "MQTT Falha: %d", status);
        update_display();
    }
}

void start_mqtt_connection(MQTT_CLIENT_STATE_T* state) {
    struct mqtt_connect_client_info_t ci = {0};
    ci.client_id = "bitdoglab-pico-w-oled";
    ci.keep_alive = 60;

    state->mqtt_client = mqtt_client_new();
    mqtt_client_connect(state->mqtt_client, &state->remote_addr, MQTT_PORT, mqtt_connection_cb, NULL, &ci);
}

static void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
    if (ipaddr) {
        mqtt_state->remote_addr = *ipaddr;
        start_mqtt_connection(mqtt_state);
    } else {
        printf("Erro de DNS\n");
        snprintf(oled_line2, sizeof(oled_line2), "WIFI: Erro DNS");
        update_display();
    }
}

int main() {
    stdio_init_all();

    // Inicializa os LEDs
    gpio_init(LED_RED);   gpio_set_dir(LED_RED, GPIO_OUT);
    gpio_init(LED_GREEN); gpio_set_dir(LED_GREEN, GPIO_OUT);
    gpio_init(LED_BLUE);  gpio_set_dir(LED_BLUE, GPIO_OUT);

    // Inicializa o display OLED
    oled_init();
    update_display(); 

    // Inicializa Wi-Fi
    if (cyw43_arch_init()) {
        printf("Erro ao iniciar Wi-Fi\n");
        snprintf(oled_line1, sizeof(oled_line1), "Erro Wi-Fi Chip");
        update_display();
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    snprintf(oled_line1, sizeof(oled_line1), "Conectando...");
    update_display();
    
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha na conexao Wi-Fi\n");
        snprintf(oled_line1, sizeof(oled_line1), "Wi-Fi Falhou");
        update_display();
        return -1;
    }
    printf("Wi-Fi conectado!\n");
    snprintf(oled_line1, sizeof(oled_line1), "Status:");
    snprintf(oled_line2, sizeof(oled_line2), "WIFI: OK");
    update_display();

    // Aloca memória para o estado MQTT
    mqtt_state = calloc(1, sizeof(MQTT_CLIENT_STATE_T));
    if (!mqtt_state) {
        return -1;
    }

    // Resolve DNS do broker
    snprintf(oled_line3, sizeof(oled_line3), "MQTT: Resolv DNS");
    update_display();
    err_t err = dns_gethostbyname(MQTT_SERVER, &mqtt_state->remote_addr, dns_found_cb, NULL);
    if (err == ERR_OK) {
        start_mqtt_connection(mqtt_state);
    } else if (err != ERR_INPROGRESS) {
        printf("Erro imediato no DNS\n");
    }

    // Loop principal
    while (true) {
        cyw43_arch_poll();
        sleep_ms(100);
    }
}
