"""`POST /puente` y `POST /puente/stream` segun PUENTE/PUENTE_EDAN_API.md.

Este router no sabe de EDAN ni de leyes: es un puente generico al LLM. No hace
retrieval, no arma contexto legal y no cita fuentes. La logica de contrato vive
en `core/puente.py`; aqui solo se traduce HTTP <-> contrato y seTipifican los
errores.
"""

import functools
import inspect
import json
import logging
from itertools import chain

from fastapi import APIRouter, Request, Security
from fastapi.responses import JSONResponse, StreamingResponse
from pydantic import BaseModel, ConfigDict, Field, model_validator
from typing import List, Literal, Optional

from api.security import _token_es_valido, api_key_scheme, token_api_disponible
from core.puente import ErrorPuente, ejecutar_puente, ejecutar_puente_stream
from db.queries import eliminar_uso_llm, finalizar_uso_llm, iniciar_uso_llm

log = logging.getLogger(__name__)

router = APIRouter(prefix="/puente", tags=["puente"])


# --- Modelos del contrato (espejo de PUENTE/schemas/puente-request) ---

class Mensaje(BaseModel):
    model_config = ConfigDict(extra="forbid")

    role: Literal["system", "user", "assistant"] = "user"
    content: str


class PuenteRequest(BaseModel):
    """Body de §3. `extra="forbid"` reproduce el `additionalProperties:false` del
    schema: una clave desconocida es un error de contrato, no algo que se ignore
    en silencio."""

    model_config = ConfigDict(extra="forbid")

    instruccion: Optional[str] = Field(
        None, description="Instruccion del asistente (bloque de estado incluido). System."
    )
    mensajes: Optional[List[Mensaje]] = Field(
        None, description="Historial literal. Si viene, gana sobre `prompt`."
    )
    prompt: Optional[str] = Field(
        None, description="Atajo de un solo turno, reservado al asistente legal."
    )
    sistema: Optional[str] = Field(
        None, description="System propio del asistente legal. Precedido por `instruccion`."
    )
    formato: Literal["texto", "json"] = Field(
        "texto", description="'json' reenvia response_format y verifica que lo que vuelve parsea."
    )
    json_schema: Optional[dict] = Field(
        None, description="Schema opcional contra el cual validar `json`."
    )
    intentos_json: int = Field(
        1, ge=0, le=2, description="Intentos extra cuando formato=json y no sale JSON valido."
    )
    razonamiento: Literal["off", "low", "medium"] = Field(
        "off", description="Mapea a reasoning_effort. 'off' no lo manda."
    )
    stop: Optional[List[str]] = Field(
        None, description="Secuencias de corte, se pasan tal cual."
    )
    max_tokens: int = Field(
        512, ge=1, description="EDAN necesita 1200 o mas; 512 trunca el parche."
    )
    temperatura: float = Field(
        0.7, ge=0.0, le=2.0, description="0.3 para EDAN, para que no invente cifras."
    )
    llm_externo: Optional[bool] = Field(
        None, description="null usa el modo global; true/false lo fuerzan para esta llamada."
    )

    @model_validator(mode="after")
    def _al_menos_un_turno(self):
        """Reproduce el `anyOf` de §3: sin conversacion no hay nada que preguntar.

        `mensajes: []` cuenta como ausente: el schema lo daria por bueno porque la
        clave esta, pero un historial vacio no es una peticion.
        """
        if not (self.instruccion or self.mensajes or self.prompt or self.sistema):
            raise ValueError(
                "manda `instruccion`, `mensajes`, `prompt` o `sistema`: "
                "sin conversacion el puente no tiene nada que responder"
            )
        return self


class TokensPuente(BaseModel):
    model_config = ConfigDict(extra="forbid")

    entrada: int = 0
    salida: int = 0
    razonamiento: int = 0


class PuenteResponse(BaseModel):
    """Envelope de §4. Declararlo aqui cierra el `200: {}` del Openapi (§1.5)."""

    respuesta: str = Field(..., description="Siempre string, aunque el modelo devuelva objeto.")
    json_: Optional[dict] = Field(
        default=None, alias="json", description="Objeto ya parseado, o null."
    )
    formato: Literal["texto", "json"]
    modelo: str
    proveedor: str
    intentos: int = Field(..., ge=1, le=3)
    tokens: TokensPuente
    avisos: List[str] = Field(default_factory=list)


