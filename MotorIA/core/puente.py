"""Logica de contrato de `POST /puente` y `POST /puente/stream`.

Este modulo NO sabe nada de EDAN (ver PUENTE/PUENTE_EDAN_API.md §11). Solo
cumple el contrato generico: armar los mensajes, garantizar JSON cuando se pide,
mapear fallos a codigos tipificados y construir el envelope.

La separacion con `core/llm.py` es deliberada: alli vive el sorteo de
proveedores, el razonamiento y el conteo de tokens (que tambien usa /chat); aqui
vive el contrato que el cliente puede cambiar sin tocar el camino juridico.
"""

import json
import re

from core.llm import (
    MAX_TOKENS_RESPUESTA,
    SYSTEM_PUENTE,
    generar_respuesta_directa,
    generar_respuesta_directa_stream,
    usar_modelo_externo,
)

# Avisos del envelope (§4.5). Texto para logs, nunca para mostrar.
AVISO_JSON_NO_SOPORTADO = "json_no_soportado"
AVISO_TRUNCADO = "truncado_por_max_tokens"
AVISO_REINTENTO_JSON = "reintento_por_json_invalido"
AVISO_RESPUESTA_NO_STRING = "respuesta_no_string"

# Tabla de errores tipificados (§6). `http` es lo que ve el cliente; lo que de
# verdad decide el comportamiento es `reintentable`.
ERRORES = {
    "rate_limit": {"http": 429, "reintentable": True, "esperar_ms": 12000},
    "json_invalido": {"http": 422, "reintentable": False, "esperar_ms": None},
    "truncado": {"http": 422, "reintentable": False, "esperar_ms": None},
    "timeout": {"http": 504, "reintentable": True, "esperar_ms": None},
    "proveedor_caido": {"http": 502, "reintentable": True, "esperar_ms": None},
    "no_autorizado": {"http": 401, "reintentable": False, "esperar_ms": None},
    "contexto_demasiado": {"http": 413, "reintentable": False, "esperar_ms": None},
}

MENSAJES_ERROR = {
    "rate_limit": "El proveedor alcanzó su límite de tokens por minuto",
    "json_invalido": "El proveedor no devolvió JSON válido",
    "truncado": "La respuesta se cortó por max_tokens y puede estar incompleta",
    "timeout": "El proveedor tardó demasiado en responder",
    "proveedor_caido": "Ningún proveedor externo respondió",
    "no_autorizado": "Falta o es inválida la cabecera X-API-Key",
    "contexto_demasiado": "El contexto no entra en la ventana del modelo",
}

# Orden en que se prueba a extraer un objeto del texto. Primero el bloque mas
# grande (markdown lo envuelve entero), despues el primer objeto balanceado.
_BLOQUE_JSON = re.compile(r"```(?:json)?\s*(.*?)```", re.DOTALL | re.IGNORECASE)


class ErrorPuente(Exception):
    """Fallo del puente con codigo de la tabla §6, para que el cliente decida."""

    def __init__(self, codigo, detalle=None, mensaje=None, esperar_ms=None):
        self.codigo = codigo
        self.detalle = detalle or {}
        self.mensaje = mensaje or MENSAJES_ERROR.get(codigo, codigo)
        self.esperar_ms = esperar_ms if esperar_ms is not None else ERRORES[codigo]["esperar_ms"]
        super().__init__(self.mensaje)

    @property
    def http(self):
        return ERRORES[self.codigo]["http"]

    @property
    def reintentable(self):
        return ERRORES[self.codigo]["reintentable"]

    def payload(self):
        return {
            "tipo": "error",
            "codigo": self.codigo,
            "mensaje": self.mensaje,
            "reintentable": self.reintentable,
            "esperar_ms": self.esperar_ms,
            "detalle": self.detalle,
        }


def construir_mensajes(instruccion=None, sistema=None, prompt=None, mensajes=None):
    """Arma los mensajes aplicando la precedencia §2 y §3.1.

        instruccion  >  sistema  >  prompt generico del puente   (el system)
        mensajes     >  prompt                                    (el turno)

    `mensajes` se pasa tal cual: el puente no reescribe ni resume el historial
    porque el cliente EDAN ya armo el bloque de estado a proposito.
    """
    if mensajes:
        return [dict(m) for m in mensajes]

    system = instruccion or sistema or SYSTEM_PUENTE
    user = prompt if prompt is not None and str(prompt).strip() else "Quiero reportar un evento."
    return [
        {"role": "system", "content": system},
        {"role": "user", "content": user},
    ]


