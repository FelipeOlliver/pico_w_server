#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"

// --- Configuração ---
// Estes valores serão definidos no seu arquivo CMakeLists.txt
// #define WIFI_SSID "Sua_Rede_WiFi"
// #define WIFI_PASSWORD "Sua_Senha_WiFi"
// #define MQTT_SERVER "IP_Externo_da_sua_Google_Cloud_VM"
// #define MQTT_USERNAME "Seu_Usuario_MQTT"
// #define MQTT_PASSWORD "Sua_Senha_MQTT"

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

// O tópico no qual vamos publicar
#define MQTT_TOPIC "pico/test"

typedef struct MQTT_CLIENT_STATE_T {
    ip_addr_t       remote_addr;
    mqtt_client_t* mqtt_client;
    bool            connected;
} MQTT_CLIENT_STATE_T;


// --- Callback de Conexão MQTT ---
// Esta função é chamada quando o status da conexão com o broker muda.
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

// --- Callback de Publicação MQTT ---
// Esta função é chamada após uma operação de publicação.
static void mqtt_pub_request_cb(void *arg, err_t err) {
    if (err != ERR_OK) {
        printf("Falha ao publicar no MQTT, código do erro %d\n", err);
    } else {
        // printf("Publicação MQTT bem-sucedida\n");
    }
}


// --- Lógica Principal do MQTT ---

// Função para iniciar a conexão do cliente MQTT
void start_mqtt_connection(MQTT_CLIENT_STATE_T* state) {
    struct mqtt_connect_client_info_t client_info = {0};
    
    // Gera um ID de cliente único a partir do ID da placa Pico
    char client_id[20];
    pico_get_unique_board_id_string(client_id, sizeof(client_id));
    
    client_info.client_id = client_id;
    client_info.keep_alive = 60; // Keep alive em segundos
    
    // Define o nome de usuário и a senha para autenticação
    client_info.client_user = MQTT_USERNAME;
    client_info.client_pass = MQTT_PASSWORD;

    // Aloca uma nova instância de cliente MQTT
    state->mqtt_client = mqtt_client_new();
    if (state->mqtt_client == NULL) {
        printf("Falha ao criar novo cliente mqtt\n");
        return;
    }

    // Conecta-se ao broker
    err_t err = mqtt_client_connect(
        state->mqtt_client,
        &state->remote_addr,
        MQTT_PORT, // A porta padrão não criptografada é 1883
        mqtt_connection_cb,
        state,
        &client_info
    );

    if (err != ERR_OK) {
        printf("mqtt_client_connect retornou o erro: %d\n", err);
    }
}


// --- Aplicação Principal ---
int main() {
    stdio_init_all();

    // Inicializa o Wi-Fi 
    if (cyw43_arch_init()) {
        printf("Falha ao inicializar o Wi-Fi\n");
        return -1;
    }
    cyw43_arch_enable_sta_mode();

    printf("Conectando ao Wi-Fi: %s\n", WIFI_SSID);
    // Conecta-se à sua rede Wi-Fi
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        return -1;
    }
    printf("Conectado ao Wi-Fi!\n");

    // Inicializa o estado do cliente MQTT
    MQTT_CLIENT_STATE_T* state = calloc(1, sizeof(MQTT_CLIENT_STATE_T));
    if (!state) {
        printf("Falha ao alocar estado\n");
        return -1;
    }
    
    // Converte a string do endereço IP do broker para uma estrutura ip_addr_t do lwIP
    if (!ip4addr_aton(MQTT_SERVER, &state->remote_addr)) {
        printf("Falha ao analisar o endereço IP para MQTT_SERVER\n");
        free(state);
        return -1;
    }
    
    // Conecta-se ao broker MQTT
    start_mqtt_connection(state);

    // Loop principal
    int counter = 0;
    while (true) {
        // Apenas tentamos publicar se estivermos conectados
        if (state->connected) {
            char message[30];
            snprintf(message, sizeof(message), "Olá do Pico! Contagem: %d", counter);
            
            // Publica a mensagem
            // O resultado será verificado na função mqtt_pub_request_cb
            cyw43_arch_lwip_begin();
            mqtt_publish(
                state->mqtt_client,
                MQTT_TOPIC,
                message,
                strlen(message),
                0, // QoS 0
                0, // Sem retenção (retain)
                mqtt_pub_request_cb,
                NULL
            );
            cyw43_arch_lwip_end();

            printf("Mensagem publicada: %s\n", message);
            counter++;
        }
        
        // Aguarda 5 segundos antes de tentar publicar novamente
        sleep_ms(5000);
    }

    // Desconecta e limpa (embora o loop acima seja infinito)
    mqtt_disconnect(state->mqtt_client);
    free(state);
    cyw43_arch_deinit();
    return 0;
}