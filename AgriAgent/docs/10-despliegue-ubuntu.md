# 10 — Despliegue Ubuntu (outline)

> **Solo outline** (sin comandos todavía), como se decidió. Este documento enumera los bloques de
> instalación en un servidor Ubuntu x86_64 (8–16 GB RAM). Los comandos exactos se añadirán cuando
> se ejecute el despliegue real.

---

## Bloque 1 — Requisitos del servidor

- Ubuntu Server 24.04/26.04 LTS, x86_64, 8–16 GB RAM, 50+ GB SSD.
- Acceso SSH con clave; firewall (UFW) cerrando todo salvo :22 (o VPN) y :443.
- Dominio o IP fija + TLS (certbot / Let's Encrypt) para Nginx.

## Bloque 2 — PostgreSQL 18 + extensiones

- Instalar PostgreSQL 18.
- Extensiones: `timescaledb`, `postgis`, `pgvector`, `pgcrypto`.
- Crear BD `agriagents`, usuario de aplicación y **usuario read-only** para el agente.
- Políticas de retención (cruda 90 días, hora 2 años) y `statement_timeout`.

## Bloque 3 — Hermes Agent

- Instalación vía `install.sh` (flags `--non-interactive --skip-browser`).
- `libatomic1` para `hermes serve` en Ubuntu.
- Perfil base `agro-ops`; provisionar perfiles `agro-*` (ver `06`/`12`).
- `hermes gateway install --system` + `loginctl enable-linger` para sobrevivir al logout.
- `hermes doctor` / `hermes cron doctor` como watchdog.

## Bloque 4 — FastAPI (agriapi)

- venv + `uvicorn`; módulos: ingest, tasks (tracking_id), agents, limits, ops.
- Usuario de BD de la aplicación con permisos mínimos.
- Unidad systemd `agriapi.service`.

## Bloque 5 — Reverse proxy y TLS

- Nginx :443 → FastAPI (:8080 interna) y, si aplica, dashboard Hermes (:9119 protegido).
- Certbot; HSTS; rate-limit por IP/dispositivo.

## Bloque 6 — Opcional: modelos locales

- Ollama (o vLLM) para servir LoRA `agro-*` como provider OpenAI-compatible.
- GPU externa/CI para entrenamiento (Nivel 1/2); el servidor de producción solo sirve inferencia.

## Bloque 7 — Observabilidad

- Prometheus + Grafana; `/metrics` de FastAPI; métricas de Hermes (gateway monitoring).
- Alertas de salud: cola de tareas, latencia LLM, estado del gateway.

## Bloque 8 — Backups y recuperación

- `pg_dump` diario + retención; snapshot de `~/.hermes/skills/` + `curator backup`.
- Prueba de restore documentada (ver `11`).

## Bloque 9 — Despliegue continuo

- Repo Git; despliegue por `git pull` + `systemctl restart`; migraciones de BD con control de versión.
- Ventana de mantenimiento; canary de modelos (5 %) con rollback automático.

---

**Siguiente:** [`11-roadmap-y-verificacion.md`](11-roadmap-y-verificacion.md).
