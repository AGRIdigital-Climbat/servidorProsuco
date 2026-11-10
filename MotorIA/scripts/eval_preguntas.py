#!/usr/bin/env python
"""Evaluacion RAG con preguntas complejas multi-parte.

Cada caso tiene la VERDAD DE REFERENCIA sacada de los propios documentos
(chunks de la BD), no de la memoria del LLM. Falla con exit != 0.

Uso:
    venv/bin/python scripts/eval_preguntas.py                    # servidor local
    venv/bin/python scripts/eval_preguntas.py http://127.0.0.1:8001
    venv/bin/python scripts/eval_preguntas.py --solo Q2
"""
import argparse
import json
import re
import sys
import time
import unicodedata

import requests

PREGUNTAS = [
    {
        "id": "Q0",
        "titulo": "Ley 602 - 4 partes (regresion del fallo original)",
        "texto": (
            "¿Cuál es el objeto principal de la Ley N° 602 en cuanto a la gestión de riesgos y la "
            "atención de desastres, qué instancia superior de decisión y coordinación conforma el "
            "SISRADE en el ámbito territorial, cuál es el porcentaje exacto del Presupuesto General "
            "del Estado consolidado de gastos destinado como fuente de financiamiento para el "
            "fideicomiso FORADE, y cuál es el plazo máximo de duración establecido para el régimen "
            "de excepción tras declararse una situación de desastre o emergencia?"
        ),
        "debe_contener": [
            ("objeto (Art. 3 y ss.)", ["marco institucional", "prevencion", "reduccion de riesgos",
                                       "preparacion", "alerta"]),
            ("instancia territorial = CONARADE (Art. 8)", ["conarade"]),
            ("0.15% FORADE (Art. 29.a)", ["0.15", "0,15", "quince por ciento"]),
            ("9 meses (Art. 34)", ["nueve", "9 meses"]),
        ],
        "no_debe_contener": ["no se encontro informacion", "no encuentro informacion",
                             "no encontramos datos"],
    },
    {
        "id": "Q1",
        "titulo": "Municipio que rebasa capacidades -> instancia, composicion y Min. Defensa",
        "texto": (
            "Si en un municipio ocurre una emergencia por sequía que rebasa completamente sus "
            "capacidades técnicas y económicas, ¿a qué instancia departamental o nacional debe "
            "acudir legalmente para solicitar ayuda, quiénes conforman exactamente dicha instancia "
            "de nivel nacional según el cargo de sus miembros y qué rol cumple ahí el Ministerio "
            "de Defensa?"
        ),
        "debe_contener": [
            ("instancia nacional = CONARADE (Art. 9)", ["conarade"]),
            ("conformada por cargos: Ministros (Art. 9.I)", ["ministro", "ministra"]),
            ("rol del Min. de Defensa (Art. 9.I.a / 9.V)",
             ["viceministerio de defensa", "delegacion", "secretaria tecnica", "ministro de defensa",
              "consejo"]),
            ("nivel departamental (Art. 39.b / Art. 9.III)", ["departamental", "codera", "comurade"]),
        ],
        "no_debe_contener": ["no se encontro informacion"],
    },
    {
        "id": "Q2",
        "titulo": "Financiamiento: porcentaje, no reembolsables, visto bueno tecnico",
        "texto": (
            "Quisiera saber de dónde sale el dinero para financiar la gestión de riesgos en el país, "
            "específicamente qué porcentaje del presupuesto general se destina al fondo de ayuda, "
            "bajo qué condiciones operativas no reembolsables se asignan estos recursos y quién es "
            "el encargado de dar el visto bueno técnico antes de que se entreguen."
        ),
        "debe_contener": [
            ("0.15% (Art. 29.a)", ["0.15", "0,15", "quince por ciento"]),
            ("no reembolsables (Art. 28.IV)", ["reembolsable"]),
            ("visto bueno tecnico = CONARADE (Art. 28.III)", ["conarade", "evaluacion tecnica"]),
            ("fuente: Presupuesto / TGN (Art. 29.a)", ["presupuesto general", "tesoro general",
                                                       "organismo financiador 111"]),
        ],
        "no_debe_contener": ["no se encontro informacion"],
    },
    {
        "id": "Q3",
        "titulo": "Declaratoria nacional: firma, presupuesto/contrataciones, duracion",
        "texto": (
            "Cuando se desata un desastre de gran magnitud en el país que obliga a declarar una "
            "situación de emergencia nacional, ¿quién firma esa declaratoria, qué implicaciones "
            "prácticas trae en cuanto a modificaciones presupuestarias y contrataciones estatales, "
            "y cuánto tiempo dura como máximo ese régimen de excepción antes de volver a la "
            "normalidad?"
        ),
        "debe_contener": [
            ("firma: Decreto Supremo del Presidente (Art. 39.a.1)",
             ["decreto supremo", "presidenta", "presidente"]),
            ("previa recomendacion del CONARADE (Art. 39.a.1)", ["conarade"]),
            ("modificaciones presupuestarias (Art. 32)", ["presupuestari"]),
            ("contrataciones (Art. 33)", ["contratac"]),
            ("maximo 9 meses (Art. 34)", ["nueve", "9 meses"]),
        ],
        "no_debe_contener": ["no se encontro informacion"],
    },
]


