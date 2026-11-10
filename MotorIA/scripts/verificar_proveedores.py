#!/usr/bin/env python3
"""
Verificación periódica de proveedores LLM externos.

Para cada proveedor en la tabla proveedores_llm, realiza una llamada ligera al
modelo y actualiza estado_modelo (true/false) y ultima_verificacion.

Ejecutar:
    venv/bin/python scripts/verificar_proveedores.py
"""

import os
import sys
import logging

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
log = logging.getLogger("verificar")

from db.queries import obtener_proveedores_internos, actualizar_estado_modelo_proveedor
from core.secrets import decrypt_secret
from openai import OpenAI, OpenAIError


def verificar_proveedor(prov: dict) -> bool:
    try:
        apikey = decrypt_secret(prov["apikey"])
        client = OpenAI(api_key=apikey, base_url=prov["base_url"])
        client.chat.completions.create(
            model=prov["modelo_base"],
            messages=[{"role": "user", "content": "OK"}],
            max_tokens=8,
        )
        return True
    except OpenAIError:
        return False
    except Exception:
        return False


def main():
    proveedores = obtener_proveedores_internos()
    if not proveedores:
        log.info("No hay proveedores registrados.")
        return

    total = len(proveedores)
    ok = 0
    fail = 0

    for p in proveedores:
        nombre = p.get("nombre", f"id={p['id']}")
        log.info("Verificando %s ...", nombre)
        funcionando = verificar_proveedor(p)
        actualizar_estado_modelo_proveedor(p["id"], funcionando)
        if funcionando:
            ok += 1
            log.info("  %s -> OK", nombre)
        else:
            fail += 1
            log.warning("  %s -> FALLO", nombre)

    log.info("Resultado: %d OK / %d FALLO de %d total.", ok, fail, total)
    if fail > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
