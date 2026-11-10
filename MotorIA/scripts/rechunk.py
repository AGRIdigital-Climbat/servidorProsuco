#!/usr/bin/env python3
"""
Migración: Re-chunking de documentos existentes.
Borra los chunks actuales (1 por documento) y los re-procesa
con el nuevo chunking (regex corregido + chunks ≤600 chars).

Uso: venv/bin/python scripts/rechunk.py
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from db.connection import execute_query
from db.queries import borrar_chunks_documento, obtener_documento, listar_documentos
from core.chunking import detectar_secciones
from core.embeddings import generar_embeddings_lote, liberar_modelo


def rechunk():
    docs = listar_documentos()
    print(f"Documentos encontrados: {len(docs)}")

    for doc in docs:
        doc_id = doc["id"]
        titulo = doc["titulo"]
        full_doc = obtener_documento(doc_id)
        contenido = full_doc["contenido_completo"]

        print(f"\n{'='*60}")
        print(f"Documento #{doc_id}: {titulo}")
        print(f"  Contenido: {len(contenido)} chars")

        # Borrar chunks existentes
        borrar_chunks_documento(doc_id)
        print(f"  Chunks anteriores eliminados.")

        # Detectar secciones con el nuevo regex
        secciones = detectar_secciones(contenido)
        print(f"  Secciones detectadas: {len(secciones)}")

        if not secciones:
            print("  ⚠ No se detectaron secciones. Saltando.")
            continue

        # Generar embeddings en lote
        textos = [s["contenido"] for s in secciones]
        print(f"  Generando embeddings de {len(textos)} chunks...")
        embeddings = generar_embeddings_lote(textos)

        # Insertar chunks con embeddings
        from db.queries import insertar_chunk
        for i, (seccion, embedding) in enumerate(zip(secciones, embeddings)):
            insertar_chunk(
                documento_id=doc_id,
                contenido=seccion["contenido"],
                numeral=seccion.get("numeral", ""),
                orden=seccion.get("orden", i),
                embedding=embedding,
            )

        print(f"  ✅ {len(secciones)} chunks insertados con embeddings.")

        # Mostrar distribución de tamaños
        tamaños = [len(s["contenido"]) for s in secciones]
        print(f"  Tamaño chunks: min={min(tamaños)}, max={max(tamaños)}, avg={sum(tamaños)//len(tamaños)}")

    liberar_modelo()
    print(f"\n{'='*60}")
    print("Migración completada.")


if __name__ == "__main__":
    rechunk()
