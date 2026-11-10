# 16 — Crear agentes desde cero: manual + entorno visual + comandos

> **Los agentes no los crea la IA: los creas tú.** Este documento es la guía autocontenido
> para crear un agente nuevo (riego, alertas, enfermedades, producción — o cualquier rol
> del [catálogo](18-catalogo-agentes-agricolas.md), o uno **personalizado desde cero**)
> **desde cero**, en tres formas de hacerlo: (A) el manual paso a paso con **comandos
> reales de Hermes**, (B) el **entorno visual** (Profile Builder de Hermes + nuestro
> AgriAgents Builder), y (C) cómo **probarlo** antes de que trabaje en serio.

---

## Parte A — Manual desde cero (7 pasos, con comandos)

### Requisitos (paso 0)

- Ubuntu con Hermes Agent instalado y funcionando (ver [02](02-despliegue-ubuntu.md)).
- La carpeta de perfiles: `~/.hermes/profiles/` (Hermes crea una **subcarpeta por agente**
  — ese es el "puesto de trabajo" completo: `config.yaml`, `.env`, `SOUL.md`, skills,
  cron, memoria `state.db`).

### Paso 1 — Elegir el rol del agente

| Si tu agente es… | Nombre sugerido | Dominio (doc 15) | ¿Comanda equipo? |
|---|---|---|---|
| Riego | `agro-riego` | Riego (FAO-56) | Sí (`agro-control`) |
| Alertas | `agro-alertas` | Helada/calor/sequía/tanque/sonda | No (alertas + defer) |
| Enfermedades | `agro-enfermedades` | Mildiu/roya/chinche | No (solo inspección) |
| Producción | `agro-produccion` | Biomasa/rendimiento | No (solo lectura) |

> El catálogo completo (16 agentes agrícolas con checkboxes, estados y paquetes) está en
> **[18](18-catalogo-agentes-agricolas.md)**. Los 4 de arriba son los plantilla del piloto;
> si necesitas otro rol nuevo, este manual sirve igual (elige "Personalizado" en el wizard
> o escribe su perfil a mano con los mismos 7 pasos).

### Paso 2 — Crear el perfil

```bash
# Opción CLI (respaldo completo)
hermes profile create agro-enfermedades \
  --description "Detecta riesgo de mildiu, roya y chinche en quinua; emite alertas de inspección."
agro-enfermedades setup          # claves de API + modelo (interactivo)
agro-enfermedades config set model openai/gpt-4o-mini   # o el modelo elegido
```

> Equivalente visual: el **Profile Builder** del dashboard (Parte B §1) hace estos tres
> comandos con botones.

### Paso 3 — Escribir el `SOUL.md` (identidad y conducta)

`~/.hermes/profiles/agro-enfermedades/SOUL.md` — **plantilla completa copiable:**

```markdown
# SOUL — agro-enfermedades

## Quién soy
Soy el agente de sanidad vegetal del piloto de quinua (altiplano boliviano, 1.0 ha).
Mi trabajo: detectar RIESGO de enfermedades (mildiu, roya, manchas foliares) y plagas
(chinche Nysius) a partir de datos del campo y del clima, y emitir alertas de INSPECCIÓN
al técnico. NO diagnostico a distancia: sugiero qué revisar y cuándo.

## Qué hago
1. Cada 08:00 y 20:00 leo humedad_aire, temp_aire, lluvia reciente y rocío.
2. Aplico la skill `riesgo-mildiu-quinua` (umbrales con fuente citada).
3. Si riesgo alto → alerta [ENF] con: fase del cultivo, condiciones exactas, qué revisar.
4. Si hay riego activo y riesgo alto → lo indico a agro-riego (evitar follaje).

## Lo que NUNCA hago
- Nunca aplico productos fitosanitarios ni doy dosis: solo inspección.
- Nunca doy diagnóstico sin datos (si los datos son viejos > 6 h, lo digo).
- Nunca ejecuto comandos de riego ni relés (no tengo publish_command).
- Siempre cito números: humedad, temp, días húmedos, fase (GDD) y su fuente.

## Tono
Directo, numerado, sin alarmismo: "riesgo ALTO: 4 días húmedos (>80 %), temp 12–17 °C,
revisar hojas inferiores mañana".
```