def _ultimo_de_usuario(mensajes):
    for mensaje in reversed(mensajes or []):
        if mensaje.get("role") == "user":
            return mensaje.get("content", "")
    return ""


def _mensajes_de_reparacion(mensajes, instruction_sistema):
    """Reintento de formato (§3.2): una pasada, sin historial.

    Se reusa la instruccion del cliente en vez de una propia para no perder el
    vocabulario que el cliente yaKnow: el puente sigue sin saber de EDAN, pero
    el modelo recibe la definicion de "parche" que el cliente mando.
    """
    ultimo = _ultimo_de_usuario(mensajes)
    sistema = instruction_sistema or SYSTEM_PUENTE
    return [
        {
            "role": "system",
            "content": (
                f"{sistema}\n\n"
                "Turno de reparacion: el intento anterior no devolvio JSON valido. "
                "Responde UNICAMENTE con un objeto JSON, sin texto antes ni despues, "
                "sin bloque de codigo y sin razonamiento."
            ),
        },
        {
            "role": "user",
            "content": (
                f'Lo que dijo la persona: "{ultimo}"\n'
                "Anota lo que ya dijo, sin preguntar nada.\n"
                "Si de verdad no hay ningun dato, devuelve el parche vacio."
            ),
        },
    ]


def parsear_json_tolerante(texto):
    """Saca un objeto JSON de un texto que quizas viene envuelto en prosa.

    Devuelve (objeto|None, venia_structura). `venia_structura` distingue que el
    modelo ya habia devuelto un objeto (o un string JSON entre comillas) de que
    hubo que rasparlo del texto, que es lo que dispara el aviso
    `respuesta_no_string`.
    """
    if texto is None:
        return None, False
    if isinstance(texto, dict):
        return texto, True

    crudo = str(texto).strip()
    if not crudo:
        return None, False

    # El proveedor ya entrego el objeto, solo que el SDK lo dio como texto.
    if len(crudo) >= 2 and crudo[0] == "{" and crudo[-1] == "}":
        try:
            valor = json.loads(crudo)
        except ValueError:
            pass
        else:
            return (valor, True) if isinstance(valor, dict) else (None, True)

    for candidato in _candidatos_json(crudo):
        try:
            valor = json.loads(candidato)
        except ValueError:
            continue
        if isinstance(valor, dict):
            return valor, False
    return None, False


def _candidatos_json(texto):
    """Texto completo, bloque de ```json y primer objeto balanceado."""
    yield texto

    for bloque in _BLOQUE_JSON.findall(texto):
        bloque = bloque.strip()
        if bloque:
            yield bloque

    inicio = texto.find("{")
    if inicio != -1:
        fin = _fin_de_objeto(texto, inicio)
        if fin > inicio + 1:
            yield texto[inicio:fin]


def _fin_de_objeto(texto, inicio):
    """Indice del cierre del objeto que arranca en `inicio`, respetando strings."""
    profundidad = 0
    en_cadena = False
    escapado = False
    for i in range(inicio, len(texto)):
        caracter = texto[i]
        if en_cadena:
            if escapado:
                escapado = False
            elif caracter == "\\":
                escapado = True
            elif caracter == '"':
                en_cadena = False
            continue
        if caracter == '"':
            en_cadena = True
        elif caracter == "{":
            profundidad += 1
        elif caracter == "}":
            profundidad -= 1
            if profundidad == 0:
                return i + 1
    return len(texto)


def validar_json_schema(obj, json_schema):
    """Valida contra el schema que manda el cliente. Devuelve bool.

    No se traga la importacion: si `jsonschema` no esta, la validacion se
    salta. Degradar es mejor que tumbarle la ficha al cliente.
    """
    if not json_schema or obj is None:
        return True
    try:
        from jsonschema import Draft202012Validator
    except ImportError:
        return True
    try:
        errores = list(Draft202012Validator(json_schema).iter_errors(obj))
    except Exception:
        return True
    return not errores


