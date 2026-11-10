#!/bin/bash
# Script para restaurar la base de datos de MotorIA
# Uso: bash scripts/restaurar_db.sh

set -e

echo "=== Restaurando base de datos motoria ==="

# Verificar que existe el backup
if [ ! -f "db/backup_motoria.sql" ]; then
    echo "Error: No se encontro db/backup_motoria.sql"
    exit 1
fi

# Eliminar base de datos existente
echo "Eliminando base de datos existente..."
sudo -u postgres psql -c "DROP DATABASE IF EXISTS motoria;"
sudo -u postgres psql -c "DROP USER IF EXISTS climbat;"

# Crear usuario y base de datos
echo "Creando usuario y base de datos..."
sudo -u postgres psql -c "CREATE USER climbat WITH PASSWORD 'AGRIdigital2024';"
sudo -u postgres psql -c "CREATE DATABASE motoria OWNER climbat;"

# Restaurar
echo "Restaurando desde backup..."
sudo -u postgres psql -d motoria -f db/backup_motoria.sql

# Otorgar permisos
echo "Otorgando permisos..."
sudo -u postgres psql -d motoria -c "GRANT ALL ON SCHEMA public TO climbat;"
sudo -u postgres psql -d motoria -c "GRANT ALL ON ALL TABLES IN SCHEMA public TO climbat;"
sudo -u postgres psql -d motoria -c "GRANT ALL ON ALL SEQUENCES IN SCHEMA public TO climbat;"

echo "=== Base de datos restaurada correctamente ==="
