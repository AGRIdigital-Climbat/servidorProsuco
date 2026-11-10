# servidorProsuco — Monorepo Integral

> **Repositorio unificado** que agrupa tres proyectos principales del ecosistema AGRIdigital:
> **SINAGERSAT** (Sistema Nacional de Alerta Temprana), **AgriAgent** (Plataforma de agentes agrícolas auto-mejorables) y **MotorIA** (Motor de IA conversacional RAG).

---

## 📁 Estructura del Monorepo

```
servidorProsuco/
├── SINAGERSAT/                    # Sistema de Alerta Temprana y Gestión de Riesgos
│   ├── BaseData/                  # API REST FastAPI v1.2.7 + PostgreSQL/PostGIS
│   │   ├── api/                   # 21 módulos: auth, edan, rea, usuarios, shared, upload, audit
│   │   ├── database/              # 11 archivos SQL (backups, migraciones, geografía INE Bolivia)
│   │   ├── deploy/                # Scripts despliegue Ubuntu (systemd, PostgreSQL, Cloudflare Tunnel)
│   │   ├── config/                # Configuración PostgreSQL (pg_hba.conf, postgresql.conf)
│   │   ├── requirements.txt       # 9 dependencias Python
│   │   ├── start_api.sh           # Iniciar API local (puerto 8000)
│   │   ├── start_tunnel.sh        # Iniciar API + Cloudflare Quick Tunnel público
│   │   └── README.md              # Documentación completa v1.2.7
│   └── admin-panel/               # Frontend Vue 3 + Vite 5 + Tailwind CSS 3 + Pinia
│       ├── src/                   # 25+ componentes/vistas (auth, geo, rea, upload, usuarios)
│       ├── package.json           # 7 deps + 6 devDeps
│       └── README.md
│
├── AgriAgent/                     # Plataforma multiusuario de agentes agrícolas (Hermes Agent)
│   ├── docs/                      # 13 documentos técnicos + 2 en lenguaje-sencillo
│   │   ├── 00-diagnostico-y-decisiones.md
│   │   ├── 01-vision-plataforma.md
│   │   ├── 02-potencial-hermes-agent.md
│   │   ├── 03-arquitectura.md
│   │   ├── 04-base-de-datos.md
│   │   ├── 05-api-fastapi.md
│   │   ├── 06-agentes-y-auto-mejora.md
│   │   ├── 07-dispositivos-y-borde.md
│   │   ├── 08-dominios-aplicacion.md
│   │   ├── 09-seguridad-y-roles.md
│   │   ├── 10-despliegue-ubuntu.md
│   │   ├── 11-roadmap-y-verificacion.md
│   │   ├── 12-agentes-creacion-y-autoentrenamiento.md
│   │   └── lenguaje-sencillo/
│   ├── docs_simple/               # 20 docs piloto riego quinua (altiplano boliviano)
│   ├── firmware/
│   │   ├── esp32_agri/            # PlatformIO multi-modelo (9 ESP32 + cellular SIM7000/SIM7600)
│   │   └── esp32_agri_arduino/    # Variante Arduino IDE
│   ├── agro-hermes-plan.md
│   ├── INFORME_AVANCES.md
│   └── README.md (en docs/)
│
├── MotorIA/                       # Motor de IA conversacional RAG multi-proveedor
│   ├── api/                       # Backend FastAPI (chat, búsqueda, vault, providers)
│   ├── frontend/                  # Interfaz web
│   ├── core/                      # Núcleo: RAG, embeddings, vector store
│   ├── db/                        # Esquemas y migraciones PostgreSQL + pgvector
│   ├── docs/                      # Documentación de despliegue y arquitectura
│   ├── modelos/                   # Modelos locales (GGUF) — *excluidos del repo*
│   ├── scripts/                   # Utilidades: migración, evaluación, verificación
│   ├── v1/                        # Versión anterior (legacy)
│   ├── requirements.txt
│   └── README.md
│
├── .gitignore                     # Exclusiones globales (venv, node_modules, secretos, binarios)
└── README.md                      # Este archivo
```

---

## 🚀 Inicio Rápido por Proyecto

### SINAGERSAT — API + Admin Panel

```bash
# 1. API (Backend)
cd SINAGERSAT/BaseData
sudo bash deploy/desplegar_todo.sh          # Despliegue completo en Ubuntu vacío
# O manualmente:
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
bash start_api.sh                           # → http://localhost:8000/docs

# 2. Admin Panel (Frontend)
cd ../admin-panel
npm install
npm run dev                                 # → http://localhost:5173 (proxy a API:8000)
```

**Documentación:** `SINAGERSAT/BaseData/README.md` | `SINAGERSAT/admin-panel/README.md`

---

### AgriAgent — Agentes Agrícolas + Firmware ESP32

