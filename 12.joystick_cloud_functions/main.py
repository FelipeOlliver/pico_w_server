# main.py para a função get-joystick-data

import functions_framework
from google.cloud import firestore

# Inicializa o cliente do Firestore.
db = firestore.Client()

@functions_framework.http
def get_joystick_data(request):
    """
    Função HTTP que lê os dados do joystick do Firestore e os retorna como JSON.
    """
    # Headers para permitir o acesso de qualquer página web (CORS)
    headers = {
        'Access-Control-Allow-Origin': '*'
    }

    try:
        # IMPORTANTE: Apontamos para o novo documento "joystick_01"
        doc_ref = db.collection("pico_devices").document("joystick_01")

        doc = doc_ref.get()

        if doc.exists:
            data = doc.to_dict()

            # Converte o timestamp para uma string legível
            if 'ultima_atualizacao' in data and hasattr(data['ultima_atualizacao'], 'isoformat'):
                data['ultima_atualizacao'] = data['ultima_atualizacao'].isoformat()

            # Retorna os dados como JSON
            return (data, 200, headers)
        else:
            return ({'error': 'Documento do joystick não encontrado'}, 404, headers)

    except Exception as e:
        return ({'error': str(e)}, 500, headers)