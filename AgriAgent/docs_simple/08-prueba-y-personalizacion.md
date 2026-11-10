# 08 — Prueba y personalización del piloto

> Cómo verificar que todo el circuito funciona, y cómo el usuario personaliza variables,
> intervalos y umbrales **sin tocar firmware ni código del agente**.

---

## 1. Prueba del circuito completo (checklist)

### Fase A — sin hardware real (simulador)

| # | Paso | Éxito si… |
|---|---|---|
| A1 | Servidor levantado (BD → API → agente → MCP, ver [02](02-despliegue-ubuntu.md)) | Servicios activos, reinicio limpio |
| A2 | Enviar lote de lecturas firmado (script simulador o curl manual) | `202` + filas en `readings`; firma mala → `401` |
| A3 | Crear tarea `irrigation_analysis` | `202` con `tracking_id` |
| A4 | Sondear `GET /v1/tasks/{id}` | `200 ready` con `command` o `noop` **con justificación numérica** |
| A5 | Verificar `decision_inputs` | Lecturas + números Agrisignal + umbral citados |
| A6 | Mandar `ack executed` | Tarea en `done`; evento visible en `v_irrigation_events` |
| A7 | Humedad sintética baja (20 %) vs. alta (40 %) | `irrigate` vs. `skip` correctamente |
| A8 | Guardas: duración excesiva, tanque 5 %, helada activa | Rechazos `409` con código de guarda |

### Fase B — con equipo en el banco (sin riego real)

| # | Paso | Éxito si… |
|---|---|---|
| B1 | MCU con su clave; arranque pide `/config` | Recibe sus variables e `interval_seconds` |
| B2 | Sumergir sonda en agua/secar | Valores plausibles en `readings` con `ts` correctos |
| B3 | Desconectar la red 15 min | Buffer local y reenvío idempotente al reconectar (`409 duplicate_seq` bien manejado) |
| B4 | Orden `irrigate` con relé a un vaso | Relé abre N segundos; `ack` reporta duración real |
| B5 | Cambiar `interval_seconds` por API | MCU lo aplica en el siguiente sondeo **sin reinicio** |

### Fase C — campo real (riego de prueba controlado)

| # | Paso | Éxito si… |
|---|---|---|
| C1 | Una zona, una tarde | Un evento ejecutado con volumen ≈ `caudal × tiempo` |
| C2 | Verificación física (calicata o sonda) | La humedad 20 cm subió tras el riego; 40 cm no se empapó (sin exceso) |
| C3 | 7 días | Lotes continuos, decisiones trazables, cero órdenes en ventana de helada |
| C4 | Reporte semanal | `v_decision_quality` y `v_daily_water_balance` con datos reales |

---

## 2. Personalización por el usuario

### 2.1 Agregar una variable nueva (ejemplo: EC del suelo)

1. **Crear** la variable: `POST /v1/variables` con
   `{"key": "ec_suelo_20cm", "label": "Conductividad 20 cm", "unit": "dS/m",
     "value_type": "double", "role": "soil_ec", "depth_cm": 20}`.
2. **Asignar** al dispositivo con su intervalo:
   `PUT /v1/devices/altiplano-lote-a/variables/ec_suelo_20cm`
   `{"interval_seconds": 3600, "enabled": true}`.
3. **Cablear** el sensor al MCU y registrarlo en `sensors` y su variable en `sensor_variables` (puerto → key, con rangos).
4. El MCU la pide en su siguiente `/config` y empieza a enviarla. **Ningún cambio de
   firmware, ni de API, ni del agente.**

### 2.2 Cambiar el intervalo de medición

`PUT /v1/devices/{id}/variables/{key}` con `{"interval_seconds": 900}` — efectivo en el
próximo sondeo. Recomendaciones del piloto (ajustables):

| Variable | Día | Noche / ventana de helada |
|---|---|---|
| Humedad de suelo | 30 min | 60 min |
| Temperatura de aire | 15–30 min | 15 min (helada) |
| Nivel de tanque | 60 min | 60 min |

### 2.3 Ajustar umbrales agronómicos (zona)

`PUT /v1/zones/{id}/field-config`:

| Parámetro | Significado | Default piloto (ver doc 09) |
|---|---|---|
| `mad_fraction` (p) | Fracción de TAW agotable antes de regar | 0.5 |
| `fc_percent` / `pmp_percent` | Constantes hídricas del suelo (% volumen) | Medidos o de tabla textural |
| `sowing_date` | Fecha de siembra → fase/Kc/GDD | La real del lote |
| `max_seconds_per_event` | Guarda dura de duración | 1800 s |
| `max_events_per_day` | Guarda dura de frecuencia | 2 |
| `tank_min_percent` | No regar con tanque bajo | 20 % |

Cambios de umbrales quedan auditados (quién, cuándo, antes/después).

### 2.4 Cambiar el cerebro (LLM)

Configuración de Hermes, **no de la app**: MiniMax (default, de pago y no obligatorio),
cualquier endpoint compatible OpenAI u **Ollama local**. Cambiar proveedor no altera
skills ni reglas — solo quién redacta la decisión.

### 2.5 Cambiar las reglas del agente (skills)

Las reglas viven en las skills del perfil (`agro-riego`) basadas en
[09-conocimiento-agronomico.md](09-conocimiento-agronomico.md). Para ajustar una regla:
editar la skill (o dejar que Hermes Curator la refine con evidencia de `decision_inputs`),
nunca código. Toda regla nueva **cita su fuente** — si no tiene fuente, no entra.

---

## 3. Diagnóstico rápido de problemas

| Síntoma | Causa probable | Acción |
|---|---|---|
| `401` en lecturas | Clave HMAC desalineada o reloj fuera de ±5 min | Revisar secreto en `devices` y NTP |
| Lotes antiguos en `readings` | MCU en buffer por caída | Normal; verificar secuencia sin huecos grandes |
| Tareas `expired` | Agente/LLM lento o caído | Revisar servicios y trazas del LLM (doc 02 §6–7) |
| Decisión `defer` recurrente | Datos stale o ventana de helada | Revisar intervalos y alertas `agro-alertas` |
| Humedad 40 cm sube en cada riego | Exceso (percolación) | Bajar duración / revisar caudal vs. TAW |
| Órdenes rechazadas `guard_*` | Agente proponiendo fuera de política | Ajustar `zone_policies` o revisar skill |
| Agrisignal sin respuesta | Red o paquete MCP | Verificar `agrisignal-mcp` en el kit; el agente debe **defer**, no improvisar |

---

## 4. Antes de declarar el piloto operativo

1. Checklist A, B y C completadas con evidencia (capturas/registros).
2. Al menos 7 días de operación continua con < 1 % de pérdidas de lote.
3. Toda decisión de riego auditable con insumos (consulta `decision_inputs`).
4. Cero órdenes ejecutadas en ventana de helada o con tanque bajo.
5. Backup **restaurado con éxito** en una máquina limpia.