class ErrorPuenteBody(BaseModel):
    """Errores tipificados de §6: el cliente decide reintentar o bajar al extractor."""

    tipo: Literal["error"] = "error"
    codigo: Literal[
        "rate_limit", "json_invalido", "truncado", "timeout",
        "proveedor_caido", "no_autorizado", "contexto_demasiado",
    ]
    mensaje: str
    reintentable: bool
    esperar_ms: Optional[int] = None
    detalle: dict = Field(default_factory=dict)


# --- Auth ---

def _exigir_api_key(x_api_key: Optional[str] = Security(api_key_scheme)):
    """§1/§13.10: sin `X-API-Key` valido, 401 `no_autorizado`. Siempre.

    A diferencia de `_validar_token_api`, un servidor sin `API_AUTH_TOKEN` no
    devuelve 500: cierra en falso y se avisa por log, porque un 500 le diria al
    cliente que reintente cuando el problema es de configuracion.
    """
    if not _token_es_valido(x_api_key):
        if not token_api_disponible():
            log.error(
                "API_AUTH_TOKEN no esta configurado: /puente rechaza todas las llamadas."
            )
        raise ErrorPuente(
            "no_autorizado",
            detalle={"cabecera": "X-API-Key"},
            mensaje="Falta o es invalida la cabecera X-API-Key.",
        )


def _cerrar_uso(uso_id, exitoso, contexto_uso=None):
    """Cierra la fila de `uso_llm`. Nunca deja que el registro rompa la respuesta.

    El conteo de uso es una metrica, no parte del contrato: si falla, el cliente
    ya recibio su respuesta y no debe enterarse. Antes, una excepcion ahi se
    traducía en un `proveedor_caido` despues de haber contestado bien.
    """
    if uso_id is None:
        return
    try:
        if exitoso:
            finalizar_uso_llm(uso_id, exitoso=True, **_solo_claves_de_uso(contexto_uso))
        else:
            eliminar_uso_llm(uso_id)
    except Exception:
        log.warning("No se pudo registrar el uso %s", uso_id, exc_info=True)


@functools.lru_cache(maxsize=1)
def _claves_de_uso():
    """Nombres que `finalizar_uso_llm` acepta, leidos de su firma.

    Filtrar por introspection evita mantener una lista a mano que se desincronice
    cuando `contexto_uso` gane una clave nueva.
    """
    return frozenset(inspect.signature(finalizar_uso_llm).parameters)


def _solo_claves_de_uso(contexto_uso):
    if not contexto_uso:
        return {}
    aceptadas = _claves_de_uso()
    return {k: v for k, v in contexto_uso.items() if k in aceptadas}


@router.get("/modo", responses={401: {"model": ErrorPuenteBody}})
def modo_puente(x_api_key: Optional[str] = Security(api_key_scheme)):
    """A que modelo ira el puente ahora mismo, sin llamar a ningun proveedor.

    Es una ayuda de diagnostico: con el sorteo entre proveedores, ver el modo
    global separado de la llamada es lo primero que hay que mirar cuando un
    cliente reporta que el modelo "no entiende".
    """
    _exigir_api_key(x_api_key)
    from core.llm import get_modo_global

    modo = get_modo_global()
    return {
        "modo": modo,
        "destino": "externo" if modo == "external" else "local",
        "explicacion": (
            "Con el sistema en 'external' el puente responde un proveedor "
            "externo sorteado al azar entre los habilitados; con 'local', el "
            "modelo local. Cada llamada puede forzarlo con `llm_externo`."
        ),
    }


# --- Rutas ---

