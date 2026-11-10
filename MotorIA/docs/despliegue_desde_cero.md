# MotorIA – Guía de Despliegue desde Cero

> **Fecha:** 2025-11-03
> **Finalidad:** Levantar el motor completo en un entorno nuevo usando únicamente los archivos de backup y los scripts del proyecto.

---

## 1. Requisitos previos

| Componente | Versión / Nota |
|------------|----------------|
| **Sistema operativo** | Linux (Ubuntu 24.04 o comparable) |
| **Python** | 3.11+ (recomendado con `venv`) |
| **PostgreSQL** | 16+ y extensión `pgvector` |
| **Modelo LLM local** | Qwen2.5‑3B (`modelos/qwen.gguf`, ≈ 2 GB) |
| **Clave maestra** | `.keys/llm_master.key` (AES‑256‑GCM, permisos `600`) |
| **Token de autenticación API** | `5NyaftKXfKrdWWLiT90hYm6ideMHTaz83VTBP1FgV1g` |
| **Backups disponibles** | Carpeta `backups/` generada por `scripts/motoria.sh backup` (ver sección 4). |

### 1.1. Estructura de carpetas esperada

```
/home/climbat/MotorIA/
├─ backups/
│  ├─ motoria_2026-11-03.sql          # volcado de BD
│  ├─ qwen.gguf_2026-11-03.gguf        # modelo local
│  ├─ .env_2026-11-03                   # variables de entorno
│  └─ keys/
│     └─ llm_master.key_2026-11-03    # clave maestra (chmod 600)
├─ docs/
│  └─ despliegue_desde_cero.md        # <-- este archivo
├─ api/
├─ core/
├─ db/
├─ frontend/
├─ modelos/
├─ scripts/
├─ .env          # se sobreescribirá en el paso 3
├─ .keys/llm_master.key
├─ requirements.txt
└─ ...
```

### 1.2. Instalar dependencias

```bash
# 1. Clonar o asegurar el código
git clone https://github.com/tuusuario/MotorIA.git   # si es necesario
cd MotorIA

# 2. Crear y activar entorno virtual
python -m venv venv
source venv/bin/activate

# 3. Instalar paquetes Python
pip install -r requirements.txt
```

### 1.3. PostgreSQL y base de datos `motoria`

```bash
# Instalar PostgreSQL (si no lo tienes)
sudo apt-get update
sudo apt-get install postgresql postgresql-contrib

# Crear la base y la extensión vector
sudo -u postgres psql -c "CREATE DATABASE motoria;"
sudo -u postgres psql -d motoria -c "CREATE EXTENSION IF NOT EXISTS vector;"

# Importar esquema base (opcional, si ya tienes el dump completo salta este paso)
psql "postgresql://climbat:AGRIdigital2024@localhost:5432/motoria" -f db/schema.sql
```

### 1.4. Restaurar la base de datos desde el backup

```bash
# Supongamos que el backup está en la carpeta backups/
psql "postgresql://climbat:AGRIdigital2024@localhost:5432/motoria" \
     -f backups/motoria_2026-11-03.sql
```

> **Qué hace:**  
> * Crea todas las tablas, índices y datos (documentos, embeddings, proveedores LLM, uso‑LLM, etc.).  
> * La tabla `proveedores_llm` trae las **apikey cifradas**; no son texto plano.

### 1.5. Recuperar el modelo local Qwen2.5‑3B

```bash
# Copiar el .gguf que está en el backup a la carpeta de modelos
cp backups/qwen.gguf_2026-11-03.gguf modelos/qwen.gguf
# (Verificar tamaño ≈ 1.9 GB)
```

### 1.6. Restaurar el fichero `.env` y la clave maestra

```bash
# .env (variables de entorno)
cp backups/.env_2026-11-03 .env
# La clave maestra debe estar en .keys/llm_master.key con permisos 600
chmod 600 .keys/llm_master.key
# Si el backup tiene la key con sufijo de fecha, puedes renombrarla:
mv backups/keys/llm_master.key_2026-11-03 .keys/llm_master.key
chmod 600 .keys/llm_master.key
```

### 1.7. Insertar (o actualizar) los proveedores de LLM

El proyecto gestiona los proveedores externos mediante la tabla `proveedores_llm`. Las **apikey** están cifradas; para darlas de alta se usa el script `scripts/migrar_secretos.py`.

