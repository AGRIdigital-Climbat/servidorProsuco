#!/usr/bin/env python3
"""
Migración idempotente: cifra en sitio cualquier apikey en texto plano.

Ejecutar una vez tras desplegar core/secrets.py:
    venv/bin/python scripts/migrar_secretos.py

Se puede ejecutar varias veces sin efectos secundarios.
"""

import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from db.connection import execute_query, get_connection
from core.secrets import encrypt_secret, es_cifrado


def main():
    rows = execute_query(
        "SELECT id, nombre, apikey FROM proveedores_llm WHERE apikey IS NOT NULL"
    )
    migradas = 0
    for r in rows:
        key = r["apikey"]
        if not key or es_cifrado(key):
            continue
        cipher = encrypt_secret(key)
        execute_query(
            "UPDATE proveedores_llm SET apikey = %s WHERE id = %s",
            (cipher, r["id"]),
            fetch=False,
        )
        migradas += 1
        print(f"  [OK] #{r['id']} {r['nombre']}")

    print(f"\nMigración completada: {migradas} clave(s) cifrada(s).")


if __name__ == "__main__":
    main()
