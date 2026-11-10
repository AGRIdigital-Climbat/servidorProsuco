#!/bin/bash
# Script de gestion de servicios MotorIA
# Uso: bash scripts/motoria.sh {start|stop|restart|status|url|log|backup}
#
# Servicios:
#   postgresql  -> Base de datos PostgreSQL 16 + pgvector
#   motoria     -> API FastAPI (uvicorn) en el puerto 8001
#   cloudflared -> Tunnel Cloudflare (quick tunnel) que publica el puerto 8001

set -e

LOGFILE=/var/log/cloudflared.log
SERVICES="postgresql motoria cloudflared"

usage() {
    echo "Uso: $0 {start|stop|restart|status|proveedores|url|log|backup}"
    echo ""
    echo "  start    Inicia todos los servicios (postgresql, motoria, cloudflared)"
    echo "  stop     Detiene todos los servicios (orden inverso)"
    echo "  restart  Reinicia todos los servicios"
    echo "  status   Muestra el estado de todos los servicios"
    echo "  proveedores  Verifica los proveedores LLM externos"
    echo "  url      Muestra la URL publica actual del tunnel Cloudflare"
    echo "  log      Sigue los logs de la API (Ctrl+C para salir)"
    echo "  backup   Genera un backup de la base de datos en db/backup_motoria.sql"
    echo "  purgar   Libera espacio: journals, caches apt/npm y temporales viejos"
}

url() {
    if [ ! -f "$LOGFILE" ]; then
        echo "Error: No existe $LOGFILE. Verifica que cloudflared este corriendo."
        return 1
    fi
    local URL
    URL=$(grep -oE 'https://[a-z0-9-]+\.trycloudflare\.com' "$LOGFILE" | tail -1)
    if [ -z "$URL" ]; then
        echo "Aun no se ha asignado una URL. Espera unos segundos y reintenta."
        return 1
    fi
    echo "URL publica: $URL"
    echo "Health check: curl $URL/health"
}

case "${1:-}" in
    start)
        for s in $SERVICES; do
            echo "Iniciando $s..."
            sudo systemctl start "$s"
        done
        echo "=== Servicios iniciados ==="
        sleep 2
        url
        ;;
    stop)
        for s in cloudflared motoria postgresql; do
            echo "Deteniendo $s..."
            sudo systemctl stop "$s"
        done
        echo "=== Servicios detenidos ==="
        ;;
    restart)
        sudo systemctl restart postgresql motoria cloudflared
        echo "=== Servicios reiniciados ==="
        sleep 2
        url
        ;;
    status)
        for s in $SERVICES; do
            echo "--- $s ---"
            sudo systemctl status "$s" --no-pager | head -3
        done
        ;;
    proveedores)
        echo "=== Verificando proveedores LLM ==="
        sudo -u climbat /home/climbat/MotorIA/venv/bin/python /home/climbat/MotorIA/scripts/verificar_proveedores.py
        ;;
    url)
        url
        ;;
    log)
        sudo journalctl -u motoria -f
        ;;
    purgar)
        echo "=== Liberando espacio en disco ==="
        df -h / | awk 'NR==1 || /\/$/{print}'
        echo "--- Vacunando journal a 100M ---"
        sudo journalctl --vacuum-size=100M 2>&1 | tail -1
        echo "--- Limpiando caches apt ---"
        sudo apt-get clean
        sudo rm -rf /var/lib/apt/lists/*
        echo "--- Limpiando caches npm (root) ---"
        if [ -d /root/.npm ]; then
            sudo rm -rf /root/.npm/_cacache /root/.npm/_npx
        fi
        echo "--- Temporales viejos en /tmp y /var/tmp ---"
        sudo find /tmp -maxdepth 1 -type f -mtime +1 -delete 2>/dev/null
        sudo find /var/tmp -maxdepth 1 -type f -mtime +7 -delete 2>/dev/null
        echo "--- Resultado ---"
        df -h / | awk 'NR==1 || /\/$/{print}'
        ;;
    backup)
        mkdir -p "$(dirname "$0")/../db"
        sudo -u postgres pg_dump motoria > "$(dirname "$0")/../db/backup_motoria.sql"
        echo "Backup guardado en db/backup_motoria.sql"
        ;;
    *)
        usage
        exit 1
        ;;
esac
