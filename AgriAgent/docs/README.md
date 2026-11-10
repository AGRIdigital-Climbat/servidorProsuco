# AgriAgents — Documentación de la Plataforma

> Plataforma multiusuario de agentes agrícolas auto-mejorables sobre **Hermes Agent** (Nous Research, MIT),
> **PostgreSQL 18 + TimescaleDB** y **FastAPI**, con dispositivos de campo (ESP32 / Raspberry Pi / SIM800 / SIM7000)
> conectados **solo por HTTP** (sin MQTT) que **sienten, actúan y preguntan** ("¿hay ejecuciones?").

**Fecha:** 2026-10-22 · **Estado:** documentación v1 (completa) · **Idioma:** español

---

## Mapa de lectura

| # | Documento | Qué contiene | Dónde leerlo |
|---|---|---|---|
| 00 | [Diagnóstico y decisiones](00-diagnostico-y-decisiones.md) | Hallazgos sobre Hermes Agent, la BD "compatible", gaps detectados y decisiones confirmadas | Intermedio |
| 01 | ⭐ [Visión de la plataforma](01-vision-plataforma.md) | **Pilar detallado**: la tesis (el MCU siente/actúa, el servidor piensa), ciclo de vida end-to-end del dato y la decisión, multiusuario, crecimiento | Empezar aquí |
| 02 | ⭐ [Potencial de Hermes Agent](02-potencial-hermes-agent.md) | **Pilar detallado**: TODA la capacidad de Hermes feature por feature y su uso en la plataforma | Empezar aquí |
| 03 | [Arquitectura](03-arquitectura.md) | Capas, diagramas, flujos (telemetría, comando long-poll, tarea de agente), por qué HTTP y no MQTT | Técnico |
| 04 | [Base de datos](04-base-de-datos.md) | Esquema completo PostgreSQL+Timescale: usuarios, device_models, readings JSONB, agentes, límites, auto-mejora, vistas de métricas | Técnico |
| 05 | [API FastAPI](05-api-fastapi.md) | Todos los endpoints: ingest, long-poll de comandos, gestión de usuarios/agentes/límites, tareas, episodios, ops | Técnico |
| 06 | [Agentes y auto-mejora](06-agentes-y-auto-mejora.md) | Registro de N agentes por equipo/modelo, skills, bucle de mejora en 3 niveles, ejemplo agente alerta de heladas | Técnico |
| 07 | [Dispositivos y borde](07-dispositivos-y-borde.md) | Firmware HTTP-only para ESP32/RPi/SIM800-SIM7000, HMAC, long-poll sin matar el timeout, buffer offline, OTA, fallback por comportamiento histórico | Técnico |
| 08 | [Dominios de aplicación](08-dominios-aplicacion.md) | Riego, invernadero, ganadería, domótica, laboratorio: sensores, actuadores, agentes y casos de uso | Consulta |
| 09 | [Seguridad y roles](09-seguridad-y-roles.md) | Admin vs cliente, matriz de permisos, guards deterministas, auditoría, TLS/red | Técnico |
| 10 | [Despliegue Ubuntu](10-despliegue-ubuntu.md) | Outline (sin comandos) de instalación: Postgres, Hermes, FastAPI, systemd, nginx, backups | Pendiente |
| 11 | [Roadmap y verificación](11-roadmap-y-verificacion.md) | Fases F0–F7, criterios de aceptación, riesgos y mitigaciones | Gestión |
| 12 | ⭐ [Creación de agentes y autoentrenamiento](12-agentes-creacion-y-autoentrenamiento.md) | **Cómo se crean los agentes en Hermes** (perfiles, SOUL.md, skills, cron), **qué helpers usamos para entrenar** (Curator, trainer, batch_runner, judge) y el **protocolo de autoentrenamiento** en lenguaje sencillo **y** técnico | Empezar aquí |

### Documentos en lenguaje sencillo (`lenguaje-sencillo/`)

| # | Documento | Qué contiene |
|---|---|---|
| 00 | [Resumen general en lenguaje sencillo](lenguaje-sencillo/00-resumen-general.md) | **Todo el sistema explicado de forma simple pero muy detallada**: tablas, diagramas Mermaid, el viaje de un dato, los agentes, la auto-mejora, los dominios, la seguridad, el futuro |
| 01 | [Potencialidades agrícolas](lenguaje-sencillo/01-potencialidades-agricolas.md) | Potencialidades, alcances, ventajas, revolución agrícola y potencial por caso de uso, en lenguaje cotidiano |

---

## Los 3 mensajes clave (lee esto primero)

> ### ⚠️ Aclaración de arquitectura — la más importante de todo el proyecto
>
> **El agente SIEMPRE está en el servidor Ubuntu. NUNCA en el microcontrolador.**
>
> - **Servidor Ubuntu** = donde vive el cerebro: Hermes Agent (LLM + skills + memoria + auto-mejora),
>   FastAPI y PostgreSQL. Aquí se **piensa**, se **decide** y se **aprende**.
> - **Microcontrolador (ESP32 / RPi / SIM800 / SIM7000)** = solo un **cliente HTTP "tonto"** que:
>   1. **Mide** (sensores) y **manda** un lote JSON al servidor.
>   2. **Pregunta** `POST .../tasks` → recibe un `tracking_id`, y luego `GET .../tasks/{id}` → "¿ya terminaste? dame la acción a ejecutar".
>   3. **Ejecuta** el relé/válvula si el servidor le responde con la acción, o aplica su **fallback local**
>      (última política de comportamiento histórico firmada) si no hay red.
>
> El MCU **no ejecuta ningún modelo, ninguna skill, ninguna IA**. Es un actuador/sensor con
> peticiones HTTP. Toda la "inteligencia" está en el servidor. Esto es lo que hace barato,
> masificable y actualizable el despliegue en campo.

1. **El cerebro está en el servidor; el MCU solo siente y actúa** (ver recuadro de arriba).
   El equipo manda datos y pregunta "¿hay algo que hacer?"; si la respuesta llega tarde o no llega,
   actúa por **comportamiento histórico aprendido** que el servidor le había entregado previamente.
2. **Cada equipo puede tener N agentes distintos** (todos corriendo en el servidor), cada uno con su
   propio entrenamiento, sus skills, sus límites ajustables por API y **métricas de mejora visibles**.
3. **Hermes Agent es el motor de auto-mejora** (corre en el servidor): memoria persistente, skills que
   el propio agente crea y refina (Curator), automatización (cron/loops/goals), delegación, MCP para
   tocar nuestros datos, y trazabilidad completa de cada decisión para entrenar mejor el siguiente modelo.

## Convenciones de esta documentación

- Diagramas en sintaxis **Mermaid** (se renderizan en GitHub, GitLab, VS Code con extensión, etc.).
- Rutas de API con prefijo `/v1/` — todo cambio rompe compatibilidad incrementa a `/v2/`.
- Nombres de tablas en `snake_case` plural; campos `created_at`/`updated_at` en UTC.
- Cuando la doc oficial de Hermes y esta documentación difieran en versiones, manda la doc oficial
  (verificada el 2026-10-22): <https://hermes-agent.nousresearch.com/docs>
