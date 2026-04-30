# Servicio web para la segunda parte
# NOTA: Hemos partido del código que se nos presentó en el Github de clase.
# https://github.com/acaldero/uc3m_sd/blob/main/materials/topic-ws/web-services.md

from flask import Flask, request

app = Flask(__name__)

@app.route('/normalizar', methods=["POST"])
def normalizar():
    try:
        # primero tomar el json de la request 
        req  = request.get_json()

        # ahora si, operamos
        # nuestro JSON: {"texto", "aqui el texto"}
        texto = req["texto"]

        # ahora toca procesar el texto

        texto_sin_espacios = texto.split() # array

        texto_bueno= " ".join(texto_sin_espacios)

        return {"texto": texto_bueno}, 200

    except Exception as e:
        return {"error": str(e)}, 415

app.run(debug=False, host="0.0.0.0", port="7777")