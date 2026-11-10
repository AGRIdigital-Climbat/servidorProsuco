# 00 — Diagnóstico y decisiones

**Fecha:** 2026-10-22 · **Verificación:** doc oficial de Hermes Agent consultada el mismo día.

---

## 1. Diagnóstico: ¿qué base de datos es "compatible con Hermes Agent"?

### 1.1 Qué usa Hermes internamente

Hermes Agent guarda **su propio estado** en SQLite, en su directorio de perfil (`~/.hermes/`):

| Archivo | Contenido | Regla de uso |
|---|---|---|
| `state.db` | Sesiones, mensajes, enrutamiento del gateway | Esquema **interno** que cambia entre versiones. La doc oficial dice textualmente: *no lo consultes desde scripts externos — apunta a tu propia base de datos*. |
| `cron/executions.db` | Libro mayor de intentos de ejecución de cron (estados `claimed/running/completed/failed/unknown`) | Auditoría interna; solo consumible con `hermes cron runs`. |
| `cron/jobs.json` | Trabajos programados | Legible, pero propiedad de Hermes. |
| `skills/`, `memories/` | Skills y memoria en Markdown | El mecanismo natural de auto-mejora. |

**Conclusión:** no existe una BD externa que Hermes "exija". La compatibilidad real se logra así:

```
┌─────────────────────────────┐     ┌──────────────────────────────────────┐
│  Hermes Agent               │     │  Plataforma AgriAgents               │
│  state.db (SQLite interno)  │     │  PostgreSQL 18 + TimescaleDB         │
│  — no se toca —             │────▶│  ← ÚNICA FUENTE DE VERDAD de         │
│  consulta vía HTTP/scripts  │     │    telemetría, usuarios, agentes,    │
│  sobre nuestra API          │     │    límites y métricas                │
└─────────────────────────────┘     └──────────────────────────────────────┘
```

Los cron jobs de Hermes leen **nuestra** base (por script Python que hace HTTP a nuestra API, o
directamente por `psql` con usuario de solo lectura) — patrón validado en la doc oficial de cron
(ejemplo con `sqlite3`/`psql` en los *pre-run gates*).

### 1.2 Por qué PostgreSQL 18 + TimescaleDB para la plataforma

| Criterio | PostgreSQL + TimescaleDB | SQLite |
|---|---|---|
| Multiusuario concurrente (N equipos subiendo lotes + N agentes consultando) | ✅ Nativo | ⚠️ Escritor único |
| Series temporales grandes (lecturas cada 5 min × cientos de equipos) | ✅ Hypertables + agregados continuos (hora/día automáticos) | ❌ Hay que implementarlo a mano |
| JSONB para **columnas dinámicas por modelo de equipo** | ✅ Índices GIN, consultas SQL directas | ⚠️ JSON1 sin índices comparables |
| pgvector para RAG agronómico | ✅ Extensión | ❌ |
| PostGIS (parcelas, zonas de riego) | ✅ Extensión | ❌ |
| Despliegue en Ubuntu Server con apt (`postgresql-18-timescaledb`) | ✅ | ✅ |

**Decisión confirmada:** PostgreSQL 18 + TimescaleDB (+ PostGIS + pgvector) como registro oficial.
Hermes conserva su SQLite interno intacto.

---

## 2. Gaps detectados respecto a los requisitos multiusuario

El diseño base cubría arquitectura, telemetría, colas y auto-mejora, pero **no detallaba** estas piezas, que ahora sí están documentadas en `docs/`:

| # | Gap | Dónde queda documentado |
|---|---|---|
| 1 | **Usuarios y roles** (`users`, roles `admin`/`cliente`, API keys) | `04-base-de-datos.md`, `09-seguridad-y-roles.md` |
| 2 | **Catálogo de modelos de equipo** (`device_models` + `device_model_metrics`): cada modelo declara sus métricas (nombre, unidad, rango válido) → permite validar el JSONB dinámico | `04-base-de-datos.md` |
| 3 | **Registro de agentes** (`agents`): N agentes por equipo/modelo, cada uno con perfil Hermes propio | `04`, `06` |
| 4 | **Límites manuales por agente vía API** (`agent_limits`): umbrales, presupuestos y acciones permitidas ajustables por usuario | `04`, `05`, `06` |
| 5 | **Métricas de mejora del agente visibles** (vistas `v_agent_performance*`, historial de versiones de skill/modelo) | `04`, `06` |
| 6 | **Agente de alerta de heladas** con tool de pronóstico como ejemplo completo | `06` |
| 7 | **Reparto de inteligencia servidor↔microcontrolador** y fallback por comportamiento histórico | `01`, `07` |

