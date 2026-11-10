import unicodedata
import re

from sentence_transformers import SentenceTransformer
import numpy as np

_model = None


def _normalizar_texto(texto):
    """Normaliza texto para embeddings: quita tildes, minúsculas, espacios extra."""
    if not texto:
        return ""
    # Quitar tildes/diacríticos (NFD → eliminar marcas → recomponer)
    texto = unicodedata.normalize("NFD", texto)
    texto = "".join(c for c in texto if unicodedata.category(c) != "Mn")
    texto = unicodedata.normalize("NFC", texto)
    # Minúsculas
    texto = texto.lower()
    # Colapsar espacios múltiples
    texto = re.sub(r"\s+", " ", texto).strip()
    return texto


def cargar_modelo():
    global _model
    if _model is None:
        print("Cargando modelo de embeddings all-MiniLM-L6-v2...")
        _model = SentenceTransformer("sentence-transformers/all-MiniLM-L6-v2")
        print("Modelo de embeddings cargado.")
    return _model


def liberar_modelo():
    global _model
    if _model is not None:
        del _model
        _model = None
        print("Modelo de embeddings liberado de memoria.")


def generar_embedding(texto):
    model = cargar_modelo()
    texto_norm = _normalizar_texto(texto)
    embedding = model.encode(texto_norm, normalize_embeddings=True)
    return embedding.tolist()


def generar_embeddings_lote(textos):
    model = cargar_modelo()
    textos_norm = [_normalizar_texto(t) for t in textos]
    embeddings = model.encode(textos_norm, normalize_embeddings=True, show_progress_bar=False)
    return [e.tolist() for e in embeddings]
