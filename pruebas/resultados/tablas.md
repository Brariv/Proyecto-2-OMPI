# Tablas de tiempos — Proyecto 02 (CC3069)

## Tabla 1 — Inciso 2: tiempo de busqueda, estrategia base (ciclica), -np 4

| Prueba | Clave | Espacio de busqueda | Tiempo (s) | Candidatas (suma) |
|---|---|---|---:|---:|
| 2a | 123456 | completo 2^56 | 0.052195 | 129169 |
| 2b | 2^56/4 | ventana 2^22 centrada | 0.541887 | 2109441 |
| 2c | 2^56/4+1 | ventana 2^22 centrada | 0.559452 | 2109441 |

> Las claves 2^56/4 y 2^56/4+1 son inviables por fuerza bruta ciclica sobre el espacio completo [0, 2^56): se midieron sobre una ventana centrada de 2^22 candidatas alrededor del objetivo. Ver Tabla 2 para la comparacion con las estrategias que si recorren el espacio completo.

## Tabla 2 — Inciso 3: comparacion de estrategias (-np 4)

| Clave | Estrategia | Espacio | Tiempo (s) | Candidatas | Fase que halla |
|---|---|---|---:|---:|---|
| 123456 | ciclica | completo 2^56 | 0.052195 | 129169 | ciclica |
| 123456 | bloques | completo 2^56 | 0.160431 | 504385 | bloques |
| 123456 | heuristica | completo 2^56 | 0.050261 | 129169 | heuristica (diccionario) |
| 2^56/4 | ciclica | ventana 2^22 centrada | 0.541887 | 2109441 | ciclica |
| 2^56/4 | bloques | completo 2^56 | 0.004212 | 12289 | bloques |
| 2^56/4 | heuristica | completo 2^56 | 0.080162 | 209785 | heuristica (diccionario) |
| 2^56/4+1 | ciclica | ventana 2^22 centrada | 0.559452 | 2109441 | ciclica |
| 2^56/4+1 | bloques | completo 2^56 | 0.004485 | 12290 | bloques |
| 2^56/4+1 | heuristica | completo 2^56 | 0.074697 | 209785 | heuristica (diccionario) |

> Sobre el espacio completo 2^56, bloques encuentra 2^56/4 en la primera prueba del proceso 1 y 2^56/4+1 en la segunda; la heuristica las halla en el diccionario (potencia de dos y su vecina). La ciclica solo es competitiva con la clave pequena.

## Tabla 3 — Speedup de la estrategia ciclica (ventana comun por clave)

| Clave | T(1 proc) s | T(2 proc) s | T(4 proc) s | Speedup x2 | Speedup x4 |
|---|---:|---:|---:|---:|---:|
| 123456 | 0.062740 | 0.033837 | 0.047835 | 1.85 | 1.31 |
| 2^56/4 | 1.056753 | 0.588079 | 0.626521 | 1.80 | 1.69 |
| 2^56/4+1 | 1.093445 | 0.555352 | 0.574341 | 1.97 | 1.90 |

> Speedup = T(1 proceso) / T(n procesos). El tope practico depende de los nucleos fisicos de la maquina; con mas procesos que nucleos la mejora se satura por la sobre-suscripcion.