---

## 3. Decisiones confirmadas

| # | Decisión | Elección |
|---|---|---|
| 1 | BD de la plataforma | **PostgreSQL 18 + TimescaleDB** (+ PostGIS, pgvector). Hermes mantiene su SQLite interno. |
| 2 | Alcance de la documentación | **Incluir los gaps multiusuario** (users, device_models, agents, agent_limits, métricas de mejora) como parte del diseño. |
| 3 | Idioma | **Español** en toda la carpeta `docs/`. |
| 4 | Despliegue Ubuntu | **Solo outline** (`10-despliegue-ubuntu.md`), sin comandos todavía. |
| 5 | Comunicación equipo↔servidor | **HTTP + long-poll**, sin MQTT (dispositivos celulares únicos, SIM800/SIM7000). |
| 6 | Esquema de telemetría | **JSONB dinámico validado contra el catálogo del modelo** del equipo. |
| 7 | Motor de agentes | **Hermes Agent** (perfiles, skills, cron, MCP, auto-mejora). |
| 8 | API de la plataforma | **FastAPI** (Python), versionada en `/v1/`. |

## 4. Aclaración sobre el LLM (MiniMax): ¿es obligatorio? ¿es gratis?

> **Pregunta:** "¿El uso de MiniMax es una IA gratuita? ¿Se puede reemplazar con una IA que usemos
> posteriormente? ¿Es sí o sí para Hermes Agent?"

**Respuesta corta:** **NO es obligatorio, NO es "la IA de Hermes", y NO es gratis.** Es **solo uno
de los muchos "providers" (proveedores de modelos) que Hermes Agent soporta**, y se puede
**reemplazar en cualquier momento** por el que uses después — sin tocar el código de la plataforma.

### 4.1 ¿Qué es MiniMax en este contexto?

| Punto | Realidad |
|---|---|
| ¿Es parte de Hermes? | **No.** Hermes es un *runtime* de agentes (MIT, de Nous Research). MiniMax es una **empresa de modelos de IA** (china) cuyos modelos (p. ej. `MiniMax-M2.7`) Hermes puede usar como **cerebro** vía API. |
| ¿Es gratis? | **No es gratuito por defecto.** Tiene planes de pago por uso. Hermes permite entrar vía OAuth de MiniMax o con API key. No hay que confundirlo con *Nous Portal* (que sí tiene un modelo gratuito temporal como Nemotron). |
| ¿Por qué aparece en el plan? | Porque en el entorno del proyecto **MiniMax ya estaba configurado como provider**; es un **default práctico**, no una dependencia. |
| ¿Hermes funciona sin él? | **Sí, perfectamente.** Hermes funciona con cualquier provider compatible (ver tabla 4.3). |

### 4.2 ¿Se puede reemplazar después? — **Sí, y sin reescribir nada**

Hermes resuelve el modelo en tiempo de ejecución. Cambiar de cerebro es un comando/config, no un
cambio de código:

```bash
hermes model            # ver el modelo actual
hermes model            # cambiar de provider/modelo en caliente
# o en ~/.hermes/config.yaml:
# model: {provider: openai, default: gpt-4o-mini}
```

Cada **perfil** de agente (`agro-riego`, `agro-heladas`, …) puede usar un modelo **distinto**, y
cada **cron job** puede pinear su propio modelo y *reasoning effort*. La plataforma AgriAgents
guarda en `agents.model_pin` qué modelo usa cada agente → cambiarlo es un `PATCH` por API.

### 4.3 Providers que soporta Hermes (todos válidos)

