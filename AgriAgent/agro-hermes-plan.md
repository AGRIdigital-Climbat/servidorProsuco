# Plataforma de Agentes Agrícolas Auto-Mejorables — Plan técnico

**Base:** Hermes Agent (NousResearch/hermes-agent, MIT) · PostgreSQL 18 · HTTP-only (sin MQTT)
**Fecha:** 2026-10-22 · **Estado:** plan aprobado en estructura, pendiente de decisión en §0

---

## 0. Decisiones a confirmar (default recomendado en negrita)

| # | Decisión | Default |
|---|---|---|
| 1 | Capa de datos ("datls") | **Parquet + DuckDB local** (particionado `site=/fecha=`), Postgres como registro, MinIO/S3 solo si hace falta |
| 2 | Escala | **Piloto 1-3 sitios** (1 invernadero, 1 granja, 1 casa), 20-100 dispositivos |
| 3 | Autonomía | **Autonomía con guardas deterministas** + aprobación humana solo en decisiones irreversibles |
| 4 | LLM | **MiniMax** (ya configurado) para razonamiento; modelo local pequeño (Qwen3/llama.cpp) para tareas repetitivas |
| 5 | Despliegue | **Servidor Ubuntu x86_64 (8-16 GB RAM)** + **Raspberry Pi 5** como borde; esta caja PRoot solo dev |
| 6 | Domótica | **Home Assistant + homeassistant-mcp** |

---

## 1. Investigación: qué se automatiza con agentes en cada dominio

### 1.1 Invernadero (CEA)

| Automatizable | Base técnica / evidencia |
|---|---|
| Control climático (temp, HR, CO₂, VPD, luz) | PID como base; **RL sobre simulador** (reward = rendimiento − energía − agua) |
| Riego y fertirrigación | **FAO-56 Penman-Monteith** (ET₀ → ETc = ET₀·Kc), humedad de zona de raíces, lluvia prevista |
| Diagnóstico de plagas/enfermedades | **PlantVillage** (54.306 img, 14 cultivos, 26 enfermedades), PlantDoc, YOLO/RT-DETR |
| Predicción de rendimiento | Regresión sobre histórico + clima + manejo |
| Energía | Optimización de caldera / bomba de calor / tanque de agua; auditoría de consumos |
| Registro de campo | **farmOS** (REST API, modelo log/entidad) o registro propio en Postgres |

### 1.2 Ganadería y cría

| Automatizable | Base técnica |
|---|---|
| PLF (ganado, rumia, actividad, ubicación, temperatura) | Collar/RFID + acelerómetro; celo y parto por patrones de rumia + actividad |
| Estrés térmico | THI con sensores de ambiente del establo |
| Alimentación | Consumo individual; sincronía de rumia vs. formulation |
| Salud / CCS | Registros de salud, condición corporal por cámara, CCS en leche |
| Cría y genética | Registros de pedigree, intervalos entre partos, retención/reemplazo |
| Modelo de datos | **FIWARE Smart Data Models (Livestock)** como esquema canónico |

### 1.3 Laboratorios

| Automatizable | Base técnica |
|---|---|
| Muestras y protocolos | **SENAITE / Bika-LIMS** (REST) o LIMS propio sobre Postgres |
| Control de instrumentos | **SiLA 2** (`silapy`), **OPC-UA** (`asyncua`), **SCPI** (pyserial), **AnIML** para resultados |
| Control de calidad | Reglas **Westgard** (1-3s, 2-2s, R-4s, 4-1s, 10x) sobre series de control |
| Ejecución de métodos por agente | **LAP** (Agent-to-Instrument Protocol, arXiv 2606.03755) como referencia de diseño |

### 1.4 Domótica (suelo, clima, riego, plantas, plagas)

