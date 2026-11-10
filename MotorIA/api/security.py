import os
import hmac
from urllib.parse import urlparse
from dotenv import load_dotenv
from fastapi import Header, HTTPException, Request, Security
from fastapi.security import APIKeyHeader

load_dotenv()

_API_AUTH_TOKEN = os.getenv("API_AUTH_TOKEN", "")

_token_bytes = b""
if _API_AUTH_TOKEN:
    _token_bytes = _API_AUTH_TOKEN.encode("utf-8")

# Esquema visible en Swagger (botón Authorize)
api_key_scheme = APIKeyHeader(
    name="X-API-Key",
    auto_error=False,
    description=(
        "Token de autenticacion (valor de API_AUTH_TOKEN en .env). Se requiere en: "
        "/llm/modo, crear/editar/eliminar/probar proveedores y en las operaciones "
        "de escritura de /documentos y /tipos-documento. Las peticiones del mismo "
        "origen (navegador) y los endpoints de lectura no lo exigen."
    ),
)


def _token_es_valido(x_api_key) -> bool:
    if not _token_bytes or not x_api_key:
        return False
    try:
        provided = x_api_key.encode("utf-8")
    except Exception:
        return False
    return hmac.compare_digest(provided, _token_bytes)


def token_api_disponible() -> bool:
    """Hay `API_AUTH_TOKEN` configurado en el servidor.

    Lo consulta /puente para distinguir "el cliente no mando clave" de "el
    servidor no tiene clave puesta", que son dos decisiones operativas distintas.
    """
    return bool(_token_bytes)


def _mismo_origen(request: Request, origin: str) -> bool:
    try:
        origin_host = urlparse(origin).netloc
    except Exception:
        return False
    request_host = request.headers.get("host", "")
    return bool(origin_host) and origin_host == request_host


def _validar_token_api(x_api_key: str = Security(api_key_scheme)):
    """Dependency estricta: exige X-API-Key válido (tiempo constante).

    Aplicar en /llm/* (gestión de proveedores y modo).
    """
    if not _token_bytes:
        raise HTTPException(
            status_code=500,
            detail="API_AUTH_TOKEN no configurado en el servidor.",
        )
    if not _token_es_valido(x_api_key):
        raise HTTPException(status_code=401, detail="Token inválido.")


def _validar_token_api_origen(
    request: Request,
    x_api_key: str = Security(api_key_scheme),
    origin: str = Header(None),
):
    """Dependency permisiva para escrituras del SPA (documentos/tipos).

    Acepta: (a) X-API-Key válido (CLI/curl), o
           (b) petición del mismo origen (navegador sirviendo el SPA).
    Bloquea llamadas anónimas externas (sin token y sin Origin).
    """
    if _token_es_valido(x_api_key):
        return
    if origin and _mismo_origen(request, origin):
        return
    raise HTTPException(
        status_code=401,
        detail="Autenticación requerida: X-API-Key válido o petición del mismo origen.",
    )