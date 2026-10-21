#!/usr/bin/env python3
"""Genera las tablas en Markdown del reporte a partir de los CSV de medicion.

Lee  resultados/estrategias.csv  y  resultados/speedup.csv  y escribe
resultados/tablas.md con:
  - Tabla 1: inciso 2 (tiempo de la estrategia base ciclica por clave).
  - Tabla 2: inciso 3 (comparacion de estrategias con -np 4).
  - Tabla 3: speedup secuencial vs -np 2 vs -np 4 (estrategia ciclica).
"""
import csv
import os

AQUI = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(AQUI, "resultados")


def leer(nombre):
    with open(os.path.join(RES, nombre), newline="") as f:
        return list(csv.DictReader(f))


def f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def fmt_t(x):
    v = f(x)
    return f"{v:.6f}" if v is not None else "—"


ORDEN = ["123456", "2^56/4", "2^56/4+1"]


def ordenar(filas, clave_etiqueta="etiqueta"):
    return sorted(filas, key=lambda r: ORDEN.index(r[clave_etiqueta])
                  if r[clave_etiqueta] in ORDEN else 99)


def tabla_inciso2(est):
    """Tiempo de la estrategia base (ciclica) por clave."""
    cic = ordenar([r for r in est if r["estrategia"] == "ciclica"])
    out = ["## Tabla 1 — Inciso 2: tiempo de busqueda, estrategia base (ciclica), -np 4",
           "",
           "| Prueba | Clave | Espacio de busqueda | Tiempo (s) | Candidatas (suma) |",
           "|---|---|---|---:|---:|"]
    nombres = {"123456": "2a", "2^56/4": "2b", "2^56/4+1": "2c"}
    for r in cic:
        out.append(f"| {nombres.get(r['etiqueta'], '')} | {r['etiqueta']} "
                   f"| {r['espacio']} | {fmt_t(r['tiempo_s'])} | {r['candidatas']} |")
    out += ["",
            "> Las claves 2^56/4 y 2^56/4+1 son inviables por fuerza bruta ciclica sobre "
            "el espacio completo [0, 2^56): se midieron sobre una ventana centrada de "
            f"2^{VENTANA} candidatas alrededor del objetivo. Ver Tabla 2 para la "
            "comparacion con las estrategias que si recorren el espacio completo."]
    return "\n".join(out)


def tabla_inciso3(est):
    """Comparacion ciclica vs bloques vs heuristica (-np 4)."""
    by_key = {}
    for r in est:
        by_key.setdefault(r["etiqueta"], {})[r["estrategia"]] = r
    out = ["## Tabla 2 — Inciso 3: comparacion de estrategias (-np 4)",
           "",
           "| Clave | Estrategia | Espacio | Tiempo (s) | Candidatas | Fase que halla |",
           "|---|---|---|---:|---:|---|"]
    for k in ORDEN:
        if k not in by_key:
            continue
        for est_nom in ("ciclica", "bloques", "heuristica"):
            r = by_key[k].get(est_nom)
            if not r:
                continue
            out.append(f"| {k} | {est_nom} | {r['espacio']} | {fmt_t(r['tiempo_s'])} "
                       f"| {r['candidatas']} | {r['fase']} |")
    out += ["",
            "> Sobre el espacio completo 2^56, bloques encuentra 2^56/4 en la primera "
            "prueba del proceso 1 y 2^56/4+1 en la segunda; la heuristica las halla en "
            "el diccionario (potencia de dos y su vecina). La ciclica solo es competitiva "
            "con la clave pequena."]
    return "\n".join(out)


def tabla_speedup(spd):
    """Speedup secuencial vs np=2 vs np=4 (estrategia ciclica)."""
    by_key = {}
    for r in spd:
        by_key.setdefault(r["etiqueta"], {})[int(r["np"])] = f(r["tiempo_s"])
    out = ["## Tabla 3 — Speedup de la estrategia ciclica (ventana comun por clave)",
           "",
           "| Clave | T(1 proc) s | T(2 proc) s | T(4 proc) s | Speedup x2 | Speedup x4 |",
           "|---|---:|---:|---:|---:|---:|"]
    for k in ORDEN:
        if k not in by_key:
            continue
        t = by_key[k]
        t1, t2, t4 = t.get(1), t.get(2), t.get(4)
        s2 = f"{t1/t2:.2f}" if t1 and t2 else "—"
        s4 = f"{t1/t4:.2f}" if t1 and t4 else "—"
        out.append(f"| {k} | {fmt_t(t1)} | {fmt_t(t2)} | {fmt_t(t4)} | {s2} | {s4} |")
    out += ["",
            "> Speedup = T(1 proceso) / T(n procesos). El tope practico depende de los "
            "nucleos fisicos de la maquina; con mas procesos que nucleos la mejora se "
            "satura por la sobre-suscripcion."]
    return "\n".join(out)


VENTANA = os.environ.get("VENTANA_BITS", "22")

if __name__ == "__main__":
    est = leer("estrategias.csv")
    spd = leer("speedup.csv")
    doc = "\n\n".join([
        "# Tablas de tiempos — Proyecto 02 (CC3069)",
        tabla_inciso2(est),
        tabla_inciso3(est),
        tabla_speedup(spd),
    ]) + "\n"
    salida = os.path.join(RES, "tablas.md")
    with open(salida, "w") as fp:
        fp.write(doc)
    print(doc)
    print(f"\n[escrito en {salida}]")