### Paso 4 — Crear la skill (el procedimiento con fuentes)

`~/.hermes/skills/agro/enfermedades/riesgo-mildiu-quinua/SKILL.md` — **plantilla completa copiable:**

```markdown
---
name: riesgo-mildiu-quinua
description: Evalúa riesgo de mildiu (Peronospora variabilis), roya y chinche Nysius
  en quinua del altiplano con datos de humedad, temperatura y lluvia.
---

# Riesgo de enfermedades — quinua

## Umbrales (con fuente)
- **Mildiu (Peronospora variabilis):** favor = humedad_aire > 80 % durante ≥ 3 días
  CONTEMP 10–20 °C y lluvia reciente o rocío. Riesgo ALTO si se cumplen los 3.
  BAJA si ≥ 3 días secos y soleados. Fuente: CIP/Plantwise; Chávez-Centeno et al. 2024;
  Danielsen & Ames 2022 (pérdidas hasta 90 % sin manejo).
- **Royas / manchas foliares:** mismo patrón húmedo, pero vigilar con > 5 días húmedos.
  Fuente: guía FAO/CIP de plagas y enfermedades de quinua.
- **Chinche (Nysius spp.):** ataque en sequía y floración; vigilar si ≥ 2 días secos
  con temp > 18 °C y fase floración. Fuente: Cruces et al. 2024 (IPM quinua).

## Procedimiento
1. Leer: humedad_aire y temp_aire últimas 72 h (series), lluvia 7 d, fase actual (GDD).
2. Contar días húmedos (>80 %) y días secos consecutivos.
3. Clasificar: ALTO / MEDIO / BAJO según umbrales de arriba.
4. En fase floración (GDD 420–700): subir un nivel la clasificación.
5. Salida: etiqueta [ENF] + clasificación + números + qué revisar (hojas inferiores,
   canopeo, muestreo de chinche en panículas) + cuándo repetir (48 h si ALTO).

## Límites
- Máximo 2 alertas ALTO por semana (si sigue ALTO, informar tendencia, no repetir).
- Datos con > 6 h de antigüedad → clasificación "sin datos frescos".
```

**Registrar la skill en el kit del agente** (permiso mínimo — solo este agente la ve):

```bash
# El kit se define en la config del perfil (tools/include y skills del perfil).
# Verificación: que la skill aparezca en la lista del agente
agro-enfermedades skills list
```

### Paso 5 — Herramientas (MCP): Agrisignal + agro-control

```bash
# Servidor MCP de clima/suelo (gratis, sin API keys)
hermes mcp add agrisignal -- npx -y agrisignal-mcp
hermes mcp test agrisignal

# Servidor MCP propio (nuestra API: leer/decidir/comandar)
hermes mcp add agro-control -- python /opt/agriagents/mcp/agro_control.py
hermes mcp test agro-control
```

**Filtrar por agente** (`~/.hermes/profiles/agro-enfermedades/config.yaml`) — este agente
**no** debe poder comandar riego:

```yaml
tools:
  include:
    - "agrisignal:*"            # todo el clima/suelo
    - "agro-control:read_*"     # solo lecturas (no publish_command)
    - "agro-control:record_*"   # puede registrar su alerta
  exclude:
    - "agro-control:publish_command"
```

> Los 4 agentes usan el mismo patrón con `include` distinto: riego incluye
> `publish_command`; producción solo `read_*` (ver doc 15 §3).

### Paso 6 — Automatización (cron / pull-loop)

```bash
# Ejemplo: dos veces al día (08:00 y 20:00)
agro-enfermedades cron add --schedule "0 8,20 * * *" \
  --prompt "Evalúa riesgo de enfermedades con la skill riesgo-mildiu-quinua. Si no hay novedad, responde NO_REPLY."
```

