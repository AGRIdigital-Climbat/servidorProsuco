# Informe Técnico de Avances — AgriAgents

> Cabecera y resúmenes se completan/refinan con el comando /informe (skill: informe-avance).
**Fecha de actualización:** 2026-10-22
**Actividad / Objetivo:** Diseñar y documentar por completo la plataforma multiusuario de agentes
agrícolas auto-mejorables (Hermes Agent + PostgreSQL/TimescaleDB + FastAPI), con dispositivos de
campo HTTP-only (SIM800/SIM7000/ESP32/RPi) que consultan tareas con tracking_id, N agentes por
equipo con límites por API y auto-mejora en 3 niveles.
**Métodos aplicados:** investigación documental (doc oficial de Hermes Agent + búsqueda web),
diagnóstico de BD "compatible", diseño de esquema y protocolo, redacción de documentación técnica
y de lenguaje sencillo con diagramas Mermaid.
**Estado del trabajo:** COMPLETADO (documentación v1 + serie `docs_simple/` del piloto de riego de
quinua, incluida base de conocimiento agronómico con fuentes verificadas) — pendiente: despliegue
real en servidor Ubuntu (guía narrativa en `docs_simple/02`), push de `docs_simple/` a GitHub.

## Avances

### Avance 1 — 2026-10-22 · Documentación completa de la plataforma
- **Sesión:** AgriAgents — diseño y documentación
- **Objetivo del avance:** Crear la carpeta `docs/` completa: diagnóstico, visión de plataforma,
  potencial de Hermes Agent, arquitectura, base de datos, API, agentes/auto-mejora, dispositivos,
  dominios, seguridad, roadmap, y documentos de lenguaje sencillo.
- **Decisiones clave tomadas:**
  - BD de la plataforma: **PostgreSQL 18 + TimescaleDB** (+PostGIS, pgvector). Hermes mantiene su
    SQLite interno (state.db) sin tocar.
  - Comunicación campo↔servidor: **HTTP + tareas con tracking_id** (protocolo asíncrono), sin MQTT.
  - El **agente SIEMPRE vive en el servidor**; el MCU solo mide, pide tareas y ejecuta relés.
  - LLM: **agnóstico** (default MiniMax, reemplazable por cualquier provider o modelo local Ollama).
- **Archivos creados (docs/):**
  - `README.md` (índice + mensajes clave)
  - `00-diagnostico-y-decisiones.md` (incluye aclaración MiniMax: no es gratis ni obligatorio)
  - `01-vision-plataforma.md` (pilar)
  - `02-potencial-hermes-agent.md` (pilar, todo el potencial feature por feature)
  - `03-arquitectura.md`, `04-base-de-datos.md`, `05-api-fastapi.md`
  - `06-agentes-y-auto-mejora.md`, `07-dispositivos-y-borde.md`
  - `08-dominios-aplicacion.md`, `09-seguridad-y-roles.md`
  - `10-despliegue-ubuntu.md` (outline), `11-roadmap-y-verificacion.md`
  - `12-agentes-creacion-y-autoentrenamiento.md` (cómo se crean agentes + protocolo de autoentrenamiento en lenguaje sencillo y técnico)
  - `lenguaje-sencillo/00-resumen-general.md` (resumen completo con tablas y Mermaid)
  - `lenguaje-sencillo/01-potencialidades-agricolas.md`
- **Herramientas usadas:** websearch + webfetch (doc oficial Hermes Agent), read/write/edit (creación
  de documentación), bash (estructura de carpetas).
- **Resultado:** 16 documentos Markdown con diagramas Mermaid y tablas; protocolo de tareas con
  tracking_id documentado end-to-end; esquema PostgreSQL completo; bucle de auto-mejora en 3 niveles.

### Siguiente paso
- Desplegar en servidor Ubuntu (seguir outline de `docs/10-despliegue-ubuntu.md`) y ejecutar fase F0
  del roadmap (`docs/11-roadmap-y-verificacion.md`).

