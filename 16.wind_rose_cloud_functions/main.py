# main.py para a função get-compass-data

import functions_framework
from google.cloud import firestore

db = firestore.Client()

@functions_framework.http
def get_compass_data(request):
    """
    Função HTTP que lê os dados da rosa dos ventos e os retorna como JSON.
    """
    headers = {
        'Access-Control-Allow-Origin': '*'
    }

    try:
        # Aponta para o novo documento "compass_01"
        doc_ref = db.collection("pico_devices").document("compass_01")

        doc = doc_ref.get()

        if doc.exists:
            data = doc.to_dict()

            # Formata o timestamp
            if 'ultima_atualizacao' in data and hasattr(data['ultima_atualizacao'], 'isoformat'):
                data['ultima_atualizacao'] = data['ultima_atualizacao'].isoformat()

            return (data, 200, headers)
        else:
            return ({'error': 'Documento da rosa dos ventos não encontrado'}, 404, headers)

    except Exception as e:
        return ({'error': str(e)}, 500, headers)