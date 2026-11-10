"""Descomposición de preguntas en subpreguntas (PydanticAI, MIT).

Una pregunta que arrastra varias cuestiones ("qué declara, quién coordina y qué
se registra") retrieve mal: el embedding es uno solo y los documentos que
importan para la segunda y la tercera parte nunca llegan al LLM. Aqui se parte
la pregunta, se busca cada parte por separado y se fusionan los resultados.

Solo entra el agente cuando la pregunta realmente parece multivaluada; una
pregunta simple no paga la llamada extra.
"""

import os
import re
import threading
import time

DESCOMPOSICION_ACTIVA = os.getenv("DESCOMPOSICION", "1") not in ("0", "false", "False")
MAX_SUBPREGUNTAS = 4
TIMEOUT_SEG = 8
CACHE_TTL_SEG = 900

# Pregunta simple (una sola cuestion) -> no se descompone. Se cuentan
# interrogativos y conectores; "y" solo cuenta si ya hay >=2 interrogativos.
_INTERROGATIVOS = (
    "que", "quien", "cual", "cuales", "quienes", "como", "cuando",
    "donde", "cuanto", "cuanta", "cuantas",
)
_CONECTORES = r"\b(y|ademas|tambien|asimismo|por otra parte)\b"

_cache = {}
_lock = threading.Lock()


def _sin_acentos(texto):
    import unicodedata
    t = unicodedata.normalize("NFD", texto.lower())
    return "".join(c for c in t if unicodedata.category(c) != "Mn")


def vale_la_pena(pregunta):
    """Heurística barata: ¿la pregunta trae varias cuestiones?"""
    p = _sin_acentos(pregunta or "")
    n_interrogativos = sum(
        1 for w in _INTERROGATIVOS if re.search(rf"\b{w}\b", p)
    )
    n_signos = p.count("?")
    n_palabras = len(p.split())
    tiene_conector = re.search(_CONECTORES, p) is not None
    if n_signos >= 2 or n_interrogativos >= 3:
        return True
    # "y"/"además" con dos interrogativos, o con una frase larga: suele ser
    # más de una cosa. El umbral de palabras evita partir de más.
    return tiene_conector and (n_interrogativos >= 2 or n_palabras >= 6)


def _cache_get(pregunta):
    with _lock:
        entrada = _cache.get(pregunta)
    if entrada and (time.time() - entrada[0]) < CACHE_TTL_SEG:
        return list(entrada[1])
    return None


def _cache_put(pregunta, subs):
    with _lock:
        if len(_cache) > 200:
            _cache.clear()
        _cache[pregunta] = (time.time(), list(subs))


def _agente_pydantic():
    from pydantic import BaseModel, Field
    from pydantic_ai import Agent
    from pydantic_ai.models.openai import OpenAIChatModel
    from pydantic_ai.providers.openai import OpenAIProvider
    from core.llm import _proveedor_rapido

    sel = _proveedor_rapido()
    if not sel:
        return None
    base_url, apikey, modelo = sel

    class Subpreguntas(BaseModel):
        preguntas: list[str] = Field(max_length=MAX_SUBPREGUNTAS)

    return Agent(
        OpenAIChatModel(modelo, provider=OpenAIProvider(base_url=base_url, api_key=apikey)),
        output_type=Subpreguntas,
        system_prompt=(
            "Descompón la pregunta del usuario en subpreguntas independientes, "
            "cada una respondible por separado contra un corpus legal. "
            "Si la pregunta ya es UNA sola cosa, devuelve una sola entrada con esa "
            f"misma pregunta. Máximo {MAX_SUBPREGUNTAS}. Sin numerar, sin comillas, "
            "sin explicaciones."
        ),
    )


def descomponer(pregunta, forzar=False):
    """Devuelve la lista de subpreguntas. Ante cualquier fallo devuelve
    `[pregunta]`, de modo que el chat nunca se rompe por esto."""
    if not DESCOMPOSICION_ACTIVA or not (pregunta or "").strip():
        return [pregunta]

    cacheado = _cache_get(pregunta)
    if cacheado:
        return cacheado

    if not forzar and not vale_la_pena(pregunta):
        return [pregunta]

    t0 = time.time()
    try:
        agente = _agente_pydantic()
        if agente is None:
            return [pregunta]
        # PydanticAI >=2.x: el timeout es parte de model_settings (no kwarg de
        # run_sync, que tiraba TypeError y dejaba la descomposicion muerta).
        resultado = agente.run_sync(
            f"Pregunta: {pregunta}", model_settings={"timeout": TIMEOUT_SEG}
        )
        subs = [s.strip() for s in (resultado.output.preguntas or []) if s and s.strip()]
    except Exception:
        return [pregunta]

    # Si el modelo devolvió una sola, no aporta nada: no gastamos más.
    if len(subs) <= 1:
        subs = [pregunta]
    subs = subs[:MAX_SUBPREGUNTAS]

    _cache_put(pregunta, subs)
    return subs


def limpiar_agente():
    with _lock:
        _cache.clear()