| Provider | Tipo | Costo típico | Nota |
|---|---|---|---|
| **Nous Portal** | Cloud (suscripción) | Pago (con algunos modelos gratuitos temporales) | El recomendado por Hermes; incluye Tool Gateway |
| **OpenRouter** | Cloud (agregador) | Pago por uso | Acceso a cientos de modelos; provider routing por costo/velocidad |
| **OpenAI** | Cloud | Pago | GPT-4o/mini, etc. |
| **Anthropic** | Cloud | Pago | Claude |
| **Google** | Cloud | Pago | Gemini (AI Studio / Vertex) |
| **MiniMax** | Cloud | Pago | `MiniMax-M2.7`; OAuth o API key |
| **Amazon Bedrock / Microsoft Foundry** | Cloud empresarial | Pago | Enterprise |
| **Modelo local (Ollama / llama.cpp / vLLM)** | En tu servidor | **Gratis** (solo electricidad) | Nada sale de tu máquina; ideal para tareas repetitivas y para servir **tu propio LoRA** `agro-*` |

### 4.4 Estrategia recomendada para AgriAgents (barata y sin dependencia)

| Tarea | Modelo sugerido | Por qué |
|---|---|---|
| Pull-loop (preguntar si hay tarea) | **Script `no_agent`** (sin LLM) o modelo local pequeño | Cero costo cuando no hay nada que hacer |
| Clasificación / rutinas repetitivas | **Modelo local** (Ollama, p. ej. Qwen pequeño) | Gratis, rápido, sin salir del servidor |
| Razonamiento complejo de decisión | **Cloud** (MiniMax, OpenAI, Anthropic, el que esté disponible) | Más capacidad cuando importa |
| Diagnóstico de plagas (visión) | Modelo multimodal (cloud o local vision) | Requiere visión |
| **Nuestro LoRA `agro-riego-v2`** | **Local (Ollama/vLLM)** | Es nuestro; gratis de servir; sin dependencia externa |

> **Conclusión:** el diseño es **agnóstico al LLM**. MiniMax es el *default actual* (porque ya
> estaba configurado), pero **puedes cambiar a cualquier otro proveedor —incluido un modelo local
> gratuito— cuando quieras**, sin tocar la arquitectura. De hecho, la meta del Nivel 1 de
> auto-mejora es **entrenar tu propio modelo** y dejar de depender del cloud.

---

## 5. Decisiones del plan original aún abiertas (con defaults recomendados)

| # | Decisión | Default recomendado |
|---|---|---|
| 1 | Capa de datos analítica | **Parquet + DuckDB local** particionado (`site=/fecha=`); Postgres como registro; MinIO/S3 solo si hace falta |
| 2 | Escala inicial | **Piloto 1–3 sitios** (1 invernadero, 1 granja, 1 casa), 20–100 dispositivos |
| 3 | Autonomía de los agentes | **Autonomía con guardas deterministas**; aprobación humana solo en decisiones irreversibles |
| 4 | LLM de razonamiento | **Agnóstico**: default actual **MiniMax** (ya configurado); **reemplazable** por OpenAI/Anthropic/Google/OpenRouter o **modelo local (Ollama)** — ver sección 4. El LoRA propio `agro-*` se sirve local |
| 5 | Despliegue | **Servidor Ubuntu x86_64 (8–16 GB RAM)** + Raspberry Pi 5 como borde |
| 6 | Domótica | **Home Assistant** + plugin `homeassistant` de Hermes / `homeassistant-mcp` |

## 6. Referencias verificadas (2026-10-22)

- Docs oficiales: <https://hermes-agent.nousresearch.com/docs>
- Configuración / database (SQLite interno): <https://hermes-agent.nousresearch.com/docs/user-guide/configuration>
- Cron (patrón de gate script sobre BD propia): <https://hermes-agent.nousresearch.com/docs/user-guide/features/cron>
- API server / integración programática: <https://hermes-agent.nousresearch.com/docs/developer-guide/programmatic-integration>
- Repo: <https://github.com/NousResearch/hermes-agent> (licencia MIT)
