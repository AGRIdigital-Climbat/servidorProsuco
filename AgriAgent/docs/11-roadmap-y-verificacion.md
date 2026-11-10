# 11 — Roadmap y verificación

> Fases de construcción, criterios de aceptación verificables y riesgos con mitigación.

---

## 1. Fases (F0–F7)

| Fase | Contenido | Entregable verificable |
|---|---|---|
| **F0 · Fundaciones** | PostgreSQL 18 + extensiones, instalación Hermes, venv + FastAPI, repo, `config.yaml` base, perfil `agro-ops` | `hermes doctor` OK; `psql` con extensiones; `/v1/ops/health` responde |
| **F1 · Registro + ingestión** | Esquema completo (`04`), API ingest, HMAC, device_models/metrics, agregados continuos | 10 000 lecturas ingestadas y consultadas por SQL; validación 422 con métrica no declarada |
| **F2 · Cola de tareas + primer agente** | `tasks` con tracking_id, `commands`, guarda, MCP registry+control, skills de riego, cron pull-loop, `episodes` | 20 tareas end-to-end (POST→poll→ready→ack); 0 comandos fuera de rango |
| **F3 · Multiusuario + límites** | `users`, roles, API keys, `agents`, `agent_limits` CRUD, vistas de métricas | 2 usuarios aislados; límite de agua bloquea comando y queda en `guard_evaluations` |
| **F4 · Borde real** | Firmware ESP32/SIM7000 HTTP-only, HMAC, buffer offline, OTA, fallback por comportamiento histórico | Riego automático de un bancal real + ticket de decisión humano + fallback probado sin red |
| **F5 · Invernadero + ganadería + lab** | Clima CEA, PLF, LIMS + instrumentos, skills de dominio | Detección de oídio con F1; alerta de celo; fallo de QC Westgard |
| **F6 · Auto-mejora** | Curator + write_approval, feedback→skill, batch_runner → LoRA → eval → promotion gate | Eval del LoRA > champion en holdout; modelo promovido y servido |
| **F7 · Endurecimiento** | mTLS/MFA, rotación de claves, backups, unidades systemd, canario/rollback | Restore de backup probado + plan de reversión de modelo |

---

## 2. Criterios de aceptación (pruebas)

| Área | Prueba |
|---|---|
| Unit | FAO-56 contra referencias (CIMIS); guardas; ETc por Kc; Westgard |
| Integración | Roundtrip ingest → SQL → agregados; lease/expiry/retry de tareas; ack; buffer offline |
| Contrato | `outputSchema` por tool MCP; `hermes mcp test`; lint de skills |
| ML | Eval suite con umbral de mejora; reporte de guard violations por versión |
| E2E | sensores → tarea → decisión → acción → feedback → skill mejorada → dataset → LoRA → promoción |

---

## 3. Riesgos y mitigación

| # | Riesgo | Mitigación |
|---|---|---|
| 1 | Entorno RL (`environments/`) no está en `main` | Pinear rama `atropos-hermes-agent`; Nivel 1 (SFT) no depende de él |
| 2 | `skills.write_approval` bloquea auto-refinamiento sin humano | `true` en producción, `false` solo en piloto |
| 3 | RAM insuficiente para entrenamiento | Entrenar en GPU externa/CI; el servidor solo sirve inferencia |
| 4 | Sin systemd en algunos entornos | `pg_ctlcluster` + scripts; en destino, unidades systemd reales |
| 5 | El LLM deriva del dato | Toda decisión debe venir de `readings` (SQL verificable); la guarda lo exige |
| 6 | Un usuario ve datos de otro | Filtro por `user_id` en API/MCP + rol read-only del agente |
| 7 | La auto-mejora produce skill mala | `write_approval` + Curator + linters + backups/rollback |
| 8 | Costo de tokens crece | Scripts no-agent, `NO_REPLY`, modelos locales para lo repetitivo |
| 9 | Caída del LLM | Fallback providers + política de comportamiento histórico en el MCU |

---

## 4. Métricas de éxito (KPI)

| KPI | Meta piloto |
|---|---|
| Disponibilidad de agentes (tareas tomadas en < 2 ciclos) | > 99 % |
| Violaciones de guarda (comandos bloqueados indebidos) | 0 |
| Agua ahorrada vs. riego programado | > 15 % |
| Aceptación humana de decisiones sin editar | > 80 % |
| Tiempo task→ack (percentil 95) | < 5 min |
| Restore de backup probado | 100 % antes de F7 |

---

**Volver al [índice](README.md).**
