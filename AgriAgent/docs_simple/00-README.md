# AgriAgents — Documentación del Piloto (Versión Simple)

> Piloto con **un solo equipo de riego** en el **altiplano boliviano** para **quinua**:
> sensores que miden variables configurables, agentes de IA que deciden el riego,
> **Agrisignal** como herramienta meteorológica/edáfica fundamental (MCP sin llaves),
> y despliegue en Ubuntu narrativo paso a paso.

**Fecha:** 2026-10-22 · **Estado:** v1 · **Idioma:** español · **Serie:** `docs_simple/` (paralela a `docs/`)

---

## Qué es esta serie

`docs_simple/` es la versión **enfocada en el piloto real**: un equipo, un cultivo (quinua),
un propósito (riego inteligente). No reemplaza a `docs/` (que documenta la plataforma
multiusuario completa); la **complementa**: aquí solo está lo necesario para poner a correr
el piloto y tomar decisiones de riego correctas y trazables.

> ### ⚠️ Aclaración de arquitectura (igual que en `docs/`)
>
> **El agente SIEMPRE está en el servidor Ubuntu. NUNCA en el microcontrolador.**
>
> - **Servidor Ubuntu** = cerebro: Hermes Agent (LLM + skills + memoria), FastAPI y PostgreSQL.
>   Aquí se **piensa**, se **decide** y se **aprende**.
> - **Microcontrolador (ESP32 / Raspberry Pi / SIM800 / SIM7000)** = cliente HTTP que
>   1. **Mide** (sensores) y **manda** un lote JSON al servidor,
>   2. **Pregunta** `POST .../tasks` → recibe `tracking_id` y luego `GET .../tasks/{id}`,
>   3. **Ejecuta** relés/válvulas con la respuesta, o apaga su fallback local si no hay red.
>
> Sin MQTT: solo **HTTP + HMAC-SHA256**. El MCU no corre ningún modelo ni skill.

---

## Mapa de lectura

