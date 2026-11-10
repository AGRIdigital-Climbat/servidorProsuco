from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse
from fastapi.staticfiles import StaticFiles
from api.routes import chat, documentos, busqueda, tipos, voz, verificacion, llm, grupos, puente
from core.puente import ErrorPuente
from core.llm import LLM_MODE
from core.embeddings import generar_embedding
import os

app = FastAPI(
    title="MotorIA - Asistente Juridico",
    description=(
        "Sistema RAG para busqueda de leyes y normativas de Bolivia.\n\n"
        "## Como usar\n\n"
        "- `POST /chat` - pregunta con RAG en streaming (SSE). Campo **llm_externo**: "
        "`true` usa el modelo externo (OpenAI-compatible, ej. Groq), `false` usa el "
        "modelo local Qwen2.5-3B y omitirlo usa el modo global (`LLM_MODE`).\n"
        "- `GET/POST /llm/modo` - consultar o cambiar el modo global del LLM "
        "(local/external), con persistencia en `.env`.\n"
        "- `GET/POST/PUT/DELETE /llm/proveedores` - CRUD de proveedores de LLM "
        "externos (apikey cifrada AES-256-GCM; seleccion aleatoria + verificacion "
        "diaria).\n"
        "- `GET /llm/uso/resumen` - llamadas y tokens gastados (solo `/chat`) "
        "con `resumen_total`, `por_proveedor` (por id de `proveedores_llm`, el "
        "local como `null`) y `por_modelo`, incluyendo exitosas/sin exito; "
        "filtra por `desde`/`hasta` (YYYY-MM-DD).\n"
        "- `POST /busqueda` - busqueda semantica sin LLM.\n"
        "- `GET/POST /tipos-documento`, `GET/POST/PUT/DELETE /documentos` - gestion de "
        "tipos de norma y documentos indexados.\n"
        "- `GET/POST/PUT/DELETE /grupos` - grupos publicos de documentos. En `POST /chat` "
        "se puede enviar `grupo_id` para limitar la busqueda a los documentos del grupo "
        "(se combina con `documento_id`/`documento_ids`). Si ademas se envia `termino` "
        "(nombre de seccion/pantalla, p.ej. EDAN) la busqueda se acota primero a esa "
        "seccion dentro del grupo y luego busca la pregunta en ella.\n"
        "- `POST /documentos/{id}/verificar` y `POST /verificacion/general` - evaluacion "
        "de calidad de la busqueda.\n"
        "- `POST /voz` - audio a respuesta (STT Whisper).\n\n"
        "## Autenticacion (`API_AUTH_TOKEN`)\n\n"
        "El token se define en `.env` (`API_AUTH_TOKEN`) y se envia como header "
        "`X-API-Key: <token>` (boton **Authorize** para probarlo en Swagger). "
        "Tambien se puede configurar por peticion.\n\n"
        "Para las operaciones de **documentos/tipos** tambien se aceptan peticiones "
        "del **mismo origen** (navegador que sirve el frontend), por lo que la web no "
        "necesita token.\n\n"
        "Ejemplo:\n"
        "```bash\n"
        "curl -X POST <URL>/llm/proveedores \\\n"
        "  -H 'Content-Type: application/json' \\\n"
        "  -H 'X-API-Key: TU_API_AUTH_TOKEN' \\\n"
        "  -d '{\"nombre\":\"Groq\",\"base_url\":\"https://api.groq.com/openai/v1\",\"apikey\":\"CLAVE\",\"modelo_base\":\"openai/gpt-oss-20b\"}'\n"
        "```\n\n"
        "Documentacion completa de uso en el README del proyecto."
    ),
    version="2.3.0",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(chat.router)
app.include_router(documentos.router)
app.include_router(busqueda.router)
app.include_router(tipos.router)
app.include_router(voz.router)
app.include_router(verificacion.router)
app.include_router(llm.router)
app.include_router(grupos.router)
app.include_router(puente.router)


@app.exception_handler(ErrorPuente)
def _manejar_error_puente(request: Request, exc: ErrorPuente):
    """Los errores de /puente salen con el cuerpo de §6, no con `detail`.

    Se registra a nivel de app (no del router) porque `ErrorPuente` solo lo
    levanta esa ruta, y asi el cliente recibe siempre el mismo shape.
    """
    return JSONResponse(status_code=exc.http, content=exc.payload())


@app.on_event("startup")
def startup():
    print("Iniciando MotorIA v2.0...")
    print("Cargando modelo de embeddings...")
    generar_embedding("init")
    print("Modelo de embeddings cargado.")
    if LLM_MODE == "external":
        print("LLM_MODE=external: modelo local Qwen2.5 desactivado (no se carga).")
    else:
        from core.llm import cargar_modelo
        print("Cargando modelo de lenguaje...")
        cargar_modelo()
        print("Todos los modelos cargados. Listo.")


@app.get("/health")
def health():
    return {"status": "ok", "version": "2.3.0"}


frontend_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "frontend")
assets_dir = None
index_html = None
for subdir in ("dist", ""):
    candidate = os.path.join(frontend_dir, subdir)
    if os.path.isdir(candidate):
        assets_dir = os.path.join(candidate, "assets")
        index_html = os.path.join(candidate, "index.html")
        break

if assets_dir and os.path.isdir(assets_dir):
    app.mount("/assets", StaticFiles(directory=assets_dir), name="assets")

if index_html and os.path.isfile(index_html):
    @app.get("/{path:path}", include_in_schema=False)
    def spa_fallback(path: str):
        if path.startswith("api/"):
            return JSONResponse({"detail": "Not Found"}, status_code=404)
        return FileResponse(index_html)