| Agente | Cron (recordatorio, doc 15 §3) |
|---|---|
| `agro-riego` | c/60 min 06:00–20:00; c/3 h noche; +45 min post-riego |
| `agro-alertas` | 05:00 y 17:30 + webhook `temp_aire < 2 °C` |
| `agro-enfermedades` | 08:00 y 20:00 |
| `agro-produccion` | lunes 07:00 (semanal) |

> **Regla de oro:** todo cron llega primero a `GET /v1/agent/tasks/next`; si la API responde
> `204` → el agente responde `NO_REPLY` **sin gastar LLM** (doc 11 §1.2).

### Paso 7 — Probar antes de que trabaje

Checklist (detalle en [08](08-prueba-y-personalizacion.md)):

1. **Tarea sintética:** inyectar lecturas de un escenario (p. ej. humedad 92 % por 3 días)
   y verificar que el agente responde `[ENF] ALTO` con números.
2. **Verificar trazabilidad:** la ejecución quedó en `agent_executions` con sus insumos
   (doc 17 §4 — **esto es lo que permite medirlo en el tiempo**).
3. **Verificar límites:** que NO ejecute `publish_command` (probarlo le pidiendo "abre la
   válvula" → debe negarse).
4. **`hermes mcp test`** en verde para cada servidor registrado.
5. **Costo:** confirmar `NO_REPLY` cuando no hay novedad.

### Errores comunes

| Error | Síntoma | Solución |
|---|---|---|
| Skill sin `description` | El agente nunca la usa | La descripción es lo que el LLM lee para decidir si la carga |
| `include` demasiado ancho | Agente con poder de más | Permiso mínimo por agente (tabla doc 15 §3) |
| Cron sin `NO_REPLY` | Cuenta LLM todo el día | Siempre pasar por `agent/tasks/next` |
| SOUL sin límites | El agente improvisa | Sección "Lo que NUNCA hago" en todo SOUL |
| Datos viejos | Decisiones con lote de ayer | Verificar `age_s` antes de clasificar |

---

## Parte B — Entorno visual (la forma familiar)

### B.1 El Profile Builder de Hermes (ya existe, sin instalar nada extra)

Hermes incluye desde junio de 2026 un **asistente visual** en su dashboard web:

```bash
pip install 'hermes-agent[web]'    # el extra web trae el dashboard
hermes dashboard                   # abre http://127.0.0.1:9119
```

**Flujo con botones:** nombre/descripción → modelo/proveedor (OpenRouter, Ollama,
OpenAI-compatible…) → skills integradas (toggles) → instalar skills del catálogo →
agregar servidores MCP (URL o comando) → **"Crear"**. Escribe los mismos archivos que la
CLI (`config.yaml`, `.env`, `SOUL.md`).

| Paso del builder | Comando equivalente |
|---|---|
| Campo de nombre | `hermes profile create <nombre>` |
| Campo descripción | `hermes profile describe "…"` |
| Selector de modelo | `<perfil> config set model <id>` |
| Toggles de skills | `<perfil> skills list` / toggle |
| Catálogo Skills Hub | `<perfil> skills install <slug>` |
| Servidores MCP | editar `mcp_servers` / `<perfil> mcp install` |

**Qué NO cubre** (y por qué sigue haciendo falta lo siguiente):
- No escribe el **contenido** del `SOUL.md` (solo el nombre/descripción).
- No trae **plantillas de dominio agrícola** (riego/helada/enfermedades/producción).
- No programa **crons** ni define el pull-loop.
- No tiene **prueba con datos simulados** ni métricas.

### B.2 AgriAgents Builder — nuestra extensión (el diseño)

Una pestaña **"Agentes Agri"** dentro del mismo dashboard (o en nuestro panel), que
cierra los huecos del builder nativo. **Diseño de asistente con botones (wizard):**

| Pantalla | Contenido | Qué escribe |
|---|---|---|
| **1. Catálogo** | Checkboxes de los 16 agentes del [catálogo 18](18-catalogo-agentes-agricolas.md) con paquetes preseleccionados (Piloto mínimo / Piloto quinua ⭐ / Experto / Personalizado) | precarga todo lo demás; tarjeta **"Personalizado"** → abre esta guía (Parte A) |
| **2. Equipo y límites** | Selector del equipo + preset de límites (presupuesto de agua, ventana, acciones permitidas) con **validación vs `sensor_variables`**: si falta una variable requerida, ofrece "agregar variable" o "modo degradado" (doc 18 §5) | filas `device_agents` + `agent_limits` (doc 04 §6) |
| **3. Identidad** | Nombre (auto: `agro-…`), descripción, misión (texto prellenado de la plantilla) | `SOUL.md` (plantilla doc 16 §3/15 §3, editable) |
| **4. Modelo** | Desplegable: MiniMax / Ollama local / OpenAI-compatible | `config.yaml → model` |
| **5. Skills** | Checkboxes con las skills de la plantilla del dominio (+ botón "nueva skill" con editor) | `skills/agro/<dominio>/<skill>/SKILL.md` |
| **6. Herramientas** | Checkboxes por tool con descripción: Agrisignal (7), `agro-control` (`read_*`, `record_*`, `publish_command` con candado) | `tools.include/exclude` (el candado de `publish_command` solo se abre si el rol lo permite) |
| **7. Automatización** | Presets de cron: "horario de riego", "2× día", "semanal", "webhook de temp" + horario editable | `cron add` del perfil |
| **8. Probar** | Botón **"Probar con datos simulados"** → elige escenario (doc 17 §2) y ve la respuesta antes de activar | nada (solo ejecución de prueba) |
| **▶ Crear agente** | Un botón final | **provisioning**: perfil Hermes (SOUL + skills + cron) + filas `agents`/`device_agents`/`agent_limits` (doc 12 §6.2) |

**Pantalla "Mis agentes" (tarjetas):** cada agente con sus botones:

| Botón | Acción |
|---|---|
| **Editar** | Reabre el wizard en modo edición (SOUL, skills, tools, cron) |
| **Duplicar** | Clona el perfil (p. ej. `agro-enfermedades-sur` para otro lote) |
| **Probar** | Ejecuta un escenario simulado y muestra respuesta + costo LLM |
| **Métricas** | Entra al tablero de mejora en el tiempo (doc 17 §5) |
| **Pausar / Activar** | Detiene/arranca su cron (sin borrar nada) |
| **Eliminar** | Borra el perfil (con confirmación) |

**API subyacente** (diseño — detalle en [12](12-api-crud-completa.md) §6):

```
GET    /v1/agent-templates                    → catálogo (doc 18) con variables requeridas
GET    /v1/devices/{id}/agents                → activados + estado y última métrica
POST   /v1/devices/{id}/agents                → activar (provisioning: perfil + fila + cron + límites)
GET    /v1/devices/{id}/agents/{aid}          → SOUL + skills + tools + cron + límites
PUT    /v1/devices/{id}/agents/{aid}          → actualiza (reescribe archivos)
PUT    /v1/devices/{id}/agents/{aid}/limits   → límites (auditado, doc 19 §6)
POST   /v1/agent/feedback                     → veto/corrección (doc 19 §7)
GET    /v1/devices/{id}/agents/{aid}/performance?window=30d → tendencia (docs 17/19)
DELETE /v1/devices/{id}/agents/{aid}          → desactiva (retira perfil, conserva métricas)
```

### B.3 ¿Cuál uso?

| Situación | Herramienta |
|---|---|
| Primer agente, aprendiendo | **Manual (Parte A)** — entiendes cada archivo |
| Agente estándar rápido | **Profile Builder** de Hermes (B.1) |
| Agente agrícola con plantilla, sin terminal | **AgriAgents Builder** (B.2) |
| Scripting / masivo / CI | **CLI** (Parte A) |

Las tres producen **exactamente los mismos archivos** — el agente no sabe con qué botón
se creó. Por eso: aprende con el manual, trabaja con el visual.

---

Continúa: [17-simulacion-y-mejora.md](17-simulacion-y-mejora.md) — cómo simular, comparar
y **rastrear la mejora de cada agente en el tiempo con métricas por ejecución**.
