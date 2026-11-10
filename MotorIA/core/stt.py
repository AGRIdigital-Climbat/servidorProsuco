import os
from dotenv import load_dotenv

load_dotenv()

_model = None
WHISPER_MODEL = os.getenv("WHISPER_MODEL", "tiny")

try:
    import whisper
    WHISPER_DISPONIBLE = True
except ImportError:
    WHISPER_DISPONIBLE = False
    print("WARNING: whisper no instalado. La funcion de voz no estara disponible.")


def cargar_modelo():
    global _model
    if not WHISPER_DISPONIBLE:
        raise RuntimeError("Whisper no esta instalado. Instale con: pip install openai-whisper")
    if _model is None:
        print(f"Cargando modelo Whisper {WHISPER_MODEL}...")
        _model = whisper.load_model(WHISPER_MODEL)
        print("Modelo Whisper cargado.")
    return _model


def transcribir_audio(audio_path):
    model = cargar_modelo()
    resultado = model.transcribe(audio_path, language="es")
    return resultado["text"]