@router.post("", response_model=PuenteResponse, responses={401: {"model": ErrorPuenteBody}})
def puente(req: PuenteRequest, x_api_key: Optional[str] = Security(api_key_scheme)):
    """Puente directo al modelo, sin RAG.

    No busca chunks, no arma contexto de leyes y no cita fuentes: la instruccion
    va al modelo tal cual. El modelo se elige igual que en `/chat`, via
    `usar_modelo_externo`: modo global `external` -> proveedor sorteado al azar
    entre los habilitados; modo global `local` -> modelo local. `llm_externo`
    fuerza uno u otro solo para esta llamada.

    Requiere `X-API-Key`. Para ir token a token, `POST /puente/stream`.
    """
    _exigir_api_key(x_api_key)
    uso_id = iniciar_uso_llm(tipo="puente")
    contexto_uso = {}
    try:
        envelope = ejecutar_puente(req, contexto_uso=contexto_uso)
    except ErrorPuente as e:
        _cerrar_uso(uso_id, False)
        raise
    except Exception as e:
        _cerrar_uso(uso_id, False)
        log.exception("Fallo inesperado en /puente")
        raise ErrorPuente("proveedor_caido", mensaje=str(e)) from e

    _cerrar_uso(uso_id, True, contexto_uso)
    return envelope


@router.post("/stream", responses={401: {"model": ErrorPuenteBody}})
def puente_stream(req: PuenteRequest, x_api_key: Optional[str] = Security(api_key_scheme)):
    """Como `POST /puente`, pero la respuesta llega token a token (SSE).

    Eventos (§5): `token` por cada delta visible, `razonamiento` para el
    pensamiento (para que el cliente lo oculte), `json` una vez al terminar con
    el objeto parseado, `done` con el envelope completo y `[DONE]` al final.

    Los errores de auth y de validacion salen como HTTP normal antes de abrir el
    stream. Tambien el primer evento del generador se consume antes de abrirlo, de
    modo que un fallo temprano (413 de ventana, 429 del proveedor) conserva su
    status HTTP; los que pasan mas adelante ya van como evento `error`, porque las
    cabeceras se enviaron.
    """
    _exigir_api_key(x_api_key)
    uso_id = iniciar_uso_llm(tipo="puente")
    contexto_uso = {}

    # Un generador no ejecuta nada hasta la primera iteracion, y aqui importa:
    # sin esto, una ventana de contexto insuficiente saldria como evento `error`
    # en un 200 en vez de un 413 de verdad.
    generador = ejecutar_puente_stream(req, contexto_uso=contexto_uso)
    try:
        primer_evento = next(generador, None)
    except ErrorPuente as e:
        _cerrar_uso(uso_id, False)
        raise
    except Exception as e:
        _cerrar_uso(uso_id, False)
        log.exception("Fallo inesperado en /puente/stream")
        raise ErrorPuente("proveedor_caido", mensaje=str(e)) from e

    def event_stream():
        cerrado = False

        def cerrar(exitoso):
            nonlocal cerrado
            if cerrado:
                return
            cerrado = True
            _cerrar_uso(uso_id, exitoso, contexto_uso if exitoso else None)

        try:
            # `chain` y no una lista: el resto del stream tiene que seguir saliendo
            # token a token, no acumularse entero en memoria antes del primer yield.
            for evento in chain([primer_evento], generador) if primer_evento else generador:
                if evento.get("__done__"):
                    cerrar(True)
                    yield "data: [DONE]\n\n"
                    return
                yield f"data: {json.dumps(evento, ensure_ascii=False)}\n\n"
            cerrar(True)
            yield "data: [DONE]\n\n"
        except GeneratorExit:
            cerrar(False)
            raise
        except ErrorPuente as e:
            cerrar(False)
            log.warning("/puente/stream: %s (%s)", e.codigo, e.mensaje)
            yield f"data: {json.dumps(e.payload(), ensure_ascii=False)}\n\n"
            yield "data: [DONE]\n\n"
        except Exception as e:
            cerrar(False)
            log.exception("Fallo inesperado en /puente/stream")
            fallo = ErrorPuente("proveedor_caido", mensaje=str(e))
            yield f"data: {json.dumps(fallo.payload(), ensure_ascii=False)}\n\n"
            yield "data: [DONE]\n\n"

    return StreamingResponse(
        event_stream(),
        media_type="text/event-stream",
        headers={
            "Cache-Control": "no-cache",
            "Connection": "keep-alive",
            "X-Accel-Buffering": "no",
        },
    )