| Elemento | Sensor / Actuador | Base |
|---|---|---|
| Suelo | Humedad capacitivo (v%), pH, EC, temperatura | ADS1115 + sondas; lectura cada 5-15 min |
| Clima | T/HR (SHT31), CO₂ (MH-Z19), luz (BH1750), viento/lluvia | Open-Meteo como respaldo |
| Riego | Electroválvula + bomba + caudalímetro | FAO-56 + histéresis + lockout por lluvia |
| Plantas | Cámara (RPi) + visión computacional | Detección de HOA, oídio, Botrytis, pulgón |
| Plagas | Trampas de feromonas con contador, trampas de color | Umbrales por plaga y cultivo |
| Hub | **Home Assistant** (REST + WS + eventos) | **homeassistant-mcp** (FastMCP, 40 tools, BM25) o plugin nativo Hermes (`ha_list_entities`, `ha_get_state`, `ha_call_service`) |
| Borde | ESP32 / RPi, firmware **HTTP-only** | HMAC-SHA256, buffer offline, OTA firmado Ed25519 |

---

## 2. Arquitectura por capas

```
ESP32 / RPi ──HTTP+HMAC──┐
Sensores-domótica ────────┤
Cámaras ───────────────────┤
Home Assistant ────────────┘        ┌────────── CAPA EDGE (Raspberry Pi 5) ──────────┐
                                     │ FastAPI ingest /v1/devices (buffer local)     │
                                     └───────────────────────┬───────────────────────┘
                                                             ▼
┌──────────────────────── SERVIDOR UBUNTU (x86_64) ─────────────────────────┐
│ API HTTP (FastAPI, :8080)                                                 │
│   ├─ /v1/devices/*   ingest + comandos (long-poll) + OTA                  │
│   ├─ /v1/agent/*     cola de tareas y decisiones ← "¿hay algo pendiente?"  │
│   ├─ /v1/ops/*       board, health, audit                                │
│   └─ /mcp            transporte MCP (mismo service layer)                 │
│                                                                            │
│ PostgreSQL 18 + TimescaleDB + PostGIS + pgvector  ← REGISTRO OFICIAL      │
│   lecturas · eventos · tareas · decisiones · comandos · guardas ·         │
│   episodios · trayectorias · datasets · evals · modelos · feedback        │
│                                                                            │
│ Lake: Parquet + DuckDB (/lake/site=*/fecha=*)  ← histórico y ML            │
│                                                                            │
│ Hermes Agent (runtime de agentes y auto-mejora)                           │
│   perfiles: agro-greenhouse · agro-livestock · agro-lab · agro-home ·     │
│             agro-ops (supervisor) · agro-trainer                           │
│   skills:   ~/.hermes/skills/agro/**  (Curator auto-refina)               │
│   cron:     pull-loop por perfil (consulta la cola cada 1 min)            │
│   MCP:      agro-registry · agro-control · agro-weather · agro-vision ·    │
│             agro-lims · homeassistant                                     │
│   memoria:  MEMORY.md / USER.md por perfil + Honcho opcional              │
│   gateway:  Telegram/Slack para decisiones humanas                         │
│                                                                            │
│ Grafana + Prometheus  |  pgvector + docs agronómicos (RAG)                │
└────────────────────────────────────────────────────────────────────────────┘
```

**Principio clave:** Postgres es la única fuente de verdad (telemetría, tareas, decisiones, auditoría).
El lake Parquet es un derivado reconstruible. Hermes Kanban **no** se usa en v1 como motor de cola
(ver §7 decisión B) — puede añadirse después como board espejo para el usuario.

---

## 3. El bucle de auto-mejora (3 niveles) — núcleo del proyecto

### Nivel 0 — Skills y memoria (sin pesos, diario)

- **Curator**: `curator.enabled: true`, `interval_hours: 168`, `stale_after_days: 14`,
  `archive_after_days: 30`, `backup.enabled: true` → `hermes curator run --background`,
  `hermes curator status|rollback`.
- Auto-refinamiento por el propio agente: tool `skill_manage`
  (acciones `create|patch|write_file|remove_file`) sobre `~/.hermes/skills/agro/**`,
  con `skills.write_approval: true` → los cambios quedan en `~/.hermes/pending/skills/`
  y se aprueban con `/skills approve <id>` (`skills.write_approval: false` = autonomía total,
  con riesgo de skill corrupta).
