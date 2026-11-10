from llama_cpp import Llama
import os
import random
import re
import time
from dotenv import load_dotenv

load_dotenv()

_llm = None
MODEL_PATH = os.getenv("MODEL_PATH", "modelos/qwen.gguf")

LLM_MODE = os.getenv("LLM_MODE", "local")  # local | external
_set_modo_global = LLM_MODE
LLM_API_BASE = os.getenv("LLM_API_BASE", "https://api.groq.com/openai/v1")
LLM_MODEL_FALLBACK = os.getenv("LLM_MODEL", "openai/gpt-oss-20b")

# Presupuesto de contexto: el modelo local no puede pedir mas que su ventana,
# el externo si. CTX_MAX_LOCAL = n_ctx del Qwen2.5-3B (llama.cpp, ver
# cargar_modelo): es un limite duro, no una preferencia. CTX_MAX_EXTERNO es
# configurable porque los proveedores sanos (gpt-oss-20b) tienen ~131k de
# ventana y 2048 desperdiciaba >95% de ella.
CTX_MAX_LOCAL = int(os.getenv("LLM_CTX_LOCAL", "2048"))
CTX_MAX_EXTERNO = int(os.getenv("LLM_CTX_EXTERNO", "8192"))
MAX_TOKENS_RESPUESTA = int(os.getenv("LLM_MAX_TOKENS", "512"))
RAZONAMIENTO_MAX = int(os.getenv("LLM_RAZONAMIENTO_TOKENS", "2048"))
CHARS_POR_TOKEN = 4
SAFETY_MARGIN = 96
# Un chunk que rompa este tope (texto corrupto/pegado) no entra al prompt: se
# salta y se sigue con el siguiente en vez de rendirse.
MAX_CHUNK_CHARS_PROMPT = 6000
SYSTEM_PROMPT = (
    "Eres un asistente jurídico experto en leyes y normativas de Bolivia. "
    "Responde de forma precisa y clara usando exclusivamente la información "
    "proporcionada en el contexto. Si la información no está en el contexto, "
    "indica que no encontraste datos relevantes. "
    "Cita las fuentes usando el formato [1], [2], etc. "
    "Escribe siempre en español correcto, con tildes, ñ y puntuación adecuados. "
    "Responde exclusivamente en texto plano: sin Markdown, sin negritas ni "
    "asteriscos, sin viñetas, sin listas numeradas ni con guiones, sin "
    "encabezados, sin tablas y sin ningún otro formato. Redacta todo como "
    "prosa, en párrafos completos y bien estructurados. "
    "Responde directamente con la respuesta final, sin mostrar procesos de "
    "razonamiento, sin resumir las instrucciones recibidas y sin incluir ningún "
    "texto de 'pensamiento'. "
    "Corrige siempre fallas ortográficas o de sentido en la pregunta del usuario "
    "o en la extracción de información del contexto."
)
IM_END = "im_end"
IM_START = "im_start"

GUIAS_RESPUESTA = {
    "definicion": (
        "El usuario busca la definición de un término. Si el contexto contiene "
        "una entrada de glosario o definición literal, úsala como base. "
        "Explica el término de forma clara en párrafos de prosa y cita el artículo "
        "o parte de la norma que la define."
    ),
    "procedimiento": (
        "El usuario pregunta por un procedimiento o proceso. Explica los pasos "
        "en orden cronológico en un texto continuo de prosa, indicando quién los "
        "realiza y en qué artículo de la norma se establece cada paso, sin usar "
        "listas ni viñetas."
    ),
    "autoridad": (
        "El usuario pregunta por la autoridad responsable. Identifica qué entidad, "
        "ministerio, comité o instancia tiene la competencia y cita la norma y "
        "el artículo que lo establece, en párrafos de prosa."
    ),
    "requisito": (
        "El usuario pregunta por requisitos o condiciones. Describe cada requisito "
        "de forma clara y ordenada en un texto continuo de prosa, citando el "
        "artículo o parte de la norma correspondiente, sin usar listas ni viñetas."
    ),
    "plazo": (
        "El usuario pregunta por plazos o períodos de vigencia. Indica los plazos, "
        "fechas y la duración exacta de la vigencia en párrafos de prosa, citando "
        "el artículo que lo establece."
    ),
    "general": (
        "Responde de forma precisa y clara usando exclusivamente la información "
        "del contexto y citando las fuentes correspondientes. Usa tildes, ñ y "
        "puntuación correctos. Redacta en párrafos de prosa, sin listas ni viñetas."
    ),
}


def _guia_para(intencion):
    return GUIAS_RESPUESTA.get(intencion, GUIAS_RESPUESTA["general"])


