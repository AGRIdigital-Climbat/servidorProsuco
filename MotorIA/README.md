# MotorIA - Asistente Juridico Bolivia

Sistema RAG (Retrieval-Augmented Generation) para busqueda de leyes, decretos, ordenanzas y normativas de Bolivia. Sube documentos PDF, el sistema los indexa con embeddings y responde preguntas juridicas con fuentes.

## Arquitectura

```
Frontend (Web)
    |
    v  REST API (FastAPI)
Backend
    |-- Embeddings (all-MiniLM-L6-v2, bajo demanda)
    |-- Busqueda hibrida inteligente (semantica + texto + intencion)
    |-- pgvector (PostgreSQL, busqueda semantica)
    |-- LLM (Qwen2.5 3B local O LLM externo OpenAI-compatible)
    |-- Sistema de verificacion de calidad
    |
    v
Respuesta con fuentes (streaming SSE)
```

## Componentes

| Componente | Modelo | Uso |
|---|---|---|
| Embeddings | `all-MiniLM-L6-v2` | Convierte texto a vectores (384 dim) |
| LLM local | `Qwen2.5-3B-Instruct` | Genera respuestas basadas en contexto (default) |
| LLM externo (opcional) | OpenAI-compatible (ej. `openai/gpt-oss-20b` en Groq) | Mismo chat via `llm_externo: true` o `LLM_MODE=external` |
| STT | `Whisper tiny` | Transcribe audio a texto (opcional) |
| Base de datos | PostgreSQL 16 + pgvector | Almacena documentos y embeddings |

## Funcionalidades

- **Frontend SPA (Vue 3)** con 5 pestanas: Consultar (chat), Buscar, Biblioteca (gestion de PDFs), **Calidad** (evaluacion) y Tipos de norma.
- **Busqueda hibrida inteligente**: combina similitud semantica (embeddings) + coincidencia textual + deteccion de intencion (definicion, procedimiento, plazo, autoridad, requisito).
- **Previsualizador de PDF**: abre el documento original en el navegador (`/documentos/{id}/descargar?inline=1`).
- **Evaluacion de calidad por documento**: `POST /documentos/{id}/verificar` genera consultas de prueba y mide en que posicion aparece el fragmento correcto.
- **Evaluacion general del sistema**: `POST /verificacion/general` prueba toda la plataforma con consultas al azar de todos los documentos y desglosa por intencion y tipo de norma (indice general 0-1).
- **Prueba del LLM**: opcionalmente genera respuestas reales y verifica que citen sus fuentes, tiempo y tokens.
- **Dos motores de LLM**: modelo local (Qwen2.5-3B, default) o externo OpenAI-compatible (Groq et al.), elegibles por peticion con `llm_externo` o globalmente con `LLM_MODE`.
- **Respuestas breves y con ortografia correcta**: `max_tokens` es un maximo de la respuesta de SALIDA, no una meta; el prompt pide respuestas concisas en espanol correcto (tildes y ñ). El margen interno de modelos que razonan se calibra automaticamente y no consume ese limite.
- **Filtro `documento_id`/`documento_ids`** (opcional): tanto `/chat` como `/busqueda` aceptan `documento_id` (un documento) y `documento_ids` (varios, incluso de distinto tipo) para restringir la busqueda; consume menos tokens de contexto y da respuestas mas precisas sobre esos documentos.

## Estructura del Proyecto

```
MotorIA/
|-- api/
|   |-- main.py              # Punto de entrada FastAPI
|   |-- routes/
|       |-- chat.py           # POST /chat (stream y normal)
|       |-- documentos.py     # CRUD documentos + subida + descarga + verificar
|       |-- busqueda.py       # POST /busqueda
|       |-- tipos.py          # CRUD tipos de norma
|       |-- verificacion.py   # POST /verificacion/general
|       |-- voz.py            # POST /voz
|-- core/
|   |-- embeddings.py         # Servicio de embeddings
|   |-- llm.py                # Servicio Qwen2.5 3B
|   |-- chunking.py           # Deteccion de articulos
|   |-- rag.py                # Pipeline RAG + busqueda hibrida inteligente
|   |-- verificacion.py       # Verificacion por documento y del sistema
|   |-- stt.py                # Speech-to-text
|-- db/
|   |-- schema.sql            # DDL de la base de datos
|   |-- connection.py         # Pool de conexiones
|   |-- queries.py            # Funciones de acceso a datos
|   |-- backup_motoria.sql    # Backup de la base de datos
|-- frontend/                 # Interfaz web (Vue 3 + Vite)
|   |-- src/components/       # ChatTab, BusquedaTab, DocumentosTab, CalidadesTab, ...
|-- scripts/
|   |-- motoria.sh            # Gestion de servicios (start/stop/url/backup)
|   |-- restaurar_db.sh       # Restaurar base de datos desde backup
|-- docs/
|   |-- despliegue.md         # Guia de despliegue y operacion diaria
|   |-- uso.md                # Guia de uso de la API y frontend
|   |-- flujo.md              # Flujogramas del sistema
|   |-- verificacion.md       # Comprobar servicios
|   |-- entrenamiento.md      # Como entrenar un modelo propio
|-- modelos/                  # Modelos descargados (NO se suben a git)
|-- .env                      # Variables de entorno (NO se sube a git)
|-- requirements.txt          # Dependencias Python
```

