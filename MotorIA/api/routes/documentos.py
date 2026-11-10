from fastapi import APIRouter, UploadFile, File, Form, HTTPException, Query, Depends
from fastapi.responses import FileResponse
from pydantic import BaseModel, Field
from typing import Optional
from datetime import date
import os
import re
import fitz
from db.queries import (
    listar_documentos_filtrados,
    obtener_documento,
    eliminar_documento,
    contar_chunks_documento,
    listar_chunks_documento,
    actualizar_documento,
    obtener_tipo_documento,
)
from core.rag import procesar_documento
from core.verificacion import verificar_documento
from api.security import _validar_token_api_origen

router = APIRouter(prefix="/documentos", tags=["documentos"])

BASE_DIR = os.path.dirname(os.path.dirname(os.path.dirname(__file__)))
UPLOAD_DIR = os.path.join(BASE_DIR, "documentos")
os.makedirs(UPLOAD_DIR, exist_ok=True)


def _extraer_metadatos(md):
    claves = {
        "title": "titulo_pdf",
        "author": "autor",
        "subject": "asunto",
        "keywords": "keywords",
        "creator": "creador",
        "producer": "productor",
        "creationDate": "fecha_creacion",
        "modDate": "fecha_modificacion",
    }
    return {claves.get(k, k): v for k, v in md.items() if k in claves and v}


def _normalizar_tipo_id(tipo_id):
    if tipo_id in (None, 0):
        return None
    return tipo_id


def _sanitizar_nombre_archivo(nombre):
    limpio = "".join(c if c.isalnum() or c in ("-", "_") else "_" for c in nombre)
    limpio = re.sub(r"_+", "_", limpio).strip("_")
    if not limpio:
        limpio = "documento"
    return limpio[:120]


def _nombre_descarga(doc):
    base = doc.get("titulo") or f"documento_{doc['id']}"
    limpio = "".join(c if c.isalnum() or c in ("-", "_") else "_" for c in base).strip("_")
    if not limpio:
        limpio = f"documento_{doc['id']}"
    return limpio + ".pdf"


def _resolver_archivo_path(archivo_path):
    if not archivo_path:
        return None
    if os.path.isabs(archivo_path):
        return archivo_path
    return os.path.join(BASE_DIR, archivo_path)


def _respuesta_descarga(documento_id, inline=False):
    doc = obtener_documento(documento_id)
    if not doc:
        raise HTTPException(status_code=404, detail="Documento no encontrado")

    archivo_path = _resolver_archivo_path(doc.get("archivo_path"))
    if not archivo_path or not os.path.isfile(archivo_path):
        raise HTTPException(status_code=404, detail="Archivo del documento no encontrado en disco")

    return FileResponse(
        path=archivo_path,
        media_type="application/pdf",
        filename=_nombre_descarga(doc),
        content_disposition_type="inline" if inline else "attachment",
    )


@router.post("", dependencies=[Depends(_validar_token_api_origen)])
def subir_documento(
    archivo: UploadFile = File(..., description="Archivo PDF a subir e indexar."),
    titulo: str = Form(..., description="Titulo del documento para la biblioteca."),
    tipo_id: int = Form(..., description="Id del tipo de norma (GET /tipos-documento)."),
    fecha_publicacion: Optional[str] = Form(None, description="Fecha de publicacion en formato YYYY-MM-DD (opcional)."),
):
    """Sube un PDF, lo indexa con embeddings y lo verifica.

    Multipart/form-data. Extrae el texto con PyMuPDF, genera chunks y embeddings,
    luego ejecuta una verificacion de calidad inicial. El nombre fisico del
    archivo se limpia (quita caracteres invalidos de ruta conservando `.pdf`).
    """
    if not archivo.filename.lower().endswith(".pdf"):
        raise HTTPException(status_code=400, detail="Solo se aceptan archivos PDF")

    fecha = None
    if fecha_publicacion:
        try:
            fecha = date.fromisoformat(fecha_publicacion)
        except ValueError:
            raise HTTPException(status_code=400, detail="Formato de fecha invalido. Use YYYY-MM-DD")

    nombre_base = _sanitizar_nombre_archivo(titulo)
    base_pdf, ext_pdf = os.path.splitext(archivo.filename)
    nombre_pdf = _sanitizar_nombre_archivo(base_pdf)
    nombre_pdf += (ext_pdf.lower() if ext_pdf else ".pdf")
    archivo_path = os.path.join(UPLOAD_DIR, f"{nombre_base}_{nombre_pdf}")
    with open(archivo_path, "wb") as f:
        content = archivo.file.read()
        f.write(content)

    doc = fitz.open(archivo_path)
    num_paginas = doc.page_count
    texto_completo = ""
    for pagina in doc:
        texto_completo += pagina.get_text()
    metadatos = _extraer_metadatos(doc.metadata)
    doc.close()

    if not texto_completo.strip():
        raise HTTPException(status_code=400, detail="No se pudo extraer texto del PDF")

    resultado = procesar_documento(
        titulo=titulo,
        tipo_id=tipo_id,
        fecha_publicacion=fecha,
        archivo_path=archivo_path,
        contenido=texto_completo,
        num_paginas=num_paginas,
        metadatos=metadatos,
    )

    verificacion = verificar_documento(resultado["documento_id"], num_preguntas=10)

    return {
        "mensaje": "Documento procesado correctamente",
        "documento_id": resultado["documento_id"],
        "total_chunks": resultado["total_chunks"],
        "num_paginas": resultado["num_paginas"],
        "titulo": resultado["titulo"],
        "verificacion": verificacion,
    }


