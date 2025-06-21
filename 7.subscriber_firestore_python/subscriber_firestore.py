import paho.mqtt.client as mqtt
from google.cloud import firestore
import datetime

# --- Configurações ---
MQTT_BROKER_HOST = "localhost"
MQTT_BROKER_PORT = 1883
# Mesmas credenciais configuradas no seu Pico W e Mosquitto.
MQTT_USERNAME = "SEU_USUARIO_MQTT"
MQTT_PASSWORD = "SUA_SENHA_MQTT"

# Tópico genérico para receber todos os dados do Pico
# O caractere '#' é um curinga que representa todos os níveis de tópicos abaixo.
MQTT_TOPIC_SUBSCRIBE = "pico/#"

# --- Configuração do Firestore ---
# Inicializa o cliente do Firestore.
db = firestore.Client()

# O nome da "coleção" e do "documento" onde os dados serão armazenados.
FIRESTORE_COLLECTION = "pico_devices"
FIRESTORE_DOCUMENT = "pico_w_01"


# --- Funções Callback do MQTT ---

# Esta função é chamada quando o cliente se conecta ao broker.
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Conectado ao Broker MQTT com sucesso!")
        # Se a conexão for bem-sucedida, se inscreve no tópico.
        client.subscribe(MQTT_TOPIC_SUBSCRIBE)
        print(f"Inscrito no tópico: {MQTT_TOPIC_SUBSCRIBE}")
    else:
        print(f"Falha ao conectar, código de erro: {rc}")


# Esta função é chamada toda vez que uma nova mensagem é recebida do broker.
def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8")
    topic = msg.topic
    
    print(f"Mensagem recebida! Tópico: {topic}, Dado: {payload}")

    try:
        sensor_id = topic.split('/')[-1]

        doc_ref = db.collection(FIRESTORE_COLLECTION).document(FIRESTORE_DOCUMENT)

        doc_ref.set({
            sensor_id: payload,
            u'ultima_atualizacao': firestore.SERVER_TIMESTAMP
        }, merge=True)
        
        print(f"Firestore atualizado: campo '{sensor_id}' com valor '{payload}'")

    except Exception as e:
        print(f"Ocorreu um erro ao processar a mensagem ou ao atualizar o Firestore: {e}")


# --- Função Principal ---

def main():
    # Cria o cliente MQTT
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1) 
    
    # Define o nome de usuário e senha
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)

    # Associa as funções de callback
    client.on_connect = on_connect
    client.on_message = on_message

    # Conecta-se ao broker
    client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, 60)

    # Inicia o loop de rede em segundo plano.
    print("Iniciando loop do cliente MQTT...")
    client.loop_forever()


if __name__ == '__main__':
    main()