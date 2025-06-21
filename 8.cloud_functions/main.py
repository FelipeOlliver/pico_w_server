import functions_framework
from google.cloud import firestore

# Inicializa o cliente do Firestore fora da função para reutilização.
db = firestore.Client()

# O decorador @functions_framework.http indica que esta função responde a requisições HTTP.
@functions_framework.http
def get_pico_data(request):
    """
    Função HTTP que lê os dados do Firestore e os retorna como JSON.
    """
    # --- Configuração de CORS ---
    # Isso permite que o seu site (de qualquer origem) acesse esta API.
    headers = {
        'Access-Control-Allow-Origin': '*'
    }

    try:
        # Referência ao documento onde os dados do Pico estão salvos.
        doc_ref = db.collection("pico_devices").document("pico_w_01")

        doc = doc_ref.get() # Lê o documento do banco de dados.

        if doc.exists:
            data = doc.to_dict() # Converte o documento para um dicionário Python.

            if 'ultima_atualizacao' in data and hasattr(data['ultima_atualizacao'], 'isoformat'):
                data['ultima_atualizacao'] = data['ultima_atualizacao'].isoformat()

            # Retorna os dados como uma resposta JSON, com o status 200 (OK) e os headers CORS.
            return (data, 200, headers)
        else:
            # Se o documento não for encontrado, retorna um erro 404.
            return ({'error': 'Documento não encontrado'}, 404, headers)

    except Exception as e:
        # Em caso de qualquer outro erro, retorna um erro 500.
        return ({'error': str(e)}, 500, headers)