```bash
# Documentación completa
cat AgriAgent/docs/README.md

# Firmware ESP32 (PlatformIO)
cd AgriAgent/firmware/esp32_agri
pio run -e auto                             # Build auto-detect modelo
pio run -e auto -t upload                   # Flash
pio device monitor -b 115200                # Monitor serie
```

**Arquitectura clave:** El agente **SIEMPRE vive en el servidor** (Hermes Agent + FastAPI + PostgreSQL/TimescaleDB). El ESP32 es cliente HTTP "tonto" que mide, pregunta `POST /tasks` → `GET /tasks/{id}` y ejecuta relés.

**Documentación:** `AgriAgent/docs/` (técnica) | `AgriAgent/docs_simple/` (piloto quinua)

---

### MotorIA — IA Conversacional RAG

```bash
cd MotorIA
python3 -m venv venv && source venv/bin/activate
pip install -r requirements.txt
# Configurar .env con API keys (ver .env.example si existe)
python -m api.servidor                      # Iniciar servidor
```

**Características:** Multi-proveedor LLM (OpenAI, Anthropic, Ollama, etc.), Vault AES-256-GCM para API keys, RAG con pgvector, búsqueda híbrida, citas automáticas.

**Documentación:** `MotorIA/docs/` | `MotorIA/README.md`

---

## 🔧 Requisitos Comunes

| Componente | Versión mínima |
|------------|----------------|
| **Ubuntu/Debian** | 22.04+ |
| **PostgreSQL** | 15+ (SINAGERSAT), 16+ con TimescaleDB + pgvector (AgriAgent), 15+ con pgvector (MotorIA) |
| **Python** | 3.11+ |
| **Node.js** | 18+ (Admin Panel) |
| **PlatformIO** | 6.1+ (Firmware ESP32) |
| **Docker** | Opcional (para despliegue contenedorizado) |

---

## 📦 Despliegue en Producción

### SINAGERSAT (Ubuntu Server)
```bash
# Despliegue todo-en-uno (BD + API + Tunnel público opcional)
sudo bash SINAGERSAT/BaseData/deploy/desplegar_todo.sh --tunnel
# Servicios systemd: sinagersat-api, sinagersat-tunnel
# Nginx/Caddy para Admin Panel estático (dist/)
```

### AgriAgent (Ubuntu Server)
```bash
# Ver outline narrativo (sin comandos exactos)
cat AgriAgent/docs/10-despliegue-ubuntu.md
# Fases F0-F7 en AgriAgent/docs/11-roadmap-y-verificacion.md
```

### MotorIA
```bash
# Ver documentación en MotorIA/docs/
# Requiere: PostgreSQL + pgvector, claves API en vault
```

---

## 🔐 Seguridad y Secretos

- **NUNCA** commitear `.env`, claves API, contraseñas, certificados
- Usar `.env.example` o `.env.template` como plantillas
- SINAGERSAT: `JWT_SECRET` en `.env` (cambiar en producción)
- MotorIA: Vault AES-256-GCM para API keys (`MotorIA/.keys/`)
- AgriAgent: HMAC-SHA256 por dispositivo, claves rotables vía API

---

## 📚 Documentación por Proyecto

| Proyecto | Docs Técnicas | Docs Usuario/Piloto | Estado |
|----------|---------------|---------------------|--------|
| **SINAGERSAT** | `BaseData/docs/` (6 archivos) | `BaseData/README.md` | ✅ v1.2.7 Produccción |
| **AgriAgent** | `docs/` (13 + 2 sencillo) | `docs_simple/` (20 archivos) | ✅ Docs v1, Firmware listo |
| **MotorIA** | `docs/` + `README.md` | — | ✅ Funcional |

---

## 🤝 Contribución

1. Fork del repositorio
2. Rama por feature: `git checkout -b feat/nueva-funcionalidad`
3. Commits convencionales: `feat:`, `fix:`, `docs:`, `refactor:`, `chore:`
4. Pull Request con descripción clara

---

## 📄 Licencias

- **SINAGERSAT**: Uso interno AGRIdigital / Gobierno (consultar `BaseData/LICENSE` si existe)
- **AgriAgent**: MIT (ver `AgriAgent/LICENSE` o firmware)
- **MotorIA**: Propietario / Uso interno (consultar `MotorIA/LICENSE`)

---

## 🔗 Enlaces Útiles

- **SINAGERSAT API Docs** (local): `http://localhost:8000/docs`
- **AgriAgent GitHub Original**: https://github.com/AGRIdigital-Climbat/AgriAgents
- **Hermes Agent Docs**: https://hermes-agent.nousresearch.com/docs
- **PostgreSQL + TimescaleDB**: https://docs.timescale.com
- **PlatformIO ESP32**: https://docs.platformio.org/en/latest/platforms/espressif32.html

---

## 📞 Contacto

**Equipo AGRIdigital** — Desarrollo de tecnología agrícola y de gestión de riesgos en Bolivia.

> *Monorepo consolidado el 2026-11-10. Proyectos originalmente separados, unificados para despliegue y mantenimiento centralizado.*