### Avance 2 — 2026-10-21 (2026-10-21 → 2026-10-22)
- **Sesión:** "Diseño de base de datos y API para agente agrícola"
- **Objetivo del avance:** quiero que me me ayudes con algo, quiero en un servidor ubuntu poner en una base de datos compatible con hermes agent que esta en la documentacion, la logica es la siguiente tenemos equipos meteorologicos, tambien equipos de riego que envian datos entonces las columnas son dinamicas pero por modelo …
- **Cambios (archivos):** Ninguno
- **Herramientas usadas:** bash, edit, glob, grep, question, read, todowrite, webfetch, websearch, write
- **Resultado del asistente:** Todo listo. Resumen de lo completado: ## ✅ Subido a GitHub https://github.com/AGRIdigital-Climbat/AgriAgents — rama `main`, commit `a474ad8` (19 archivos, 3710 líneas). ## ✅ Aclaración sobre MiniMax (añadida en `docs/00-diagnostico-y-decisiones.md` §4) | Pregunta | Respuesta | |---|---| | ¿Es gratis…

### Avance 3 — 2026-10-21 (2026-10-21 → 2026-10-22)
- **Sesión:** "Diseño de base de datos y API para agente agrícola"
- **Objetivo del avance:** quiero que me me ayudes con algo, quiero en un servidor ubuntu poner en una base de datos compatible con hermes agent que esta en la documentacion, la logica es la siguiente tenemos equipos meteorologicos, tambien equipos de riego que envian datos entonces las columnas son dinamicas pero por modelo …
- **Cambios (archivos):** Ninguno
- **Herramientas usadas:** bash, edit, glob, grep, list_mcp_resource_templates, question, read, todowrite, webfetch, websearch, write
- **Resultado del asistente:** Perfecto. Y sobre tu duda de las **múltiples herramientas**: sí, se pueden poner **tantas como quieras** — te lo aclaro rápido porque es clave: ## Cómo funcionan las herramientas múltiples (lo que no quedó claro) Un agente **no tiene una sola herramienta — tiene un "kit" de herramientas**. En Hermes…

### Avance 4 — 2026-10-21 (2026-10-21 → 2026-10-22)
- **Sesión:** "Diseño de base de datos y API para agente agrícola"
- **Objetivo del avance:** quiero que me me ayudes con algo, quiero en un servidor ubuntu poner en una base de datos compatible con hermes agent que esta en la documentacion, la logica es la siguiente tenemos equipos meteorologicos, tambien equipos de riego que envian datos entonces las columnas son dinamicas pero por modelo …
- **Cambios (archivos):** Ninguno
- **Herramientas usadas:** bash, edit, glob, grep, list_mcp_resource_templates, question, read, todowrite, webfetch, websearch, write
- **Resultado del asistente:** La revisión de internet está completa. Aquí está el resumen de lo verificado y el plan final. --- ## 1. Investigación completada (fuentes verificadas) **Fenología de la quinua (días post-siembra):** | Fase | DAS aprox. | Fuente | |---|---|---| | Germinación | 3–5 | UNSCH/Perú, CEDSAN | | Emergencia …

### Avance 5 — 2026-10-22 · Serie `docs_simple/` (piloto de riego de quinua)
- **Sesión:** AgriAgents — piloto simple: un equipo de riego en el altiplano boliviano
- **Objetivo del avance:** Crear la carpeta `docs_simple/` (paralela a `docs/`) para un piloto con
  un solo equipo de riego de **quinua** en el **altiplano boliviano**: variables e intervalos de
  medición configurables por el usuario, agentes de riego completos, **Agrisignal como MCP
  fundamental**, despliegue Ubuntu narrativo sin comandos, y una **base de conocimiento agronómico
  con fuentes verídicas** que sustente las reglas del agente (revisión exhaustiva de internet).