> **Importante:** los modelos (`modelos/*.gguf`), `.env`, `venv/` y los PDFs
> subidos no se versionan en git.

## Requisitos

- Python 3.10+
- PostgreSQL 16 con extension pgvector
- ~3GB de espacio en disco
- ~4GB de RAM disponible

## Instalacion Rapida

```bash
# Clonar repositorio
git clone https://github.com/AGRIdigital-Climbat/Motor_IA.git
cd Motor_IA

# Crear entorno virtual
python3 -m venv venv
source venv/bin/activate

# Instalar dependencias
pip install torch --index-url https://download.pytorch.org/whl/cpu
pip install -r requirements.txt

# Configurar base de datos
sudo -u postgres psql -c "CREATE USER climbat WITH PASSWORD 'AGRIdigital2024';"
sudo -u postgres psql -c "CREATE DATABASE motoria OWNER climbat;"
sudo -u postgres psql -d motoria -c "CREATE EXTENSION IF NOT EXISTS vector;"
sudo -u postgres psql -d motoria -f db/schema.sql
```

## Documentacion

- [Guia de Despliegue](docs/despliegue.md) - instalacion, servicios systemd, tunnel Cloudflare, operacion diaria y backup
- [Referencia de API](docs/API.md) - todos los endpoints, schema del chat (`llm_externo`, `max_tokens`), subida de PDF y modelos de lenguaje
- [Guia de Uso](docs/uso.md) - frontend, API REST y Swagger
- [Documentacion del Flujo](docs/flujo.md) - flujogramas del sistema, carga de documentos y estado de las IA
- [Verificacion de Servicios](docs/verificacion.md) - comprobar que los servicios corren y consultar la URL publica
- [Replicar la Web en Otro Sitio](docs/replicar-web.md) - desplegar una copia identica del frontend en otra web consumiendo la API desplegada
- [Entrenamiento de Modelos](docs/entrenamiento.md) - como llevar MotorIA a un modelo entrenado (reranker, LoRA, embeddings) usando lo ya construido
- [API Reference](http://localhost:8001/docs) (Swagger)

## Modelos de Lenguaje (LLM)

Configurables en `.env` (no versionado en git):

| Variable         | Default                              | Descripcion                                        |
|------------------|--------------------------------------|----------------------------------------------------|
| `LLM_MODE`       | `local`                              | `local` (Qwen) o `external` (OpenAI-compatible)    |
| `MODEL_PATH`     | `modelos/qwen.gguf`                  | Modelo local Qwen2.5-3B                            |
| `LLM_MAX_TOKENS` | `512`                                | Tope maximo por defecto de la respuesta de SALIDA   |
| `LLM_RAZONAMIENTO_TOKENS` | `2048`                   | Techo del margen interno para razonamiento (auto-calibrado) |
| `LLM_MASTER_KEY_FILE` | `.keys/llm_master.key`          | Clave maestra AES-256-GCM (fuera de BD, chmod 600) |
| `API_AUTH_TOKEN` | -                                    | Token para `X-API-Key` (protege `/llm/*` y escrituras) |

> La apikey de los proveedores **no** vive en `.env`: se gestiona por
> `/llm/proveedores` y se guarda **cifrada** (AES-256-GCM) en la tabla
> `proveedores_llm`; la clave maestra está en `LLM_MASTER_KEY_FILE`, nunca en la BD.

- `POST /chat` acepta `llm_externo: true|false` por peticion (sobreescribe el modo global sin cambiar config).
- `GET/POST /busqueda` (sin LLM) acepta los mismos filtros: `tipo_id`, `documento_id`, `documento_ids`, `umbral`, `limite`.
- `GET/POST /llm/modo` consulta y cambia el modo global en caliente (con persistencia en `.env` ante reinicios). Requiere `X-API-Key`.
- `GET /llm/uso/resumen` muestra llamadas y tokens gastados del motor (solo uso real de `/chat`), como `resumen_total`, `por_proveedor` (por id de `proveedores_llm`, el local como `null`) y `por_modelo`, con `llamadas_exitosas`/`llamadas_sin_exito` y tokens, filtrable por `desde`/`hasta`. Cada `/chat` se registra en `uso_llm` desde el despliegue.
- Los proveedores se gestionan por API con `/llm/proveedores` (CRUD + `POST /{id}/probar`) y se guardan en `proveedores_llm` con la clave cifrada. **Seleccion aleatoria**: de los proveedores con `estado_manual=true` y `estado_modelo=true`; si uno falla pasa a `estado_modelo=false` y se reintenta con otro.
- Un timer systemd (`motoria-verificar-proveedores.timer`, 05:00 UTC) verifica diariamente que los modelos sigan respondiendo (`bash scripts/motoria.sh proveedores` para ejecutarlo a mano).
- `max_tokens` en `/chat` limita el largo de la respuesta de SALIDA (visible; es un MAXIMO, no una meta; por defecto el modelo responde mas breve). El razonamiento interno se calibra automaticamente por proveedor y no consume ese tope.
- El motor responde **siempre en espanol, en texto plano y por parrafos**: sin Markdown, sin viñetas ni listas (instruido en el prompt y limpiado en la salida de `/chat`).
- `LLM_MODE=external` no carga la memoria Qwen en RAM.

## Operacion Diaria (produccion)

Todo el control se centraliza en `scripts/motoria.sh`:

```bash
# Iniciar todos los servicios (postgresql -> motoria -> cloudflared)
bash scripts/motoria.sh start

# Detener todos los servicios
bash scripts/motoria.sh stop

# Ver estado de los servicios
bash scripts/motoria.sh status

# Ver la URL publica asignada al tunnel Cloudflare
bash scripts/motoria.sh url

# Seguir los logs de la API
bash scripts/motoria.sh log

# Generar backup de la base de datos
bash scripts/motoria.sh backup
```

Los servicios (postgresql, motoria, cloudflared) arrancan solos al encender el servidor. La URL publica es un quick tunnel de Cloudflare y **cambia en cada reinicio**.

## Endpoints

| Metodo | Ruta | Descripcion |
|---|---|---|
| GET | `/health` | Estado del servidor |
| GET | `/tipos-documento` | Lista de tipos con jerarquia |
| POST | `/tipos-documento` | Crear tipo de norma |
| PUT | `/tipos-documento/{id}` | Editar tipo de norma |
| DELETE | `/tipos-documento/{id}` | Borrar o desactivar tipo |
| GET | `/documentos` | Listar documentos |
| GET | `/documentos/{id}` | Detalle de documento (incluye metadatos) |
| GET | `/documentos/{id}/chunks` | Listar chunks de un documento |
| POST | `/documentos` | Subir PDF (indexa con embeddings y verifica) |
| PUT | `/documentos/{id}` | Editar titulo, tipo y fecha |
| DELETE | `/documentos/{id}` | Eliminar documento |
| GET | `/documentos/{id}/descargar` | Descargar PDF original (`?inline=1` para previsualizar) |
| POST | `/documentos/{id}/verificar` | Verificacion de calidad del documento |
| POST | `/chat` | Pregunta con RAG (stream y normal) |
| GET | `/llm/modo` | Consultar el modo global del LLM (local/external) — requiere token |
| POST | `/llm/modo` | Cambiar el modo global del LLM (persistente) — requiere token |
| GET/POST | `/llm/proveedores` | CRUD de proveedores de LLM externos (claves cifradas; GETs abiertos, escrituras con token) |
| POST | `/llm/proveedores/{id}/probar` | Probar proveedor y actualizar `estado_modelo` (true/false) — requiere token |
| POST | `/busqueda` | Busqueda semantica sin LLM |
| POST | `/verificacion/general` | Evaluacion general del sistema de busqueda |
| POST | `/voz` | Audio a respuesta |

## Busqueda Hibrida Inteligente

El sistema combina tres senales para encontrar los fragmentos mas relevantes:

1. **Semantica**: similitud coseno de embeddings (all-MiniLM-L6-v2, 384 dim).
2. **Textual**: coincidencia de palabras clave en el texto.
3. **Intencion**: deteccion del tipo de consulta (definicion, procedimiento, plazo,
   autoridad, requisito, general) que ajusta los pesos de la busqueda.

La busqueda se hace por tipo de documento de mayor a menor prioridad. Solo los
resultados con similitud > umbral (default 0.30) se incluyen, ordenados por
relevancia y limitados a 15.

## Evaluacion de Calidad

El sistema puede medirse a si mismo:

- **Por documento** (`POST /documentos/{id}/verificar`): toma oraciones al azar
  del documento y verifica en que posicion del ranking aparece el fragmento
  correcto. Devuelve `precision_at_1/3/5`, similitud promedio y cada consulta
  con su resultado.
- **General** (`POST /verificacion/general`): igual pero con consultas de todos
  los documentos, desglosado por intencion y tipo de norma, con un indice general
  (`indice_general`). Opcionalmente prueba respuestas reales del LLM (que citen
  fuentes) con `{"probar_respuestas": true}`.

> **Nota de rendimiento**: probar el LLM tarda ~2 min por respuesta en este
> servidor (CPU). Usar `probar_respuestas: false` para evaluaciones rapidas.

## Seguridad

- PostgreSQL acepta conexiones desde `localhost` y desde la red local `10.10.10.0/24` (DBeaver). No se expone a internet.
- La API y el frontend se exponen al publico unicamente a traves del tunnel Cloudflare.
- Las credenciales estan en `.env` (no versionado en git).

## Licencia

Proyecto interno AGRIdigital.