- Linter automático: `incident-log-shape`, `references-sprawl`, `oversized-body`.
- **Memoria**: tool `memory` (`add|patch|remove`, `target: memory|user`) sobre
  `memories/MEMORY.md` y `memories/USER.md` por perfil → personalización (cultivos, umbrales,
  presupuesto de agua, estilo de reportes). Backend externo opcional: **Honcho**
  (`hermes memory setup honcho`, `dialecticDepth`, `recallMode: hybrid`).
- **Feedback humano** (veto/corrección en dashboard o Telegram) → tabla `feedback` →
  parche inmediato de la skill correspondiente (mismo día).

### Nivel 1 — SFT / LoRA (semanal)

1. Cada corrida de agente se graba en `episodes` (prompt, tool calls, resultado, outcome, guard violations).
2. `batch_runner.py --dataset_file datasets/agro_prompts.jsonl --run_name agro_v1 --model <modelo>`
   → `trajectories.jsonl` (formato **ShareGPT**, `tool_stats`, `tool_error_counts`, filtros de calidad:
   se descartan entradas sin razonamiento y nombres de tool alucinados).
3. Curación en Postgres: solo episodios con outcome positivo, sin violación de guardas y con aprobación humana.
4. SFT/LoRA (TRL / Axolotl) → modelo local.
5. **Eval**: suite de holdout con el mismo `batch_runner` + métricas propias
   (guard violations, agua ahorrada, acierto diagnóstico, tasa de aceptación).
6. **Promotion gate**: el candidato solo se promueve si mejora las métricas clave;
   se registra en `models`/`promotions` y se sirve vía Ollama/vLLM como provider OpenAI-compatible:

   ```yaml
   providers:
     agro-local:
       api: http://127.0.0.1:11434/v1
       transport: openai_chat
       models: [{name: agro-riego-v2, context_length: 32768}]
   model: {provider: agro-local, default: agro-riego-v2}
   ```

7. Los agentes se ajustan **sin reescribir prompts a mano**: las skills son el contrato de política
   y el modelo cambia de pesos detrás.

### Nivel 2 — RL sobre simulador (opcional, mensual)

- **No está en `main`**: el código de entornos (`environments/`, `hermes_base_env.py`) vive en las
  ramas `atropos-hermes-agent` / `atropos-integrations` del repo (commit `9a14540`). Hay que pinear la rama.
- Contrato: subclase `HermesAgentBaseEnv` → `setup()`, `get_next_item()`, `format_prompt(item)`,
  `compute_reward(item, result, ctx)`, `evaluate()`; `env_config_cls = HermesAgentEnvConfig`
  (`enabled_toolsets`, `terminal_backend`, `max_agent_turns`); se sirve OpenAI-compatible
  (`serve --openai.model_name <model>`) para que lo consuma `batch_runner`.
- Entornos propios a crear: **invernadero CEA** (reward = rendimiento − energía − agua − guard violations)
  y **riego domótico** (reward = sostener humedad objetivo − agua − impacto).
- Ciclo: política → LoRA → eval en simulación → promotion gate → despliegue en hardware con
  **canario** (5 % del tráfico) y reversión automática.

### Trazabilidad de todo

`episodes → trajectories → datasets → models → promotions` en Postgres, con `skills_registry`
(hash del archivo de la skill activa) para atribuir mejoras a un cambio concreto de skill o de pesos.

---

## 4. Contrato de APIs HTTP

### 4.1 Dispositivos → servidor

| Método | Ruta | Descripción |
|---|---|---|
| POST | `/v1/devices/{id}/readings` | Lote `{ts, metrics:[{name,value,unit,quality}]}` → 202 |
| POST | `/v1/devices/{id}/events` | Eventos, heartbeat, estado de actuadores |
| GET | `/v1/devices/{id}/commands/next?wait=25` | **Long-poll**; 204 si no hay nada; `{id, action, params, deadline, sig}` |
| POST | `/v1/devices/{id}/commands/{cid}/ack` | `{status: ok\|failed, actual, error}` |
| GET | `/v1/devices/{id}/firmware/latest` | Manifiesto OTA firmado Ed25519 |

