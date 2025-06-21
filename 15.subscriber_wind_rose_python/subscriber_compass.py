import paho.mqtt.client as mqtt
from google.cloud import firestore
import datetime

# --- Configurações ---
MQTT_BROKER_HOST = "localhost"
MQTT_BROKER_PORT = 1883
MQTT_USERNAME = "SEU_USUARIO_MQTT" # Use as mesmas credenciais
MQTT_PASSWORD = "SUA_SENHA_MQTT"   # Use as mesmas credenciais

# Tópico para receber todos os dados da rosa dos ventos
MQTT_TOPIC_SUBSCRIBE = "compass/#"

# --- Configuração do Firestore ---
db = firestore.Client()

# IMPORTANTE: Usamos um novo documento para este projeto.
FIRESTORE_COLLECTION = "pico_devices"  # A mesma coleção
FIRESTORE_DOCUMENT   = "compass_01"     # Novo nome de documento!


# --- Funções Callback do MQTT ---

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Conectado ao Broker MQTT com sucesso!")
        client.subscribe(MQTT_TOPIC_SUBSCRIBE)
        print(f"Inscrito no tópico: {MQTT_TOPIC_SUBSCRIBE}")
    else:
        print(f"Falha ao conectar, código de erro: {rc}")


def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8")
    topic = msg.topic
    
    print(f"Mensagem recebida! Tópico: {topic}, Dado: {payload}")

    try:
        # Extrai o nome do dado do tópico (ex: "direction" ou "angle")
        sensor_id = topic.split('/')[-1]

        doc_ref = db.collection(FIRESTORE_COLLECTION).document(FIRESTORE_DOCUMENT)

        # Usa set(..., merge=True) para criar ou atualizar o documento
        doc_ref.set({
            sensor_id: payload,
            u'ultima_atualizacao': firestore.SERVER_TIMESTAMP
        }, merge=True)
        
        print(f"Firestore atualizado: campo '{sensor_id}' com valor '{payload}'")

    except Exception as e:
        print(f"Ocorreu um erro ao processar a mensagem ou ao atualizar o Firestore: {e}")


# --- Função Principal ---

def main():
    # Usa a Callback API v1 para evitar o DeprecationWarning
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
    
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(MQTT_BROKER_HOST, MQTT_BROKER_PORT, 60)

    print("Iniciando loop do cliente MQTT para a Rosa dos Ventos...")
    client.loop_forever()


if __name__ == '__main__':
    main()