- **Cambios (archivos):** `docs_simple/` — 10 archivos nuevos, ~1 591 líneas:
  - `00-README.md`, `01-vision-piloto.md`, `02-despliegue-ubuntu.md` (narrativo, sin comandos),
    `03-equipo-variables-y-medicion.md`, `04-base-de-datos.md`, `05-api.md`,
    `06-agentes-riego.md`, `07-herramientas-agrisignal-multitool.md`,
    `08-prueba-y-personalizacion.md`, `09-conocimiento-agronomico.md` (293 líneas, 35 fuentes)
  - `INFORME_AVANCES.md` (este informe, cabecera + Avance 5)
- **Herramientas usadas:** websearch (fenología, Kc/Ky, TAW/RAW, suelos y clima del altiplano,
  umbrales de sensores), read, glob, write, edit, todowrite, skill (informe-avance), bash.
- **Resultado:** Serie completa. La doc 09 consolida datos verificados con URL por fuente: Kc
  lisimétricos del altiplano 0.5/1.0/0.7 y Ky 0.67 (García et al. 2003/UMSA), ETc ~500 mm/ciclo,
  ciclo 160–180 d, GDD base 3 °C, TAW/RAW FAO-56 con umbral operativo UR = CC − p×(CC−PMP) ≈ 27 %
  (ejemplo), límites −8/−1/38 °C, riego deficitario 0.75 FI / umbral 55 % ETc, sensores 20+40 cm;
  reglas R1–R9 del agente enlazadas a su fuente; protocolo tracking_id, kit multi-tool MCP y
  guardas deterministas documentados de extremo a extremo.

### Avance 6 — 2026-10-21 (2026-10-21 → 2026-10-22)
- **Sesión:** "Diseño de base de datos y API para agente agrícola"
- **Objetivo del avance:** quiero que me me ayudes con algo, quiero en un servidor ubuntu poner en una base de datos compatible con hermes agent que esta en la documentacion, la logica es la siguiente tenemos equipos meteorologicos, tambien equipos de riego que envian datos entonces las columnas son dinamicas pero por modelo …
- **Cambios (archivos):** Ninguno
- **Herramientas usadas:** bash, edit, glob, grep, list_mcp_resource_templates, question, read, skill, todowrite, webfetch, websearch, write
- **Resultado del asistente:** Listo. **`docs_simple/` completa — 10 archivos, ~1 591 líneas:** | Doc | Contenido clave | |---|---| | `00-README` | Índice, alcance piloto, arquitectura MCU-tonto/cerebro-en-servidor | | `01-vision-piloto` | Viaje dato→decisión (Mermaid), agentes, tablas configurables, criterios de éxito | | `02-de…

### Avance 7 — 2026-10-21 (2026-10-21 → 2026-10-23)
- **Sesión:** "Diseño de base de datos y API para agente agrícola"
- **Objetivo del avance:** quiero que me me ayudes con algo, quiero en un servidor ubuntu poner en una base de datos compatible con hermes agent que esta en la documentacion, la logica es la siguiente tenemos equipos meteorologicos, tambien equipos de riego que envian datos entonces las columnas son dinamicas pero por modelo …
- **Cambios (archivos):** D:/CLIMBAT/OpenCodeProyect/AgriAgents/docs_simple/10-agentes-skills-herramientas.md, D:/CLIMBAT/OpenCodeProyect/AgriAgents/docs_simple/11-dia-tipo-y-escenarios.md, D:/CLIMBAT/OpenCodeProyect/AgriAgents/docs_simple/06-agentes-riego.md
- **Herramientas usadas:** bash, edit, glob, grep, list_mcp_resource_templates, question, read, skill, todowrite, webfetch, websearch, write
- **Resultado del asistente:** ## Objective - Documentar `docs_simple/` (paralela a `docs/`) para un piloto de **riego de quinua en el altiplano boliviano** con un solo equipo: variables e intervalos de medición configurables por usuario, agentes de riego completos, **Agrisignal como MCP central**, despliegue Ubuntu narrativo sin…