```bash
# 1. Verificar contenido actual de la tabla (solo para comprobar)
psql "postgresql://climbat:AGRIdigital2024@localhost:5432/motoria" \
     -c "SELECT id, nombre, apikey IS NOT NULL AS has_key FROM proveedores_llm;"

# 2. Ejecutar el script de migración de secretos (leo‑only, no modifica el sistema aquí)
#    Está diseñado para leer la clave maestra y actualizar la columna apikey.
#    Si quieres ejecutarlo manualmente, revisa su código en scripts/migrar_secretos.py.
#    Ejemplo genérico:
#    python scripts/migrar_secretos.py
```

> **Nota:** Si en tu nuevo entorno no tienes las claves maestras originales, deberás generar una nueva y cifrar manualmente cada apikey (el script muestra la fórmula AES‑256‑GCM).

### 1.8. Iniciar la API FastAPI

```bash
# Modo desarrollo (bloqueante)
uvicorn api.main:app --host 0.0.0.0 --port 8001
```

La API quedará accesible en `http://localhost:8001`. Los endpoints `/llm/*` exigen el header:

```
Authorization: Bearer 5NyaftKXfKrdWWLiT90hYm6ideMHTaz83VTBP1FgV1g
```

#### 1.8.1. Servicio systemd (opcional)

Crea el archivo `/etc/systemd/system/motoria.service`:

```ini
[Unit]
Description=MotorIA LLM API
After=network.target postgresql.service

[Service]
User=climbat
WorkingDirectory=/home/climbat/MotorIA
EnvironmentFile=/home/climbat/MotorIA/.env
ExecStart=/home/climbat/MotorIA/venv/bin/uvicorn api.main:app --host 0.0.0.0 --port 8001
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
sudo systemctl daemon-reload
sudo systemctl enable motoria
sudo systemctl start motoria
```

### 1.9. Verificación rápida

```bash
# Health check (no necesita token)
curl http://localhost:8001/api/v1/health

# Llamada protegida (incluye token)
curl -H "Authorization: Bearer 5NyaftKXfKrdWWLiT90hYm6ideMHTaz83VTBP1FgV1g" \
     -X POST http://localhost:8001/chat \
     -d '{"mensaje":"Hola, ¿cómo se define un recurso hídrico?"}'
```

Deberías recibir una respuesta generada por el modelo (según el modo `LLM_MODE` configurado).

### 1.10. Backup y restauración (recordatorio)

| Acción | Comando |
|---|---|
| **Backup diario** | ```bash pg_dump "postgresql://climbat:AGRIdigital2024@localhost:5432/motoria" > backups/motoria_$(date +%F).sql<br>cp modelos/qwen.gguf backups/qwen.gguf_$(date +%F).gguf<br>cp .env backups/.env_$(date +%F)<br>mkdir -p backups/keys<br>cp .keys/llm_master.key backups/keys/llm_master.key_$(date +%F)<br>chmod 600 backups/keys/llm_master.key_$(date +%F)``` |
| **Restauración** | ```bash psql "postgresql://climbat:AGRIdigital2024@localhost:5432/motoria" -f backups/motoria_YYYY-MM-DD.sql<br>cp backups/qwen.gguf_YYYY-MM-DD.gguf modelos/qwen.gguf<br>cp backups/.env_YYYY-MM-DD .env<br>chmod 600 backups/keys/llm_master.key_YYYY-MM-DD<br>cp backups/keys/llm_master.key_YYYY-MM-DD .keys/llm_master.key<br>systemctl restart motoria (si usa systemd)``` |

---

## 2. Seguridad y consideraciones finales

| Aspecto | Recomendación |
|---|---|
| **Clave maestra** | Guardar fuera del repositorio Git, permisos `600`, nunca commitear. |
| **Apikey de proveedores** | Están cifradas con la clave maestra; rotarlas con `scripts/migrar_secretos.py`. |
| **Token API auth** | No compartir; usarse únicamente en encabezado `Authorization: Bearer …`. |
| **Copias de seguridad** | Mantener la carpeta `backups/` en un ubicación externa (S3, NAS, disco externo). |
| **Actualizaciones** | Tras modificar `.env` o la base de datos, reiniciar el servicio (`systemctl restart motoria`). |

---

## 3. Enlaces rápidos

* **Documentación completa:** `docs/despliegue.md` (versión original, ahora en `docs_anterior/`).  
* **Scripts útiles:** `scripts/motoria.sh`, `scripts/restaurar_db.sh`, `scripts/migrar_secretos.py`.  
* **API docs (Swagger):** `http://<host>:8001/api/docs`.  

--- 

*Fin de la guía.* Copia este markdown en `docs/despliegue_desde_cero.md` (o el nombre que prefieras) y sigue los pasos en orden. Con los archivos de `backups/` y el código fuente ya tendrás MotorIA funcional en cualquier otro servidor.