def normalizar(texto):
    t = unicodedata.normalize("NFD", (texto or "").lower())
    t = "".join(c for c in t if unicodedata.category(c) != "Mn")
    return t


def consultar(base, pregunta, timeout=300):
    t0 = time.time()
    r = requests.post(
        f"{base.rstrip('/')}/chat",
        json={"pregunta": pregunta},
        stream=True,
        timeout=timeout,
    )
    r.raise_for_status()
    texto, fuentes, citas, intencion, error = "", [], {}, None, None
    for linea in r.iter_lines(decode_unicode=True):
        if not linea or not linea.startswith("data: "):
            continue
        try:
            ev = json.loads(linea[6:])
        except json.JSONDecodeError:
            continue
        tipo = ev.get("type")
        if tipo == "token":
            texto += ev.get("content", "")
        elif tipo == "error":
            error = ev.get("content")
        elif tipo == "done":
            fuentes = ev.get("fuentes") or []
            citas = ev.get("citas") or {}
            intencion = ev.get("intencion")
    return {
        "texto": texto.strip(),
        "fuentes": fuentes,
        "citas": citas,
        "intencion": intencion,
        "error": error,
        "seg": time.time() - t0,
    }


def evaluar_caso(base, caso):
    res = consultar(base, caso["texto"])
    cuerpo = normalizar(res["texto"])
    fallos = []

    print(f"\n{'=' * 78}")
    print(f"{caso['id']}  {caso['titulo']}")
    print(f"{'=' * 78}")
    if res["error"]:
        print(f"  ERROR DE STREAM: {res['error']}")
        fallos.append("stream")
    if not res["texto"]:
        print("  RESPUESTA VACIA")
        fallos.append("vacia")

    print(f"  {res['seg']:.1f}s | {len(res['texto'])} chars | "
          f"~{len(res['texto']) // 4} tokens | intencion={res['intencion']}")
    print(f"  fuentes: {len(res['fuentes'])} documentos | citas usadas: "
          f"{sorted(set(re.findall(r'\[(\d+)\]', res['texto'])), key=int) or '-'}")

    for etiqueta, alternativas in caso["debe_contener"]:
        hit = next((a for a in alternativas if normalizar(a) in cuerpo), None)
        estado = "PASS" if hit else "FAIL"
        if not hit:
            fallos.append(etiqueta)
        print(f"  [{estado}] {etiqueta}" + (f"  (via {hit!r})" if hit else ""))

    for prohibido in caso.get("no_debe_contener", []):
        if normalizar(prohibido) in cuerpo:
            fallos.append(f"prohibido: {prohibido}")
            print(f"  [FAIL] aparece texto prohibido: {prohibido!r}")

    if res["texto"]:
        print(f"  --- respuesta ---\n  {res['texto']}\n  {'-' * 74}")
    return fallos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("base", nargs="?", default="http://localhost:8001")
    ap.add_argument("--solo", help="correr solo una pregunta (Q0..Q3)")
    ap.add_argument("--guardar", help="guarda las respuestas crudas en un json")
    args = ap.parse_args()

    casos = [c for c in PREGUNTAS if not args.solo or c["id"] == args.solo.upper()]
    if not casos:
        print(f"no existe la pregunta {args.solo}")
        return 2

    try:
        h = requests.get(f"{args.base.rstrip('/')}/health", timeout=10).json()
    except Exception as e:
        print(f"servidor caido en {args.base}: {e}")
        return 2
    print(f"servidor: {h}  ({args.base})")

    resumen, guardado = {}, {}
    for caso in casos:
        try:
            fallos = evaluar_caso(args.base, caso)
        except Exception as e:
            print(f"  EXCEPTION: {type(e).__name__}: {e}")
            fallos = ["excepcion"]
        resumen[caso["id"]] = fallos
        if args.guardar:
            guardado[caso["id"]] = {"texto": "", "fallos": fallos}

    print(f"\n{'=' * 78}\nRESUMEN")
    exitos = 0
    for cid, fallos in resumen.items():
        total = len(next(c["debe_contener"] for c in PREGUNTAS if c["id"] == cid))
        ok = not fallos
        exitos += ok
        print(f"  {cid}: {'OK' if ok else 'FALLA -> ' + '; '.join(fallos)}")
    print(f"\n{exitos}/{len(resumen)} preguntas correctas")

    if args.guardar:
        with open(args.guardar, "w", encoding="utf-8") as f:
            json.dump({"fallos": resumen}, f, ensure_ascii=False, indent=2)
        print(f"guardado en {args.guardar}")

    return 0 if exitos == len(resumen) else 1


if __name__ == "__main__":
    sys.exit(main())