def _estimar_tokens(texto):
    chars = texto if isinstance(texto, int) else len(texto)
    return max(1, chars // CHARS_POR_TOKEN)


def _limpiar_formato(texto, estado, flush=False):
    """Limpiador best-effort de markdown/viñetas para streaming por tokens.

    Aplica limpieza a cada token recibido, manteniendo en `estado` (un dict):
      - `cola`: fragmento final diferido porque podría ser el arranque de una
        marca partida entre tokens (negritas, backtick, hash o viñeta numerada).
      - `linea_inicial`: si el byte 0 del buffer cae en un inicio de línea
        real del stream (no simplemente en el byte 0 del fragmento).

    `flush=True` emite la cola pendiente sin diferirla (cierre del stream).

    Quita negritas, asteriscos, backticks, marcadores de encabezado, viñetas y
    listas numeradas al inicio de línea, y compacta saltos de línea.
    """
    texto = estado.get("cola", "") + texto
    texto = texto.replace("**", "").replace("`", "")
    texto = texto.replace("*", "")

    # `(?m)^` coincide también con el byte 0 del buffer, pero ese byte NO es
    # necesariamente inicio de línea en el stream: sin este escudo borraba el
    # "9)" de "nueve (9) meses" y dejaba "nueve (meses".
    escudo = "" if estado.get("linea_inicial", True) else "\x00"
    trabajo = escudo + texto
    antes_num = trabajo
    trabajo = re.sub(r"(?m)^\s*#{1,6}\s*", "", trabajo)
    trabajo = re.sub(r"(?m)^\s*[-*•·]\s*", "", trabajo)
    trabajo = re.sub(r"(?m)^\s*\d+[.)]\s*", "", trabajo)
    marco = trabajo != antes_num
    if escudo and trabajo[:1] == escudo:
        trabajo = trabajo[1:]
    if marco and not trabajo:
        # El marcador quedo a medias en el buffer (p.ej. "1." sin su espacio):
        # el proximo token puede traer ese espacio y hay que quitarlo. Solo
        # en ese caso; si ya vino contenido, el espacio siguiente es textual
        # ("Primer paso" + " y luego" debe salir con su espacio).
        estado["espacio_pendiente"] = True
    elif estado.pop("espacio_pendiente", False) and trabajo.startswith((" ", "\t")):
        trabajo = trabajo[1:]
    texto = re.sub(r"\n{2,}", "\n", trabajo)

    cola = ""
    resto = texto
    if not flush:
        if resto.endswith(("*", "`", "#")):
            cola = resto[-1]
            resto = resto[:-1]
        else:
            m = re.search(r"(?m)(?:^|\n)((?:\d+)|(?:[-•·]))$", resto)
            if m:
                cola = m.group(1)
                resto = resto[: m.start(1)]
    estado["cola"] = cola
    if resto:
        estado["linea_inicial"] = resto.endswith("\n")
    return resto


def _instruccion_presupuesto(max_tokens):
    if not max_tokens:
        return ""
    return (
        "\nTu presupuesto de tokens es de máximo "
        f"{int(max_tokens)}, pero ese es un LÍMITE MÁXIMO, no una meta: responde "
        "de forma breve, concisa y directa (idealmente 80-250 tokens), sin divagar "
        "ni repetir el contexto. Concluye la idea completamente y no dejes frases "
        "a medias."
        "\nNo agregues una sección final de 'Referencias' ni vuelvas a enumerar las "
        "fuentes: basta con mencionar el nombre del documento cuando sea necesario. "
        "Corrige siempre la ortografía: tildes, ñ y signos adecuados."
        "\nEscribe en párrafos de prosa, sin viñetas, sin listas y sin formato Markdown."
    )


def _build_contexto_texto(contexto_chunks):
    contexto_texto = ""
    for i, chunk in enumerate(contexto_chunks, 1):
        titulo = chunk.get("titulo", "Sin titulo")
        contenido = chunk.get("contenido", "")
        tipo = chunk.get("tipo_nombre", "")
        fuente = f"[{i}] {tipo} - {titulo}"
        contexto_texto += f"\n{fuente}\n{contenido}\n"
    return contexto_texto


def _build_prompt(pregunta, contexto_chunks, guia=None, max_tokens=None):
    contexto_texto = _build_contexto_texto(contexto_chunks)
    user_prompt = (
        f"CONTEXTO DE LEYES Y NORMATIVAS:\n{contexto_texto}\n\n"
        f"PREGUNTA: {pregunta}\n\n"
    )
    if guia:
        user_prompt += f"GUIA DE RESPUESTA: {guia}\n\n"
    user_prompt += "RESPUESTA (en español, en párrafos, sin viñetas ni markdown):"
    system_prompt = SYSTEM_PROMPT + _instruccion_presupuesto(max_tokens)
    return (
        f"<|{IM_START}|>system\n{system_prompt}<|{IM_END}|>\n\n"
        f"<|{IM_START}|>user\n{user_prompt}<|{IM_END}|>\n\n"
        f"<|{IM_START}|>assistant\n"
    )


def _presupuesto_contexto(max_tokens, llm_externo=None):
    """Tokens disponibles para el contexto, segun el modelo que va a generar.

    Usa la MISMA decision que el generador (`usar_modelo_externo`): si no, el
    presupuesto y el modelo no coincidirian.

    - local: ventana dura de llama.cpp (n_ctx), sin margen de razonamiento.
    - externo: `CTX_MAX_EXTERNO` y ademas se reserva `RAZONAMIENTO_MAX`,
      porque en un modelo razonador esos tokens salen de la misma ventana que
      el prompt y la salida visible (`_max_tokens_api` los suma al tope).
    """
    externo = usar_modelo_externo(llm_externo)
    ctx_max = CTX_MAX_EXTERNO if externo else CTX_MAX_LOCAL
    margen_raz = RAZONAMIENTO_MAX if externo else 0
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    return max(256, ctx_max - int(max_tokens) - margen_raz - SAFETY_MARGIN)


def _ordenar_con_cuota(chunks):
    """Round-robin por subpregunta: cada parte garanticiza un slot en el prompt.

    `buscar_hibrido_inteligente` anota cada chunk con `_sub` (indice de la
    subpregunta que lo saco con mejor score). Sin cuota, los chunks de las
    partes que se parecen mas a la pregunta completa (score mas alto) comen
    todos los slots y el dato de la ultima parte nunca llega: era exactamente
    el fallo del 0.15% del FORADE (posicion 13 de 45, presupuesto de 5).
    """
    grupos = {}
    for c in chunks:
        grupos.setdefault(c.get("_sub"), []).append(c)
    if len(grupos) <= 1:
        return list(chunks)  # pregunta no descompuesta: ya viene por score

    ordenados = []
    iteradores = [iter(g) for g in grupos.values()]
    while iteradores:
        vivos = []
        for it in iteradores:
            c = next(it, None)
            if c is None:
                continue
            ordenados.append(c)
            vivos.append(it)
        iteradores = vivos
    return ordenados


def seleccionar_chunks_contexto(chunks, pregunta, guia=None, max_tokens=None, llm_externo=None):
    """Arma el contexto que va a leer el LLM sin pasar el presupuesto de ventana.

    `max_tokens=None` y `llm_externo=None` resuelven igual que en
    `generar_respuesta_stream`: lo que reserve aqui es exactamente lo que
    despues va a consumir el generador.
    """
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    limite_prompt = _presupuesto_contexto(max_tokens, llm_externo)
    if not chunks:
        return []

    base = _estimar_tokens(_build_prompt(pregunta, [], guia, max_tokens))
    seleccionados = []
    tokens = base

    for chunk in _ordenar_con_cuota(chunks):
        contenido = chunk.get("contenido", "") or ""
        if len(contenido) > MAX_CHUNK_CHARS_PROMPT:
            continue  # texto corrupto/gigante: no puede comerse todo el presupuesto
        extra = _estimar_tokens(
            f"\n[{len(seleccionados) + 1}] {chunk.get('tipo_nombre', '')} "
            f"- {chunk.get('titulo', '')}\n{contenido}\n"
        )
        if tokens + extra > limite_prompt:
            continue  # antes era `break`: un chunk grande mataba a todos los demas
        seleccionados.append(chunk)
        tokens += extra

    return seleccionados or chunks[:1]


def construir_prompt_adaptativo(pregunta, contexto_chunks, intencion):
    return _build_prompt(pregunta, contexto_chunks, _guia_para(intencion))


def cargar_modelo():
    global _llm
    if _llm is None:
        print("Cargando modelo Qwen2.5 3B...")
        _llm = Llama(
            model_path=MODEL_PATH,
            n_ctx=2048,
            n_threads=8,
            n_batch=512,
            use_mmap=True,
            use_mlock=False,
            verbose=False,
        )
        print("Modelo Qwen2.5 3B cargado.")
    return _llm


def _build_messages(pregunta, contexto_chunks, guia=None, max_tokens=None):
    contexto_texto = _build_contexto_texto(contexto_chunks)
    user_prompt = (
        f"CONTEXTO DE LEYES Y NORMATIVAS:\n{contexto_texto}\n\n"
        f"PREGUNTA: {pregunta}\n\n"
    )
    if guia:
        user_prompt += f"GUIA DE RESPUESTA: {guia}\n\n"
    user_prompt += "RESPUESTA (en español, en párrafos, sin viñetas ni markdown):"
    return [
        {"role": "system", "content": SYSTEM_PROMPT + _instruccion_presupuesto(max_tokens)},
        {"role": "user", "content": user_prompt},
    ]


# --- Selección de proveedor externo (aleatoria + con retry) ---


_COOLDOWN_FALLO_SEG = 15 * 60
_fallos_recientes = {}


def _proveedores_habilitados():
    """Proveedores habilitados para el sorteo aleatorio.

    Incluye los que tengan `estado_manual=true` y no esten en cooldown por un
    fallo reciente (15 min): respeta `estado_modelo` de la BD y ademas saca del
    sorteo a los que acaban de fallar, para que un proveedor averiado no se
    siga eligiendo primero en cada peticion (sesgaba la seleccion hacia el
    "superviviente" mas fiable). Vencido el cooldown vuelven a participar
    como sonda; si fallan otra vez se re-marcan.
    """
    try:
        from db.queries import obtener_proveedores_internos
        from core.secrets import decrypt_secret
        rows = obtener_proveedores_internos()
        ahora = time.time()
        pool = []
        for r in rows:
            if not (r.get("estado_manual") and r.get("apikey") and r.get("estado_modelo")):
                continue
            fk = _fallos_recientes.get(r["id"])
            en_cooldown = fk is not None and (ahora - fk) < _COOLDOWN_FALLO_SEG
            if en_cooldown:
                continue
            try:
                apikey = decrypt_secret(r["apikey"])
            except Exception:
                continue
            pool.append({
                "id": r["id"],
                "nombre": r["nombre"],
                "base_url": r["base_url"],
                "apikey": apikey,
                "modelo_base": r["modelo_base"],
                "razonamiento_tokens": int(r.get("razonamiento_tokens") or 0),
                "latencia_prom_ms": r.get("latencia_prom_ms"),
            })
        if len(pool) <= 5:
            pass  # debug: pool size small
        return pool
    except Exception:
        return []


def _ordenar_por_velocidad(pool):
    """Ordena el pool de mas rapido a mas lento.

    El sorteo pondera por latencia medida (`latencia_prom_ms`, EMA): un
    proveedor 10x mas lento entra mucho menos, pero sigue disponible si es el
    unico que responde. Sin latencia conocida, todos pesan igual.

    Se ordena ademas por velocidad para que el recorrido de reintentos pruebe
    primero a los rapidos (el `max_intentos` corta tras 3).
    """
    import random as _random

    def peso(p):
        lat = p.get("latencia_prom_ms")
        if not lat or lat <= 0:
            return 1000.0
        # 500 ms -> ~2.0 ; 5000 ms -> ~0.2 ; 20000 ms -> ~0.05
        return 1.0 / (1.0 + (float(lat) / 1000.0))

    ordenados = sorted(pool, key=lambda p: (p.get("latencia_prom_ms") or 10**9))
    pesos = [peso(p) for p in ordenados]
    suma = sum(pesos)
    if suma <= 0:
        return ordenados
    acumulado = 0.0
    for p, w in zip(ordenados, pesos):
        acumulado += w / suma
        p["_acumulado"] = acumulado
    ordenados.append({"_acumulado": 1.0})  # centinela
    return ordenados


def _sortear_ponderado(pool):
    """Sorteo ponderado por latencia. Mismo contrato: devuelve registros de proveedor."""
    import random as _random

    ordenados = _ordenar_por_velocidad(pool)
    if not ordenados:
        return None
    if ordenados[-1].get("id") is None:
        ordenados = ordenados[:-1]  # quitar centinela
        if not ordenados:
            return None
    objetivo = _random.random()
    for p in ordenados:
        if objetivo <= p.get("_acumulado", 1.0):
            return p
    return ordenados[-1]


def _registrar_latencia(proveedor_id, segundos):
    """Manda la latencia observada a la BD (best-effort, nunca rompe el chat)."""
    if not proveedor_id or not segundos or segundos <= 0:
        return
    try:
        from db.queries import registrar_latencia_proveedor
        registrar_latencia_proveedor(proveedor_id, int(segundos * 1000))
    except Exception:
        pass


def _proveedor_rapido():
    """Proveedor sano con menos margen de razonamiento (el más veloz).

    Para tareas auxiliary cortas (descomponer una pregunta, clasificar), donde
    la latencia importa más que la profundidad del razonamiento.
    """
    pool = _proveedores_habilitados()
    if not pool:
        return None
    r = min(pool, key=lambda x: int(x.get("razonamiento_tokens") or 0))
    return r["base_url"], r["apikey"], r["modelo_base"]


def _seleccionar_proveedor():
    """Selecciona un proveedor al azar del pool habilitado.

    Devuelve (OpenAI client, modelo_base, proveedor_id, nombre) o None si no hay pool.
    """
    pool = _proveedores_habilitados()
    if not pool:
        return None
    random.shuffle(pool)
    r = pool[0]
    from openai import OpenAI
    client = OpenAI(api_key=r["apikey"], base_url=r["base_url"])
    return client, r["modelo_base"], r["id"], r["nombre"]


def _marcar_estado(proveedor_id, ok: bool):
    """Actualiza estado_modelo y ultima_verificacion en la BD.

    Ademas lleva el instante del ultimo fallo en memoria para el cooldown:
    un proveedor que falla queda fuera del sorteo 15 minutos; los exitos
    lo reincorporan de inmediato.
    """
    if ok:
        _fallos_recientes.pop(proveedor_id, None)
    else:
        _fallos_recientes[proveedor_id] = time.time()
    try:
        from db.queries import actualizar_estado_modelo_proveedor
        actualizar_estado_modelo_proveedor(proveedor_id, ok)
    except Exception:
        pass


def _hay_proveedores_registrados() -> bool:
    try:
        from db.queries import obtener_proveedores_internos
        return len(obtener_proveedores_internos()) > 0
    except Exception:
        return False


def usar_modelo_externo(llm_externo=None):
    if llm_externo is not None:
        return bool(llm_externo)
    return get_modo_global() == "external"


def get_modo_global():
    try:
        ruta = os.path.join(os.path.dirname(os.path.dirname(__file__)), ".env")
        with open(ruta, "r", encoding="utf-8") as f:
            for linea in f:
                if linea.strip().startswith("LLM_MODE="):
                    valor = linea.strip().split("=", 1)[1].strip()
                    return "external" if valor == "external" else "local"
    except Exception:
        pass
    return _set_modo_global


def _persistir_modo_env(modo):
    try:
        ruta = os.path.join(os.path.dirname(os.path.dirname(__file__)), ".env")
        with open(ruta, "r", encoding="utf-8") as f:
            lineas = f.readlines()
        encontrado = False
        for i, linea in enumerate(lineas):
            if linea.strip().startswith("LLM_MODE="):
                lineas[i] = f"LLM_MODE={modo}\n"
                encontrado = True
                break
        if not encontrado:
            lineas.append(f"LLM_MODE={modo}\n")
        with open(ruta, "w", encoding="utf-8") as f:
            f.writelines(lineas)
    except Exception:
        pass


def set_modo_global(modo, persistir=True):
    global _set_modo_global
    modo = "external" if str(modo).strip().lower() == "external" else "local"
    _set_modo_global = modo
    if persistir:
        _persistir_modo_env(modo)
    return modo


# --- Generación con modelo externo (selección aleatoria + retry) ---

_soporta_reasoning_effort = {}
_soporta_response_format = {}
_CALIBRAR_PASO = 64


class _RespuestaVaciaError(Exception):
    """El proveedor respondio pero sin texto visible (razonamiento le consumio el presupuesto)."""


class ContextoDemasiadoError(ValueError):
    """El prompt no entra en la ventana del modelo.

    El puente lo mapea a `contexto_demasiado` (413), no a error generico: el
    cliente tiene que acortar el historial, no reintentar igual.
    """


def _max_tokens_api(max_tokens, razonamiento_tokens):
    """Tope interno de la API: salida visible + margen de razonamiento calibrado."""
    return max_tokens + min(max(0, int(razonamiento_tokens or 0)), RAZONAMIENTO_MAX)


def _llamar_externo(client, modelo, messages, max_tokens, stream=False, razonamiento_tokens=0, timeout=30,
                    temperatura=0.1, razonamiento="low", formato_json=False, stop=None):
    """Llama a una API OpenAI-compatible.

    `max_tokens` es el tope de la respuesta de SALIDA (visible). Para modelos que
    razonan, se amplia el tope interno con un margen calibrado por uso real, de
    modo que el razonamiento no recorte la respuesta visible.

    Se pide `reasoning_effort` a todos los proveedores que lo acepten (el
    resultado se cac de memoria para no reenviar el parametro a los que lo
    rechazan con 400). `razonamiento="low"` mantiene el comportamiento de siempre
    (camino juridico); `razonamiento="off"` NO manda el parametro.

    `formato_json` agrega `response_format: {"type":"json_object"}`. Los
    proveedores que lo rechazan con 400 se cachean en
    `_soporta_response_format` y se reintenta sin el, que es lo que permite
    degradar en vez de fallar cuando el modelo no sabe dar JSON.

    `timeout` (segundos) evita que un proveedor lento/cuelgue bloquee la
    peticion completa. Si se excede, se lanza excepcion que el bucle de
    proveedores captura para probar el siguiente.

    `temperatura` es 0.1 en todo el camino juridico (respuestas deterministas);
    el puente lo sube a 0.7 para poder usarla como LLM de proposito general.
    """
    api_max = _max_tokens_api(max_tokens, razonamiento_tokens)
    kwargs = dict(
        model=modelo,
        messages=messages,
        max_tokens=api_max,
        temperature=temperatura,
        timeout=timeout,
    )
    if stop:
        kwargs["stop"] = list(stop)
    if stream:
        kwargs["stream_options"] = {"include_usage": True}
    key = (str(getattr(client, "base_url", "")).rstrip("/"), modelo)
    if razonamiento and razonamiento != "off" and _soporta_reasoning_effort.get(key, True):
        kwargs["extra_body"] = {"reasoning_effort": razonamiento}
    if formato_json and _soporta_response_format.get(key, True):
        kwargs["response_format"] = {"type": "json_object"}

    intentos = 0
    while True:
        intentos += 1
        try:
            create = client.chat.completions.create
            return create(stream=True, **kwargs) if stream else create(**kwargs)
        except Exception as e:
            # Los proveedores que no conocen un parametro contestan 400. Se apaga
            # el parametro, se memoriza y se reintenta una vez; en el camino del
            # puente (formato=json) queda registrado como `json_no_soportado`.
            if getattr(e, "status_code", None) != 400 or intentos > 2:
                raise
            if "extra_body" in kwargs and _soporta_reasoning_effort.get(key, True):
                _soporta_reasoning_effort[key] = False
                kwargs.pop("extra_body")
                continue
            if "response_format" in kwargs and _soporta_response_format.get(key, True):
                _soporta_response_format[key] = False
                kwargs.pop("response_format")
                continue
            raise


def json_no_soportado(base_url, modelo):
    """True si este proveedor ya rechazó `response_format` en una llamada previa.

    El puente lo usa para avisar `json_no_soportado` y degradar (§10) en vez de
    informar un error genérico.
    """
    return _soporta_response_format.get((str(base_url or "").rstrip("/"), modelo)) is False


def _estimar_uso_fallback(messages, texto_total):
    """Estima tokens cuando el proveedor no devuelve usage (stream)."""
    prompt_chars = sum(len(m.get("content", "") or "") for m in messages)
    return _estimar_tokens(prompt_chars), _estimar_tokens(texto_total)


def _calibrar_razonamiento(proveedor_id, prev, tokens_completados, texto_visible, max_tokens, truncado):
    """Ajusta el margen de razonamiento de un proveedor tras una llamada util.

    Reglas (robusteces ante cortes y respuestas vacias):
    - Respuesta vacia: el razonamiento se comio el presupuesto -> sube un paso
      (excluyendo al maximo `RAZONAMIENTO_MAX`).
    - Recorte por tope (`truncado`) con salida util que NO alcanzo `max_tokens`:
      sube un paso para dar margen a la proxima llamada. Si ya alcanzo el tope
      de salida, no cambia (el modelo solo queria escribir mas).
    - Fin natural: EMA hacia el overhead real (completion - visible), de modo
      que el margen baja cuando el modelo razona menos (gasto minimo).
    """
    texto_visible = (texto_visible or "").strip()
    visible = _estimar_tokens(texto_visible)
    completados = max(0, int(tokens_completados or 0))
    prev = max(0, int(prev or 0))
    if not texto_visible:
        nuevo = min(prev + _CALIBRAR_PASO, RAZONAMIENTO_MAX)
    elif truncado:
        if visible < max(1, int(max_tokens or MAX_TOKENS_RESPUESTA)):
            nuevo = min(prev + _CALIBRAR_PASO, RAZONAMIENTO_MAX)
        else:
            nuevo = prev
    else:
        overhead = min(max(0, completados - visible), RAZONAMIENTO_MAX)
        nuevo = int(round(0.7 * prev + 0.3 * overhead))
    try:
        from db.queries import actualizar_razonamiento_tokens_proveedor
        actualizar_razonamiento_tokens_proveedor(proveedor_id, nuevo)
    except Exception:
        pass
    return nuevo


def generar_respuesta_externo(pregunta, contexto_chunks, guia=None, max_tokens=None):
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    messages = _build_messages(pregunta, contexto_chunks, guia, max_tokens)

    if not _hay_proveedores_registrados():
        raise ValueError(
            "No hay proveedores externos en la BD. "
            "Crea uno en /llm/proveedores."
        )

    pool = _proveedores_habilitados()
    if not pool:
        raise ValueError(
            "Todos los proveedores están deshabilitados o fallaron la verificación. "
            "Activa alguno en /llm/proveedores o ejecuta verificar_proveedores.py."
        )

    import random
    random.shuffle(pool)
    last_error = None

    for prov in pool:
        from openai import OpenAI
        client = OpenAI(api_key=prov["apikey"], base_url=prov["base_url"])
        try:
            resultado = _llamar_externo(
                client, prov["modelo_base"], messages, max_tokens,
                razonamiento_tokens=prov["razonamiento_tokens"],
            )
            _marcar_estado(prov["id"], True)
            respuesta = (resultado.choices[0].message.content or "").strip()
            tokens = getattr(resultado, "usage", None)
            tokens = tokens.completion_tokens if tokens else len(respuesta) // 4
            api_max_uso = _max_tokens_api(max_tokens, prov["razonamiento_tokens"])
            finish = getattr(resultado.choices[0], "finish_reason", None)
            truncado = (finish == "length") or (finish is None and int(tokens or 0) >= api_max_uso)
            _calibrar_razonamiento(prov["id"], prov["razonamiento_tokens"], tokens, respuesta, max_tokens, truncado)
            if not respuesta:
                raise _RespuestaVaciaError("El proveedor devolvió una respuesta vacía")
            return {"respuesta": respuesta, "tokens": tokens}
        except _RespuestaVaciaError:
            last_error = _RespuestaVaciaError("respuesta vacía en todos los proveedores")
            continue
        except Exception as e:
            is_rate_limit = type(e).__name__ in ("RateLimitError",) or getattr(e, "status_code", None) == 429
            if is_rate_limit:
                _marcar_estado(prov["id"], True)  # no es fallo de salud
            else:
                _marcar_estado(prov["id"], False)
            last_error = e
            continue

    raise ValueError(
        f"Ningún proveedor respondió correctamente. "
        f"Último error: {last_error}"
    )


def generar_respuesta_stream_externo(pregunta, contexto_chunks, guia=None, max_tokens=None, contexto_uso=None):
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    messages = _build_messages(pregunta, contexto_chunks, guia, max_tokens)

    if not _hay_proveedores_registrados():
        raise ValueError(
            "No hay proveedores externos en la BD. "
            "Crea uno en /llm/proveedores."
        )

    pool = _proveedores_habilitados()
    if not pool:
        raise ValueError(
            "Todos los proveedores están deshabilitados o fallaron la verificación. "
            "Activa alguno en /llm/proveedores o ejecuta verificar_proveedores.py."
        )

    import random
    random.shuffle(pool)
    max_intentos = 3  # probamos como mucho 3 proveedores por llamada
    intentos = 0
    last_error = None

    for prov in pool:
        intentos += 1
        if intentos > max_intentos:
            raise ValueError(
                f"Ninguno de los primeros {max_intentos} proveedores respondió. "
                "Intenta nuevamente más tarde o contacta soporte."
            )
        
        from openai import OpenAI
        client = OpenAI(api_key=prov["apikey"], base_url=prov["base_url"])
        try:
            stream = _llamar_externo(
                client, prov["modelo_base"], messages, max_tokens, stream=True,
                razonamiento_tokens=prov["razonamiento_tokens"],
                timeout=30,  # 30 segundos máximo por proveedor
            )
            _marcar_estado(prov["id"], True)
            if contexto_uso is not None:
                contexto_uso.update({
                    "proveedor_id": prov["id"],
                    "proveedor_nombre": prov["nombre"],
                    "modelo": prov["modelo_base"],
                })
            tokens_prompt = tokens_comp = 0
            texto_total = ""
            finish = None
            for chunk in stream:
                usage = getattr(chunk, "usage", None)
                if usage:
                    tokens_prompt = getattr(usage, "prompt_tokens", 0) or tokens_prompt
                    tokens_comp = getattr(usage, "completion_tokens", 0) or tokens_comp
                if chunk.choices:
                    fr = getattr(chunk.choices[0], "finish_reason", None)
                    if fr:
                        finish = fr
                    delta = chunk.choices[0].delta
                    texto = getattr(delta, "content", None)
                    if texto:
                        texto_total += texto
                        yield texto
            if not tokens_prompt and not tokens_comp:
                tokens_prompt, tokens_comp = _estimar_uso_fallback(messages, texto_total)
            api_max_uso = _max_tokens_api(max_tokens, prov["razonamiento_tokens"])
            truncado = (finish == "length") or (finish is None and int(tokens_comp or 0) >= api_max_uso)
            _calibrar_razonamiento(prov["id"], prov["razonamiento_tokens"], tokens_comp, texto_total, max_tokens, truncado)
            if not texto_total.strip():
                raise _RespuestaVaciaError("El proveedor devolvió una respuesta vacía")
            if contexto_uso is not None:
                contexto_uso.update({
                    "proveedor_id": prov["id"],
                    "proveedor_nombre": prov["nombre"],
                    "modelo": prov["modelo_base"],
                    "tokens_prompt": tokens_prompt,
                    "tokens_completados": tokens_comp,
                })
            yield ""
            return
        except _RespuestaVaciaError:
            last_error = _RespuestaVaciaError("respuesta vacía en todos los proveedores")
            continue
        except Exception as e:
            is_rate_limit = type(e).__name__ in ("RateLimitError",) or getattr(e, "status_code", None) == 429
            if is_rate_limit:
                _marcar_estado(prov["id"], True)
            else:
                _marcar_estado(prov["id"], False)
            last_error = e
            continue

    raise ValueError(
        f"Ningún proveedor respondió correctamente. "
        f"Último error: {last_error}"
    )


def generar_respuesta(pregunta, contexto_chunks, guia=None, max_tokens=None, llm_externo=None):
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    if usar_modelo_externo(llm_externo):
        return generar_respuesta_externo(pregunta, contexto_chunks, guia=guia, max_tokens=max_tokens)

    llm = cargar_modelo()
    prompt = _build_prompt(pregunta, contexto_chunks, guia, max_tokens)

    resultado = llm(
        prompt,
        max_tokens=max_tokens,
        temperature=0.1,
        stop=[f"<|{IM_END}|>"],
    )

    respuesta = resultado["choices"][0]["text"].strip()
    tokens = resultado["usage"]["completion_tokens"]

    return {
        "respuesta": respuesta,
        "tokens": tokens,
    }


def generar_respuesta_stream(pregunta, contexto_chunks, guia=None, max_tokens=None, llm_externo=None, contexto_uso=None):
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    if usar_modelo_externo(llm_externo):
        yield from generar_respuesta_stream_externo(pregunta, contexto_chunks, guia=guia, max_tokens=max_tokens, contexto_uso=contexto_uso)
        return

    llm = cargar_modelo()
    prompt = _build_prompt(pregunta, contexto_chunks, guia, max_tokens)

    stream = llm(
        prompt,
        max_tokens=max_tokens,
        temperature=0.1,
        stop=[f"<|{IM_END}|>"],
        stream=True,
    )

    if contexto_uso is not None:
        contexto_uso.update({
            "proveedor_id": None,
            "proveedor_nombre": None,
            "modelo": "local/qwen2.5-3b",
        })

    tokens = 0
    for chunk in stream:
        texto = chunk["choices"][0]["text"]
        if texto:
            tokens += 1
            yield texto

    if contexto_uso is not None:
        contexto_uso.update({
            "proveedor_id": None,
            "proveedor_nombre": None,
            "modelo": "local/qwen2.5-3b",
            "tokens_prompt": _estimar_tokens(prompt),
            "tokens_completados": tokens,
        })
    yield ""


# --- Puente directo al modelo (sin RAG) ---
#
# Las funciones de arriba arman SIEMPRE un prompt de asistente juridico con un
# bloque "CONTEXTO DE LEYES Y NORMATIVAS" y la instruccion de citar fuentes
# [1], [2]. Para una API que solo hace de puente al modelo eso es ruido: el
# contexto llega vacio y el system prompt miente. Lo de abajo reutiliza TODA la
# mecanica de proveedor (pool, sorteo, cooldown, calibracion de razonamiento,
# registro de uso) pero acepta los `messages` crudos del llamador.
#
# La eleccion de modelo NO se reimplementa: pasa por `usar_modelo_externo`, asi
# que el puente queda amarrado al mismo modo global (y al override por peticion)
# que usa /chat.

SYSTEM_PUENTE = (
    "Eres un asistente útil. Responde de forma clara y directa, en español "
    "correcto con tildes y ñ, y en texto plano: sin Markdown, sin negritas, "
    "sin viñetas, sin listas numeradas ni encabezados. Redacta todo en párrafos "
    "completos. Responde únicamente con la respuesta final, sin mostrar procesos "
    "de razonamiento ni repetir las instrucciones recibidas."
)


def _mensajes_puente(instruccion, sistema=None):
    """Turno unico (system + user) para el modo de usar el puente."""
    return [
        {"role": "system", "content": sistema or SYSTEM_PUENTE},
        {"role": "user", "content": instruccion},
    ]


def _chatml_desde_mensajes(mensajes, max_tokens=None):
    """Convierte `messages` de OpenAI al prompt ChatML que espera llama.cpp."""
    partes = [
        f"<|{IM_START}|>system\n{SYSTEM_PUENTE}"
        f"{_instruccion_presupuesto(max_tokens)}<|{IM_END}|>\n\n"
    ]
    for mensaje in mensajes or []:
        rol = mensaje.get("role", "user")
        if rol not in ("system", "user", "assistant"):
            rol = "user"
        partes.append(
            f"<|{IM_START}|>{rol}\n{mensaje.get('content', '')}<|{IM_END}|>\n\n"
        )
    partes.append(f"<|{IM_START}|>assistant\n")
    return "".join(partes)


def _verificar_prompt_local(prompt, max_tokens):
    """El modelo local tiene ventana dura (n_ctx=2048): avisar en vez de truncar.

    Sin esto llama.cpp recorta el prompt por la mitad y contesta basandose en una
    instruccion partida, que es peor que un error claro.
    """
    presupuesto = _presupuesto_contexto(max_tokens, llm_externo=False)
    if _estimar_tokens(prompt) > presupuesto:
        raise ContextoDemasiadoError(
            f"El prompt ocupa ~{_estimar_tokens(prompt)} tokens y el modelo local "
            f"solo tiene ventana para {presupuesto} (n_ctx={CTX_MAX_LOCAL}). "
            "Acorta la instruccion o pon el sistema en modo externo."
        )


def _como_texto(valor):
    """Normaliza a `str`. Un proveedor puede devolver dict/list pese a pedir texto.

    El envelope lleva SIEMPRE una frase en `respuesta`; si llega estructura, se
    serializa en vez de romper el tipo que el cliente espera.
    """
    if valor is None:
        return ""
    if isinstance(valor, str):
        return valor.strip()
    if isinstance(valor, (dict, list)):
        import json
        try:
            return json.dumps(valor, ensure_ascii=False)
        except (TypeError, ValueError):
            return str(valor)
    return str(valor).strip()


def generar_respuesta_directa(mensajes, max_tokens=None, llm_externo=None,
                              temperatura=0.7, contexto_uso=None,
                              razonamiento="low", formato_json=False, stop=None):
    """Genera una respuesta SIN RAG a partir de `messages` crudos.

    Devuelve un dict con `respuesta` (SIEMPRE str), `tokens`, `tokens_entrada`,
    `tokens_salida`, `tokens_razonamiento`, `finish_reason`, `truncado`,
    `modelo` y `proveedor`. Si se pasa `contexto_uso`, lo completa con las keys
    que usa /chat para `uso_llm`.

    `respuesta` se serializa si el proveedor devolvio algo que no es texto: el
    envelope nunca puede llevar un objeto donde el cliente espera una frase.
    """
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    if not mensajes:
        raise ValueError("No hay mensajes para enviar al modelo.")

    if usar_modelo_externo(llm_externo):
        return _puente_externo(
            mensajes, max_tokens, temperatura, contexto_uso=contexto_uso,
            razonamiento=razonamiento, formato_json=formato_json, stop=stop,
        )

    llm = cargar_modelo()
    prompt = _chatml_desde_mensajes(mensajes, max_tokens)
    _verificar_prompt_local(prompt, max_tokens)

    resultado = llm(
        prompt,
        max_tokens=max_tokens,
        temperature=temperatura,
        stop=list(stop) if stop else [f"<|{IM_END}|>"],
    )

    crudo = resultado["choices"][0]["text"]
    respuesta = _como_texto(crudo)
    tokens = resultado["usage"]["completion_tokens"]
    finish = resultado["choices"][0].get("finish_reason")

    if contexto_uso is not None:
        contexto_uso.update({
            "proveedor_id": None,
            "proveedor_nombre": None,
            "modelo": "local/qwen2.5-3b",
            "tokens_prompt": _estimar_tokens(prompt),
            "tokens_completados": tokens,
        })

    return {
        "respuesta": respuesta,
        "tokens": tokens,
        "tokens_entrada": _estimar_tokens(prompt),
        "tokens_salida": tokens,
        "tokens_razonamiento": 0,
        "finish_reason": finish,
        "truncado": finish == "length",
        "modelo": "local/qwen2.5-3b",
        "proveedor": None,
        "json_no_soportado": bool(formato_json),
    }


def _puente_externo(mensajes, max_tokens, temperatura, contexto_uso=None,
                    razonamiento="low", formato_json=False, stop=None):
    """Mismo pool/sorteo/retry de `generar_respuesta_externo`, con `messages` crudos."""
    if not _hay_proveedores_registrados():
        raise ValueError(
            "No hay proveedores externos en la BD. Crea uno en /llm/proveedores."
        )

    pool = _proveedores_habilitados()
    if not pool:
        raise ValueError(
            "Todos los proveedores están deshabilitados o fallaron la verificación. "
            "Activa alguno en /llm/proveedores o ejecuta verificar_proveedores.py."
        )

    import random
    random.shuffle(pool)
    last_error = None

    for prov in pool:
        from openai import OpenAI
        client = OpenAI(api_key=prov["apikey"], base_url=prov["base_url"])
        try:
            resultado = _llamar_externo(
                client, prov["modelo_base"], mensajes, max_tokens,
                razonamiento_tokens=prov["razonamiento_tokens"],
                temperatura=temperatura, razonamiento=razonamiento,
                formato_json=formato_json, stop=stop,
            )
            _marcar_estado(prov["id"], True)
            respuesta = _como_texto(resultado.choices[0].message.content)
            usage = getattr(resultado, "usage", None)
            tokens = usage.completion_tokens if usage else len(respuesta) // 4
            tokens_prompt = (
                usage.prompt_tokens if usage and usage.prompt_tokens
                else _estimar_tokens("".join(
                    str(m.get("content", "")) for m in mensajes
                ))
            )
            tokens_razonamiento = 0
            if usage is not None:
                detalles = getattr(usage, "completion_tokens_details", None)
                tokens_razonamiento = getattr(detalles, "reasoning_tokens", 0) or 0
            api_max_uso = _max_tokens_api(max_tokens, prov["razonamiento_tokens"])
            finish = getattr(resultado.choices[0], "finish_reason", None)
            truncado = (finish == "length") or (
                finish is None and int(tokens or 0) >= api_max_uso
            )
            _calibrar_razonamiento(
                prov["id"], prov["razonamiento_tokens"], tokens, respuesta,
                max_tokens, truncado,
            )
            if not respuesta:
                raise _RespuestaVaciaError("El proveedor devolvió una respuesta vacía")
            if contexto_uso is not None:
                contexto_uso.update({
                    "proveedor_id": prov["id"],
                    "proveedor_nombre": prov["nombre"],
                    "modelo": prov["modelo_base"],
                    "tokens_prompt": tokens_prompt,
                    "tokens_completados": tokens,
                })
            return {
                "respuesta": respuesta,
                "tokens": tokens,
                "tokens_entrada": tokens_prompt,
                "tokens_salida": tokens,
                "tokens_razonamiento": tokens_razonamiento,
                "finish_reason": finish,
                "truncado": truncado,
                "modelo": prov["modelo_base"],
                "proveedor": prov["nombre"],
                "json_no_soportado": json_no_soportado(prov["base_url"], prov["modelo_base"]),
            }
        except _RespuestaVaciaError:
            last_error = _RespuestaVaciaError("respuesta vacía en todos los proveedores")
            continue
        except Exception as e:
            is_rate_limit = type(e).__name__ in ("RateLimitError",) or getattr(e, "status_code", None) == 429
            if is_rate_limit:
                _marcar_estado(prov["id"], True)  # no es fallo de salud
            else:
                _marcar_estado(prov["id"], False)
            last_error = e
            continue

    raise ValueError(
        f"Ningún proveedor respondió correctamente. Último error: {last_error}"
    )

def generar_respuesta_directa_stream(mensajes, max_tokens=None, llm_externo=None,
                                     temperatura=0.7, contexto_uso=None,
                                     razonamiento="low", formato_json=False, stop=None):
    """Generador del puente (sin RAG). Yield de eventos ya tipados.

    A diferencia de `generar_respuesta_stream`, cada yield es un dict:
      {"tipo": "token", "texto": "..."}        fragmento de la respuesta
      {"tipo": "razonamiento", "texto": "..."}  pensamiento (opcional, §8)
      {"fin": {...}}                            ultimo yield: metadatos para el
                                                 evento `done` del envelope

    Separar el razonamiento permite que el cliente lo muestre aparte y que
    `respuesta` contenga solo la respuesta final. El `""` de cierre del
    generador antiguo se sustituye por el evento `fin`.
    """
    max_tokens = max_tokens or MAX_TOKENS_RESPUESTA
    if not mensajes:
        raise ValueError("No hay mensajes para enviar al modelo.")

    if usar_modelo_externo(llm_externo):
        yield from _puente_externo_stream(
            mensajes, max_tokens, temperatura, contexto_uso=contexto_uso,
            razonamiento=razonamiento, formato_json=formato_json, stop=stop,
        )
        return

    llm = cargar_modelo()
    prompt = _chatml_desde_mensajes(mensajes, max_tokens)
    _verificar_prompt_local(prompt, max_tokens)

    modelo_local = "local/qwen2.5-3b"
    if contexto_uso is not None:
        contexto_uso.update({
            "proveedor_id": None,
            "proveedor_nombre": None,
            "modelo": modelo_local,
        })

    stream = llm(
        prompt,
        max_tokens=max_tokens,
        temperature=temperatura,
        stop=list(stop) if stop else [f"<|{IM_END}|>"],
        stream=True,
    )

    tokens = 0
    finish = None
    for chunk in stream:
        if chunk["choices"]:
            finish = chunk["choices"][0].get("finish_reason") or finish
        texto = chunk["choices"][0]["text"]
        if texto:
            tokens += 1
            yield {"tipo": "token", "texto": texto}

    tokens_prompt = _estimar_tokens(prompt)
    if contexto_uso is not None:
        contexto_uso.update({
            "tokens_prompt": tokens_prompt,
            "tokens_completidos": tokens,
        })
    yield {
        "fin": {
            "tokens_entrada": tokens_prompt,
            "tokens_salida": tokens,
            "tokens_razonamiento": 0,
            "finish_reason": finish,
            "truncado": finish == "length" or (finish is None and tokens >= max_tokens),
            "modelo": modelo_local,
            "proveedor": None,
            "json_no_soportado": bool(formato_json),
        }
    }


def _puente_externo_stream(mensajes, max_tokens, temperatura, contexto_uso=None,
                           razonamiento="low", formato_json=False, stop=None):
    """Pool/sorteo/retry de `generar_respuesta_stream_externo` con `messages` crudos."""
    if not _hay_proveedores_registrados():
        raise ValueError(
            "No hay proveedores externos en la BD. Crea uno en /llm/proveedores."
        )

    pool = _proveedores_habilitados()
    if not pool:
        raise ValueError(
            "Todos los proveedores están deshabilitados o fallaron la verificación. "
            "Activa alguno en /llm/proveedores o ejecuta verificar_proveedores.py."
        )

    import random
    random.shuffle(pool)
    max_intentos = 3
    intentos = 0
    last_error = None

    for prov in pool:
        intentos += 1
        if intentos > max_intentos:
            raise ValueError(
                f"Ninguno de los primeros {max_intentos} proveedores respondió. "
                "Intenta nuevamente más tarde o contacta soporte."
            )

        from openai import OpenAI
        client = OpenAI(api_key=prov["apikey"], base_url=prov["base_url"])
        try:
            stream = _llamar_externo(
                client, prov["modelo_base"], mensajes, max_tokens, stream=True,
                razonamiento_tokens=prov["razonamiento_tokens"],
                timeout=30, temperatura=temperatura,
                razonamiento=razonamiento, formato_json=formato_json, stop=stop,
            )
            _marcar_estado(prov["id"], True)
            if contexto_uso is not None:
                contexto_uso.update({
                    "proveedor_id": prov["id"],
                    "proveedor_nombre": prov["nombre"],
                    "modelo": prov["modelo_base"],
                })
            tokens_prompt = tokens_comp = tokens_razon = 0
            texto_total = ""
            finish = None
            for chunk in stream:
                usage = getattr(chunk, "usage", None)
                if usage:
                    tokens_prompt = getattr(usage, "prompt_tokens", 0) or tokens_prompt
                    tokens_comp = getattr(usage, "completion_tokens", 0) or tokens_comp
                    detalles = getattr(usage, "completion_tokens_details", None)
                    tokens_razon = getattr(detalles, "reasoning_tokens", 0) or tokens_razon
                if not chunk.choices:
                    continue
                fr = getattr(chunk.choices[0], "finish_reason", None)
                if fr:
                    finish = fr
                delta = chunk.choices[0].delta
                # gpt-oss manda el pensamiento en `reasoning_content` (Groq) o en
                # `reasoning` (otros proveedores). Se emiten aparte para que
                # `respuesta` solo lleve la respuesta final.
                razon = getattr(delta, "reasoning_content", None) or getattr(delta, "reasoning", None)
                if razon:
                    yield {"tipo": "razonamiento", "texto": razon}
                texto = getattr(delta, "content", None)
                if texto:
                    texto_total += texto
                    yield {"tipo": "token", "texto": texto}

            if not tokens_prompt and not tokens_comp:
                tokens_prompt, tokens_comp = _estimar_uso_fallback(mensajes, texto_total)
            if contexto_uso is not None:
                contexto_uso.update({
                    "tokens_prompt": tokens_prompt,
                    "tokens_completidos": tokens_comp,
                })
            api_max_uso = _max_tokens_api(max_tokens, prov["razonamiento_tokens"])
            truncado = (finish == "length") or (
                finish is None and int(tokens_comp or 0) >= api_max_uso
            )
            yield {
                "fin": {
                    "tokens_entrada": tokens_prompt,
                    "tokens_salida": tokens_comp,
                    "tokens_razonamiento": tokens_razon,
                    "finish_reason": finish,
                    "truncado": truncado,
                    "modelo": prov["modelo_base"],
                    "proveedor": prov["nombre"],
                    "json_no_soportado": json_no_soportado(prov["base_url"], prov["modelo_base"]),
                }
            }
            return
        except _RespuestaVaciaError:
            last_error = _RespuestaVaciaError("respuesta vacía en todos los proveedores")
            continue
        except Exception as e:
            is_rate_limit = type(e).__name__ in ("RateLimitError",) or getattr(e, "status_code", None) == 429
            if is_rate_limit:
                _marcar_estado(prov["id"], True)
            else:
                _marcar_estado(prov["id"], False)
            last_error = e
            continue

    raise ValueError(
        f"Ningún proveedor respondió correctamente. Último error: {last_error}"
    )