Auth por dispositivo: `Authorization: HMAC-SHA256 <device_id>:<ts>:<nonce>:<sig>` + ventana ±5 min
+ caché de nonces (anti-replay) + rotación de claves.

### 4.2 Agentes → servidor (la "pregunta si hay tarea pendiente")

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/v1/agent/tasks/next?agent=<perfil>&kinds=a,b&wait=20` | **204 si no hay tarea** · 200 con la tarea |
| POST | `/v1/agent/tasks/{id}/claim` | `{agent, lease_seconds}` — lease + idempotencia |
| POST | `/v1/agent/tasks/{id}/result` | `{status, summary, artifacts[], metrics{}}` |
| POST | `/v1/agent/tasks/{id}/fail` | `{reason, retryable}` → reintento con backoff o `blocked` |
| GET | `/v1/agent/decisions/next?agent=` | Decisión pendiente de aprobación humana |
| POST | `/v1/agent/decisions/{id}/resolve` | `{choice, rationale, approver}` |
| POST | `/v1/agent/tasks` | Crear tarea (agente o regla de dominio) |
| POST | `/v1/agent/episodes` | Grabar episodio (→ material de entrenamiento) |
| POST | `/v1/agent/feedback` | Feedback humano (aprobación, veto, corrección) |

### 4.3 Pull-loop en Hermes (implementación literal del patrón pedido)

```bash
hermes cron create --name pull-greenhouse --schedule "every 1m" \
  --prompt "GET /v1/agent/tasks/next?agent=agro-greenhouse. Si 204, responde NO_REPLY. Si hay tarea: ejecútala con las skills agro y responde con el resultado."