def _clasificar_fallo(excepcion):
    """Mapea la excepcion cruda del proveedor a un codigo de la tabla §6."""
    from core.llm import ContextoDemasiadoError

    if isinstance(excepcion, ContextoDemasiadoError):
        return "contexto_demasiado"
    nombre = type(excepcion).__name__
    status = getattr(excepcion, "status_code", None)
    if nombre == "RateLimitError" or status == 429:
        return "rate_limit"
    if nombre in ("APITimeoutError", "Timeout", "ReadTimeout") or "timeout" in nombre.lower():
        return "timeout"
    return "proveedor_caido"


def _envolver_error(excepcion):
    """Convierte cualquier fallo del LLM en `ErrorPuente`. Idempotente."""
    if isinstance(excepcion, ErrorPuente):
        return excepcion
    codigo = _clasificar_fallo(excepcion)
    detalle = {}
    status = getattr(excepcion, "status_code", None)
    if status:
        detalle["status_proveedor"] = status
    return ErrorPuente(codigo, detalle=detalle, mensaje=str(excepcion))



def _envelope(resultado, formato_pedido, json_obj, intentos, avisos):
    """Arma el body de §4. Regla 1: `respuesta` SIEMPRE string."""
    avisos = list(dict.fromkeys(avisos))
    envelope = {
        "respuesta": resultado["respuesta"],
        "json": json_obj,
        "formato": "json" if json_obj is not None else "texto",
        "modelo": resultado.get("modelo"),
        "proveedor": resultado.get("proveedor") or "local",
        "intentos": max(1, min(3, intentos)),
        "tokens": {
            "entrada": int(resultado.get("tokens_entrada") or 0),
            "salida": int(resultado.get("tokens_salida") or 0),
            "razonamiento": int(resultado.get("tokens_razonamiento") or 0),
        },
        "avisos": avisos,
    }
    if envelope["formato"] == "texto" and formato_pedido == "json":
        # Se pidio JSON y no vino: el aviso lo dice, y el cliente baja al
        # extractor determinista sobre `respuesta` (§10). Nunca un 500.
        if AVISO_JSON_NO_SOPORTADO not in avisos and resultado.get("json_no_soportado"):
            avisos.append(AVISO_JSON_NO_SOPORTADO)
        if intentos > 1 and AVISO_REINTENTO_JSON not in avisos:
            avisos.append(AVISO_REINTENTO_JSON)
        envelope["avisos"] = list(dict.fromkeys(avisos))
    return envelope


def _avisos_de_truncado(resultado, formato_pedido, avisos):
    """Regla de truncado: 422 solo si se pidio JSON; si no, 200 con aviso.

    Un 200 con texto truncado es recuperable (el cliente recorta el turno). Un
    200 con un JSON cortado NO lo es: el cliente lo aplicaria al borrador y
    perderia datos, asi que ahi si tiene que enterarse (§13 prueba 2).
    """
    if not resultado.get("truncado"):
        return
    if formato_pedido == "json":
        raise ErrorPuente(
            "truncado",
            detalle={
                "max_tokens": resultado.get("max_tokens"),
                "finish_reason": resultado.get("finish_reason"),
                "tokens_salida": int(resultado.get("tokens_salida") or 0),
            },
        )
    avisos.append(AVISO_TRUNCADO)


def ejecutar_puente(req, contexto_uso=None):
    """`POST /puente` sin streaming. Devuelve el envelope de §4."""
    formato_pedido = req.formato
    mensajes = construir_mensajes(
        req.instruccion, req.sistema, req.prompt, req.mensajes
    )
    avisos = []
    total_intentos = 1
    ultimo_resultado = None
    json_obj = None

    # `intentos_json` son intentos EXTRA: 1 + N. El tope de 2 sale del schema y
    # deja el contador de `intentos` del envelope dentro de 1..3 (§4.4).
    for intento in range(1 + max(0, min(2, req.intentos_json))):
        if intento > 0:
            avisos.append(AVISO_REINTENTO_JSON)
        a_enviar = (
            _mensajes_de_reparacion(mensajes, req.instruccion or req.sistema)
            if intento > 0 else mensajes
        )
        try:
            resultado = generar_respuesta_directa(
                a_enviar,
                max_tokens=req.max_tokens or MAX_TOKENS_RESPUESTA,
                llm_externo=req.llm_externo,
                temperatura=req.temperatura,
                contexto_uso=contexto_uso,
                razonamiento=req.razonamiento,
                formato_json=formato_pedido == "json",
                stop=req.stop,
            )
        except Exception as e:
            raise _envolver_error(e) from e

        ultimo_resultado = resultado
        total_intentos = intento + 1
        resultado["max_tokens"] = req.max_tokens or MAX_TOKENS_RESPUESTA

        if formato_pedido != "json":
            break

        json_obj, ya_venia = parsear_json_tolerante(resultado["respuesta"])
        if json_obj is not None and ya_venia:
            avisos.append(AVISO_RESPUESTA_NO_STRING)
        if json_obj is not None and not validar_json_schema(json_obj, req.json_schema):
            json_obj = None  # cuenta como intento fallido (§3.1)
        if json_obj is not None:
            break

    _avisos_de_truncado(ultimo_resultado, formato_pedido, avisos)
    return _envelope(ultimo_resultado, formato_pedido, json_obj, total_intentos, avisos)