@router.get("")
def listado_documentos(
    tipo_id: Optional[int] = Query(None, description="Filtrar por tipo de norma."),
    q: Optional[str] = Query(None, description="Filtro de texto por titulo."),
    orden: str = Query("creado_en", pattern="^(titulo|fecha|creado_en|chunks|prioridad)$", description="Campo de ordenamiento."),
    dir: str = Query("desc", pattern="^(asc|desc)$", description="Direccion del ordenamiento."),
):
    """Lista documentos indexados, con filtros, orden y paginacion simple."""
    tipo_id = _normalizar_tipo_id(tipo_id)
    documentos = listar_documentos_filtrados(tipo_id=tipo_id, q=q, orden=orden, direccion=dir)
    return {
        "total": len(documentos),
        "filtros": {
            "tipo_id": tipo_id,
            "q": q,
            "orden": orden,
            "dir": dir,
        },
        "documentos": documentos,
    }


@router.get("/{documento_id}")
def detalle_documento(documento_id: int):
    """Detalle de un documento: datos, metadatos PDF y total de chunks."""
    doc = obtener_documento(documento_id)
    if not doc:
        raise HTTPException(status_code=404, detail="Documento no encontrado")
    chunks_total = contar_chunks_documento(documento_id)
    doc["total_chunks"] = chunks_total
    return doc


@router.get("/{documento_id}/descargar")
def descargar_documento(documento_id: int, inline: bool = Query(False, description="true = previsualizar en el navegador.")):
    """Descarga el PDF original del documento."""
    return _respuesta_descarga(documento_id, inline=inline)


@router.head("/{documento_id}/descargar")
def descargar_documento_head(documento_id: int, inline: bool = Query(False, description="true = previsualizar en el navegador.")):
    """Cabecera de la descarga (HEAD), util para enlaces."""
    return _respuesta_descarga(documento_id, inline=inline)


class DocumentoUpdate(BaseModel):
    titulo: str = Field(..., description="Nuevo titulo del documento.")
    tipo_id: int = Field(..., description="Nuevo tipo de norma (id).")
    fecha_publicacion: Optional[str] = Field(None, description="Fecha YYYY-MM-DD (opcional).")


@router.put("/{documento_id}", dependencies=[Depends(_validar_token_api_origen)])
def editar_documento(documento_id: int, req: DocumentoUpdate):
    """Edita titulo, tipo y fecha de publicacion de un documento."""
    if not req.titulo.strip():
        raise HTTPException(status_code=400, detail="El titulo no puede estar vacio")

    if not obtener_documento(documento_id):
        raise HTTPException(status_code=404, detail="Documento no encontrado")

    if not obtener_tipo_documento(req.tipo_id):
        raise HTTPException(status_code=400, detail="Tipo de documento no valido")

    fecha = None
    if req.fecha_publicacion:
        try:
            fecha = date.fromisoformat(req.fecha_publicacion)
        except ValueError:
            raise HTTPException(status_code=400, detail="Formato de fecha invalido. Use YYYY-MM-DD")

    actualizar_documento(documento_id, req.titulo.strip(), req.tipo_id, fecha)
    return {"mensaje": "Documento actualizado correctamente"}


@router.get("/{documento_id}/chunks")
def chunks_documento(documento_id: int):
    """Lista los chunks (fragmentos indexados) de un documento."""
    if not obtener_documento(documento_id):
        raise HTTPException(status_code=404, detail="Documento no encontrado")
    return {"documento_id": documento_id, "chunks": listar_chunks_documento(documento_id)}


@router.delete("/{documento_id}", dependencies=[Depends(_validar_token_api_origen)])
def borrar_documento(documento_id: int):
    """Elimina un documento y su archivo de disco."""
    doc = obtener_documento(documento_id)
    if not doc:
        raise HTTPException(status_code=404, detail="Documento no encontrado")
    eliminar_documento(documento_id)
    return {"mensaje": "Documento eliminado correctamente"}


@router.post("/{documento_id}/verificar", dependencies=[Depends(_validar_token_api_origen)])
def verificar(documento_id: int):
    """Evaluacion de calidad por documento: precision@1/3/5 y similitud."""
    if not obtener_documento(documento_id):
        raise HTTPException(status_code=404, detail="Documento no encontrado")
    return verificar_documento(documento_id, num_preguntas=10)