```

- Ticker cada 60 s (requiere `hermes serve` o `hermes gateway run`); `loops.min_interval_seconds: 30`.
- Sin tarea → `NO_REPLY` → sin spam en Telegram/Slack.
- Con tarea → skill → MCP tools → `POST .../result` → `episodes` → Nivel 0/1 del bucle.

### 4.4 Personas y observabilidad

`GET /v1/ops/board`, `/v1/ops/audit`, `/v1/ops/health`, `/metrics` (Prometheus),
dashboard de Hermes en `:9119` (`hermes serve`), gateway de mensajería para aprobaciones.

---

## 5. Esquema PostgreSQL (registro oficial)

- **Núcleo**: `site`, `zone`, `crop`, `crop_cycle`, `device`, `device_credential`, `firmware_build`,
  `sensor`, `actuator`, `actuator_state` (deseado vs real).
- **Telemetría**: `readings` (hypertable TimescaleDB + continuous aggregates hora/día), `events` (hypertable), `alerts`.
- **Control**: `commands` (id, device_id, action, params jsonb, status, deadline, acked_at, guard_id),
  `guard_policies`, `guard_evaluations` (auditoría de cada comando aceptado/rechazado).
- **Cola**: `tasks` (uuid, kind, board, priority, status `ready|running|review|blocked|done|failed`,
  assignee, input jsonb, result jsonb, lease_until, attempts, max_attempts, created_by),
  `decisions` (question, options jsonb, rationale, status `pending|approved|rejected|expired`, decided_by).
- **Auto-mejora**: `episodes`, `trajectories` (ShareGPT jsonb), `datasets`, `evals`, `eval_runs`,
  `models` (LoRA + hash de pesos), `promotions`, `skills_registry`, `feedback`, `preferences`.
- **RAG**: `docs` + `docs_embedding` (**pgvector**): fichas de cultivo, protocolos de laboratorio, manuales.
- **GIS**: PostGIS para parcelas, zonas de riego, geocodificación de sensores.
- Extensiones: `timescaledb`, `postgis`, `vector`, `pgcrypto`.
- Vistas de lectura: `v_sensor_series`, `v_daily_agg`, `v_task_backlog`, `v_agent_performance`.
- Lake: exportador Postgres → Parquet (`/lake/site=*/fecha=*`) para analítica y datasets de entrenamiento.

---

## 6. MCP (los agentes acceden a los datos por MCP)

Un solo service layer en dos transportes: REST `/v1/*` y **MCP Streamable HTTP** en `/mcp`.

| Servidor MCP | Funcionalidad | Reglas |
|---|---|---|
| `agro-registry` | Consultas de solo lectura sobre el registro | usuario Postgres **read-only**, `statement_timeout`, sin DDL |
| `agro-control` | Crear comandos de actuadores | **siempre vía guard determinista**; nunca escribe directo a `commands` |
| `agro-weather` | Clima actual y pronóstico (Open-Meteo) | caché 15 min |
| `agro-vision` | Imágenes → plaga/enfermedad + confianza | `outputSchema` + versión del model |
| `agro-lims` | Muestras, protocolos, instrumentos (SiLA/OPC-UA/SCPI), QC Westgard | lectura por defecto; escritura con aprobación |
| `homeassistant` | Entidades, estados y servicios de HA | `tools.include` restringido; `ha_call_service` solo riesgo bajo |

Buenas prácticas MCP aplicadas: descubrimiento progresivo de tools, `outputSchema` en cada tool,
filtrado por servidor (`tools.include/exclude` con globs), `lazy: true` para servidores pesados,
`discovery_concurrency: 4`.

---

## 7. Guardas deterministas (el LLM nunca decide actuar por su cuenta)

`agro-guard` (Python, invocado por `agro-control` antes de escribir en `commands`):

- Rangos: pH 5.5–6.5 · EC 1.2–2.4 dS/m · T 15–30 °C · HR 60–85 % · VPD 0.8–1.6 kPa · humedad v% por cultivo.
- Presupuesto diario de **agua** y **energía** por zona.
- Anti-oscilación: histéresis + *dwell* mínimo entre cambios de estado.
- Lockouts: lluvia prevista, viento, helada, alarma de sensor, fallo de comunicación.
- Coherencia agronómica: ET₀ FAO-56 recalculado, Kc por etapa de cultivo, caudal real vs. planificado.
- Dos modos: `advisory` (registra y sugiere) y `enforce` (bloquea).
  Default: `advisory` para sensores, `enforce` para válvulas y fertilización.
- Cada evaluación queda en `guard_evaluations` → métrica clave de eval y de la promotion gate.

### Decisión B — Kanban de Hermes: usar o no

- **v1 sin Kanban**: la cola vive en Postgres y la API `/v1/agent/*` la expone. Motivo: una sola
  fuente de verdad, SQL para analítica, y las rutas Kanban son **sin autenticación por diseño**
  (el middleware del dashboard salta `/api/plugins/`) y viven en SQLite por board.
- **Opcional fase 2**: Kanban como board espejo para el humano (write-through vía skill/plugin),
  usando el modelo de *worker lanes* (`assignee` = nombre de perfil Hermes, `kanban_complete` /
  `kanban_request_review` / `kanban_block` como terminadores, lease TTL 15 min).

---

## 8. Perfiles y skills de Hermes

**Perfiles** (`~/.hermes/profiles/<nombre>/`, invocación `hermes -p <perfil> …`):
`agro-greenhouse`, `agro-livestock`, `agro-lab`, `agro-home`, `agro-ops` (supervisor que descompone,
estilo orchestrator), `agro-trainer` (genera datasets y evals).

**Skills** (`~/.hermes/skills/agro/<dominio>/<skill>/SKILL.md`, frontmatter + referencias progresivas):
`riego-fao56` · `clima-invernadero` · `fertilizacion-ec-ph` · `plagas-enfermedades` · `ganaderia-plf` ·
`deteccion-celo-parto` · `calidad-leche` · `lab-protocolo` · `lab-qc-westgard` · `instrumento-opcua-sila` ·
`domotica-suelo` · `domotica-clima` · `anomalia-serie` · `auditoria-sensores` · `alerta-tarea` ·
`reporte-diario` · `guardia-actuacion` (documenta el guard; la ejecución es código).

Cada skill: *cuándo usarla*, *pasos*, *errores comunes*, *referencias*. El Curator la poda y la reescribe.

---

## 9. Fases

| Fase | Contenido | Entregable verificable |
|---|---|---|
| **F0** Fundaciones | `apt install libatomic1`, instalar Hermes (`install.sh --non-interactive --skip-browser`), Postgres 18 + `timescaledb/postgis/vector`, venv + FastAPI, repo, `config.yaml` base, perfil `agro-ops` | `hermes doctor` OK, `psql` con extensiones, `/v1/ops/health` responde |
| **F1** Registro + ingestión | Esquema completo, API ingest, auth HMAC, export Parquet, Grafana | 10k lecturas ingestadas y consultadas por SQL |
| **F2** Cola + primer agente | `tasks`/`decisions` API, MCP registry+control, guard, skills de riego, cron pull-loop, `episodes` | 20 tareas de riego end-to-end, 0 comandos fuera de rango |
| **F3** Domótica edge | Home Assistant + `homeassistant-mcp`, firmware ESP32 HTTP-only (suelo/clima), FAO-56 real, buffer offline, OTA | Riego automático de un bancal real + ticket de decisión humano |
| **F4** Invernadero | Clima, fertirrigación, visión de plagas, predicción de rendimiento, energía | Detección de oídio con F1 y ticket de tratamiento |
| **F5** Ganadería + laboratorio | PLF, celo/parto, leche; LIMS + instrumentos SiLA/OPC-UA + QC Westgard | Alerta de celo y de fallo de control de calidad |
| **F6** Auto-mejora | Curator + `write_approval`, feedback→skill, `batch_runner` → SFT LoRA → eval → promotion gate | Eval del LoRA > champion en holdout y modelo promovido |
| **F7** Endurecimiento | mTLS/MFA dashboard, rotación de claves, backups, unidades systemd, canario/rollback | Restore de backup probado + plan de reversión de modelo |

---

## 10. Verificación

- Unit: FAO-56 contra referencias (CIMIS), guardas, ETc por Kc, Westgard.
- Integración: roundtrip ingest→SQL→Parquet→DuckDB; lease/expiry/retry de tareas; ack de comandos; buffer offline.
- Contrato: `outputSchema` por tool MCP; `hermes mcp test <server>`; lint de skills.
- ML: eval suite con umbral de mejora mínima y reporte de guard violations por versión de modelo.
- Aceptación: sensores → tarea → decisión → acción → feedback → skill mejorada → dataset → SFT → promoción.

---

## 11. Riesgos

1. `environments/` (RL/Atropos) **no está en `main`** → usar la rama `atropos-hermes-agent` pinneada;
   el Nivel 1 (SFT) no depende de él.
2. `skills.write_approval` bloquea el auto-refinamiento sin humano → `true` en producción, `false` solo en piloto.
3. RAM de esta caja (3.7 GB): aquí solo Postgres + Hermes + inferencia pequeña; **entrenamiento en GPU externa/CI**.
4. Sin systemd en PRoot: `pg_ctlcluster` + scripts; en destino, unidades systemd reales.
5. Derivación del dato: toda lectura usada por un agente debe venir de `readings` (SQL verificable),
   nunca de memoria del modelo; el guard lo bloquea si no hay dato reciente.

---

## 12. Entorno actual verificado (2026-10-22)

- Ubuntu 26.04 LTS en **PRoot/Termux**, aarch64, 8 cores, **3.7 GiB RAM**, **12 GB disco libre**, 6 GB swap.
- **Sin systemd**, **sin Docker/podman**. Python 3.14.4/3.14.6, Node v22.22.1, npm 9.2.0, pip 25.1.1,
  git 2.53.0, curl 8.18.0. `apt-get` operativo, red OK.
- Paquetes apt disponibles: `postgresql` 18, `postgresql-18-postgis-3` 3.6.2, `postgresql-18-pgvector` 0.8.1,
  `postgresql-18-timescaledb` 2.25.1, `nginx` 1.28.3, `redis-server` 8.0.5, `python3-venv` 3.14.3,
  `build-essential` 12.12, `libpq-dev` 18.4.
- Sin `~/.hermes` todavía. `/root/.config/opencode` contiene solo config de opencode + skill `android`.
- `/mnt/sdcard` montado y funcional; `/mnt/android` vacío (no fiable).
- **Nota**: `PREFIX` no está exportado en este shell (PRoot, no Termux puro) → la instalación de Hermes
  debe usar el script POSIX, no el layout Termux `$PREFIX/lib/hermes-agent/`.

---

## 13. Primitivas de Hermes verificadas (referencia rápida)

| Primitiva | Comando / ruta | Nota |
|---|---|---|
| Instalación | `curl -fsSL https://hermes-agent.nousresearch.com/install.sh \| bash` | flags: `--non-interactive`, `--skip-browser`, `--skip-computer-use`, `--channel main`, `--dir` |
| Runtime web | `hermes serve` (dashboard/backend, puerto 9119) | necesita extra `[web]`; en Ubuntu requiere `libatomic1` |
| Gateway (mensajería) | `hermes gateway start\|run\|install\|status` | `loginctl enable-linger` para sobrevivir al logout |
| Chat no interactivo | `hermes chat -q "…"`, `--oneshot`/`-Q` | exit 0/1/130 |
| Perfiles | `hermes profile create <n> --clone` · `hermes -p <n>` | dir `~/.hermes/profiles/<n>/`; description en `profile.yaml` |
| Cron | `hermes cron create --schedule "every 1m" --prompt "…"` | jobs en `~/.hermes/cron/jobs.json`; ticker 60 s; tool `cronjob_manage` |
| Loops | `/loop [interval] <prompt> [--times N]` | `loops.min_interval_seconds: 30`; backoff autoregulado; `LOOP_COMPLETE` |
| Heartbeats | `/heartbeat every 10m <prompt>` | solo idle; `NO_REPLY`/`[SILENT]` suprimen entrega |
| Goals | `/goal <text>`, `/goal show\|pause\|resume` | judge `goal_judge`; veredicto `done\|blocked\|continue\|wait` |
| Delegación | tool `delegate_task(goal, context, tasks[], background, role)` | `steer_subagent`, `interrupt_subagent`; máx. 10 hijos |
| Skills | `~/.hermes/skills/<ns>/<skill>/SKILL.md` | tool `skill_manage` (create/patch/write_file/remove_file); staging en `~/.hermes/pending/skills/` |
| Aprobación de skills | `skills.write_approval: true` + `/skills pending\|diff\|approve\|reject` | `skills.ledger`, `skills.guard_agent_created` |
| Curator | `hermes curator status\|run [--background]\|backup\|rollback` | `curator.enabled`, `interval_hours: 168`, `stale_after_days: 14`; backups en `~/.hermes/skills/.curator_backups/` |
| Memoria | tool `memory` (`add\|patch\|remove`, `target: memory\|user`) | `memories/MEMORY.md` + `memories/USER.md`; `memory.write_approval`; Honcho opcional |
| MCP | `mcp_servers:` en `~/.hermes/config.yaml` · `hermes mcp add --url …` · `hermes mcp test <n>` | tools `mcp_<server>_<tool>`; stdio o HTTP; OAuth en `~/.hermes/mcp-tokens/` |
| Providers custom | `providers: {mi-gateway: {api, transport: openai_chat, models: […]}}` + `model: {provider, default}` | permite servir el LoRA propio (Ollama/vLLM) |
| Batch / datos de entrenamiento | `python batch_runner.py --dataset_file … --run_name … --model …` | salida `trajectories.jsonl` (ShareGPT) + `statistics.json`; `--resume` por contenido |
| RL / Atropos | **NO está en `main`** | ramas `atropos-hermes-agent` / `atropos-integrations`; contrato `HermesAgentBaseEnv` (`compute_reward`) |
| Kanban (opcional) | `/api/plugins/kanban/*` · tools `kanban_*` | sin auth por diseño; SQLite por board (`HERMES_KANBAN_DB`); worker lanes por perfil |
| Home Assistant | plugin `homeassistant` del catálogo Hermes | `ha_list_entities`, `ha_get_state`, `ha_call_service`; delivery de cron a `homeassistant` |