def ejecutar_puente_stream(req, contexto_uso=None):
    """`POST /puente/stream`. Devuelve generador de eventos de §5.

    Los tokens salen en vivo y el `json`/`done` al final, que es lo que
    garantiza que el parche se aplique completo.

    Un reintento de formato no puede "desenviar" los tokens del intento
    anterior, y el prompt de reparacion pide JSON puro (§3.2), asi que en los
    intentos > 1 no se emiten eventos de texto: solo cuentan para produzir el
    `json` final. El `respuesta` del evento `done` es el del ultimo intento, que
    es el campo autoritativo. Cuando `intentos` vale 1 (lo normal) los `token`
    concatenados dan exactamente ese mismo texto.
    """
    formato_pedido = req.formato
    mensajes = construir_mensajes(
        req.instruccion, req.sistema, req.prompt, req.mensajes
    )
    avisos = []
    total_intentos = 1
    ultimo_fin = None
    json_obj = None

    for intento in range(1 + max(0, min(2, req.intentos_json))):
        if intento > 0:
            avisos.append(AVISO_REINTENTO_JSON)
        a_enviar = (
            _mensajes_de_reparacion(mensajes, req.instruccion or req.sistema)
            if intento > 0 else mensajes
        )
        emitir = intento == 0
        ultimo_fin = None
        acumulado = []
        try:
            for evento in generar_respuesta_directa_stream(
                a_enviar,
                max_tokens=req.max_tokens or MAX_TOKENS_RESPUESTA,
                llm_externo=req.llm_externo,
                temperatura=req.temperatura,
                contexto_uso=contexto_uso,
                razonamiento=req.razonamiento,
                formato_json=formato_pedido == "json",
                stop=req.stop,
            ):
                if "fin" in evento:
                    ultimo_fin = evento["fin"]
                    continue
                # Solo los `token` cuentan para `respuesta`: si se acumulara tambien
                # el razonamiento, la cadena de pensamiento del modelo seiria
                # adelante de la frase y el cliente la leeria en voz alta (§1.2).
                if evento.get("tipo") == "token":
                    acumulado.append(evento.get("texto") or "")
                if emitir:
                    yield evento
        except ErrorPuente:
            raise
        except Exception as e:
            raise _envolver_error(e) from e

        total_intentos = intento + 1
        if ultimo_fin is None:
            ultimo_fin = {}
        ultimo_fin["respuesta"] = "".join(acumulado)

        if formato_pedido != "json":
            break

        json_obj, ya_venia = parsear_json_tolerante(ultimo_fin["respuesta"])
        if json_obj is not None and ya_venia:
            avisos.append(AVISO_RESPUESTA_NO_STRING)
        if json_obj is not None and not validar_json_schema(json_obj, req.json_schema):
            json_obj = None
        if json_obj is not None:
            break

    ultimo_fin.setdefault("max_tokens", req.max_tokens or MAX_TOKENS_RESPUESTA)
    _avisos_de_truncado(ultimo_fin, formato_pedido, avisos)
    envelope = _envelope(ultimo_fin, formato_pedido, json_obj, total_intentos, avisos)

    if envelope["formato"] == "json":
        yield {"tipo": "json", "json": json_obj}
    yield {"tipo": "done", **envelope}
    yield {"__done__": True}