| # | Documento | Qué contiene | Dónde leerlo |
|---|---|---|---|
| 00 | **Este índice** | Alcance, mensajes clave, convenciones | — |
| 01 | [Visión del piloto](01-vision-piloto.md) | Qué busca el piloto, cómo fluye un dato hasta la decisión de riego, qué es configurable | Empezar aquí |
| 02 | [Despliegue en Ubuntu](02-despliegue-ubuntu.md) | Guía **narrativa sin comandos**: qué instalar, en qué orden y por qué | Instalación |
| 03 | [Equipo, variables y medición](03-equipo-variables-y-medicion.md) | Sensores y relés, **variables e intervalos de medición configurables por el usuario** | Campo |
| 04 | [Base de datos](04-base-de-datos.md) | Esquema simple: dispositivos, variables, lecturas, tareas, configuración | Técnico |
| 05 | [API](05-api.md) | Endpoints mínimos: ingestión por lotes, tareas con `tracking_id`, ack, long-poll | Técnico |
| 06 | ⭐ [Agentes de riego](06-agentes-riego.md) | Los agentes completos: qué reciben, qué deciden, reglas, órdenes a los relés | Empezar aquí |
| 07 | ⭐ [Herramientas: Agrisignal y multi-tool](07-herramientas-agrisignal-multitool.md) | **Agrisignal como MCP central**, kit de herramientas por agente, adaptador del pronóstico propio | Empezar aquí |
| 08 | [Prueba y personalización](08-prueba-y-personalizacion.md) | Cómo probar el piloto y cambiar variables, intervalos y umbrales | Operación |
| 09 | ⭐ [Conocimiento agronómico](09-conocimiento-agronomico.md) | **Base de conocimiento verificada**: fenología de la quinua, agua, suelos, clima y reglas del agente **con sus fuentes** | Reglas |
| 10 | ⭐ [Agente, skill y herramienta](10-agentes-skills-herramientas.md) | Definiciones exactas, **4 vías para agregar herramientas**, qué es dinámico vs. estático, seguimiento (`tracking_id`) | Definiciones |
| 11 | ⭐ [Día tipo y escenarios](11-dia-tipo-y-escenarios.md) | **Cadencias** (medir 5/10/30 min, consultar tareas), día hora por hora, helada, lluvia, sin red, tanque vacío | Operación |
| 12 | [API CRUD completa](12-api-crud-completa.md) | **Diseño de API**: POST 1×/PUT, GET últimos N, paginación + filtros, inventario equipo→sensores→variables+rangos, reportes | Técnico |
| 13 | ⭐ [48 h paso a paso](13-flujo-48h-detallado.md) | **Ejemplo detallado**: payloads minuto a minuto, 2 días completos con lluvia, caída de red y sonda fuera de rango | Verificación |
| 14 | ⭐ [Resumen y potencial](14-resumen-y-potencial.md) | **Compilación completa en lenguaje sencillo**: casos de uso con ejemplos, cualidades, comparativa vs timers/comercial y código con `if`, IoT inteligente y agentes que se automejoran | Empezar aquí |
| 15 | ⭐ [Dinámica agrícola de la quinua](15-dinamica-agricola-quinua.md) | Más allá del riego: ciclo del cultivo, **enfermedades (mildiu, royas, chinche), heladas y producción/biomasa** con fuentes; los **4 agentes plantilla** y su coordinación | Dominios |
| 16 | ⭐ [Crear agentes desde cero](16-crear-agente-desde-cero.md) | **Manual autocontenido** (7 pasos, comandos reales de Hermes, plantillas SOUL.md/SKILL.md copiables) + **entorno visual**: Profile Builder y diseño del AgriAgents Builder | Crear agentes |
| 17 | [Simulación y mejora](17-simulacion-y-mejora.md) | **Métricas por ejecución**: simular sin campo, A/B vs baseline y **rastrear la mejora de cada agente en el tiempo** (versionado de skills, tendencia) | Medición |
| 18 | ⭐ [Catálogo de agentes agrícolas](18-catalogo-agentes-agricolas.md) | **16 agentes** solo de agricultura (✅ listos / 🟡 en desarrollo / 🔮 futuros) con misión, tools, métrica y **variables requeridas**; paquetes y activación por **checkboxes** + la vía "crear desde cero" (doc 16) | Catálogo |
| 19 | [Entrenamiento y límites](19-entrenamiento-y-limites-agentes.md) | **Toda la lógica de agentes**: 3 niveles de auto-mejora (skills → SFT/LoRA → RL), helpers (Curator, `agro-trainer`, `batch_runner`, `goal_judge`), **`agent_limits` (doble ley)**, feedback humano y trazabilidad `episodes → promotions` | Entrenar |
---

## Los 3 mensajes clave

1. **Un equipo, un cerebro.** El microcontrolador solo mide y obedece; el agente de riego
   vive en el servidor, consulta clima/suelo (Agrisignal), calcula el balance hídrico y
   devuelve la orden concreta: *qué zona, cuánto tiempo, cuándo*.
2. **Todo lo configurable está en un solo lado.** El usuario elige **qué variables medir**
   (humedad, temperatura, EC, nivel de tanque…) y **cada cuánto**; la configuración viaja
   de la API → base de datos → dispositivo, sin tocar el firmware.
3. **Las reglas del agente no son inventadas.** La decisión de riego se apoya en la base
   agronómica de [09-conocimiento-agronomico.md](09-conocimiento-agronomico.md): Kc, TAW/RAW,
   umbrales FAO-56, fenología y clima del altiplano, **cada dato con fuente citada**.

---

## Convenciones

- Diagramas en sintaxis **Mermaid** (se renderizan en GitHub, VS Code, etc.).
- Rutas de API con prefijo `/v1/` — todo cambio rompe compatibilidad sube a `/v2/`.
- Nombres de tablas en `snake_case` plural; fechas `created_at`/`updated_at` en UTC.
- Los documentos de esta serie remiten a `docs/` cuando hace falta profundidad multiusuario.
- La doc oficial de Hermes (verificada 2026-10-22) manda en casos de diferencia:
  <https://hermes-agent.nousresearch.com/docs>
