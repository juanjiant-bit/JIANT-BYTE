# FORM — 22 estados de composición

FORM es un motor independiente de WAVE, MATH y DRUM-X. Sus 22 estados son **adaptaciones nativas** de las 22 referencias de [este archivo](references/bytebeat-floatbeat-funcbeat.txt), en el mismo orden. Genera audio, motivos, armonía, percusión y secciones durante la reproducción; no reproduce grabaciones. MATH FUNC y su editor siguen disponibles.

Las referencias mezclan JavaScript, arreglos extensos, afinaciones particulares, estéreo y grandes memorias de eco. FORM conserva rasgos de esas composiciones mediante rutinas acotadas y patrones compactos. No ejecuta los originales ni promete equivalencia exacta. La voz interna es mono; utiliza los buses estéreo existentes de JIANT para espacio y memoria. No reserva buffers nuevos de varios megabytes.

## Controles

| Página | Controles | Resultado |
|---|---|---|
| STATES | STATE A, STATE B, MORPH, COLOR | Dos referencias y su posición intermedia; carácter de las ecuaciones. |
| RULES | GROOVE, DENSITY, SEED, EVOLVE | Reorganización rítmica, presencia de eventos, rotación/motivos y cambios de sección. |
| CLOCK | RUN, DIV, CYCLE, SWING | NOTE o SONG, velocidad musical, extensión del ciclo en beats y desplazamiento de contratiempos. |
| NOTES | ROOT, SCALE, CHORD, VOICING | Centro tonal, cuantización, armonía y registro de acordes. |
| LAYERS | DOMAIN, DRUMS, DETUNE, DELAY | Interpretación matemática, percusión, separación de capas y envío al eco existente. |

RUN SONG arranca con **PLAY** y vuelve al inicio al detener/reiniciar. RUN NOTE mantiene el programa mientras se sostiene una nota y la transpone. Cada pista FORM usa una voz del motor; internamente contiene capas acotadas. CHORD organiza esas capas, sin disparar múltiples instancias del programa.

La batería emplea cuatro familias de ecuaciones: golpes tonales con ruido, FM/clap, percusión metálica y percusión por bits. Sus pesos también participan en MORPH.

AUTO conserva el dominio de cada estado. BYTE aplica cuantización y lógica entera; FLOAT usa una salida continua; FUNC añade una recurrencia de memoria/filtrado por voz; HYBRID cruza valores de una expresión entera con una onda continua. Son implementaciones nativas de esos comportamientos, no intérpretes de JavaScript. El estado interno se reinicia al disparar la voz.

MORPH interpola los relojes melódicos, alturas de motivos, progresión y pesos de eventos/secciones. Las alturas se ajustan a SCALE, por lo que cambian en grados musicales. Las topologías de síntesis diferentes se evalúan por separado y se mezclan; sólo A y B están activos. Los efectos de bus son controles compartidos de la pista. LIFE puede intercambiar características armónicas con las otras engines, sin modificar las referencias almacenadas.

DRUMS regula la percusión presente en los estados elegidos. FIVE PART y NESTED FM son texturas sin patrón de batería propio: para añadir batería, se puede morphear hacia un estado rítmico o usar DRUM-X. DETUNE y VOICING actúan sobre estados con varias capas. Los ocho controles principales admiten los destinos E1–E8 de la matriz; PITCH y SHP actúan sobre afinación y COLOR.

## Relación con el corpus

| Nº / estado | Rasgos trasladados | Adaptación / límite |
|---|---|---|
| 01 SCORE | Motivo, progresión, voz secundaria y entradas de batería | Patrón compacto, no la partitura completa de 64 segundos. |
| 02 TIME RULE | Tiempo modular, triángulo y lógica, ritmo irregular | Máscaras acotadas en lugar de listas JS de eventos. |
| 03 COUNTER | Melodía codificada, contrapunto y entradas por sección | Capas mono y eco compartido; no salidas estéreo independientes. |
| 04 FIVE PART | Cinco funciones a múltiplos impares y envolventes por bits | La tangente del original se sustituye por una combinación acotada de parciales. |
| 05 TUNED MEM | Motivo `14BGFB6B34BGIKNR`, pulso/detune, recurrencia y eco | Escala de BYTE; no los buffers ni todas las afinaciones del original. |
| 06 SWITCH | Alternancia de lógica temporal y ritmos | Divisores y cambios acotados. |
| 07 BASS ECHO | Bajo en capas desafinadas, subvoz, melodía y eco | Buses existentes en lugar de anillo privado de 12288 muestras. |
| 08 DETUNE | Cuatro capas desafinadas y memoria espacial | La apertura estéreo procede del bus. |
| 09 RATIOS | Razones 1, 4/5, 3/4, 1/2, 1/4 y secuencia reflejada | Relojes compactos y transposición común. |
| 10 NESTED FM | Modulación anidada e índice lento variable | Oscilador continuo acotado, sin batería propia. |
| 11 BIT MORPH | AND/XOR y melodía por semitonos | Banco lógico nativo, no evaluación de texto JS. |
| 12 CRUSH CHD | Acordes `037A05AC`, saturación y reducción de resolución | Recurrencia y bus de eco compartido. |
| 13 FILTER CHD | Acordes/pulso, componente pasaaltos, bajo y batería | Filtrado acotado y eco existente. |
| 14 SWEEPS | Barridos recíprocos de fase y percusión | Denominadores finitos, sin singularidades. |
| 15 BIT GROOVE | Multiplicaciones, desplazamientos y máscaras rítmicas | Reloj sincronizado al tempo. |
| 16 MASK SONG | Ritmos codificados y entradas escalonadas | Ciclo configurable en lugar de cortes fijos en muestras. |
| 17 NONLIN CHD | Fase elevada a potencia impar, acordes y puertas | Motivo compacto, sin tabla Unicode ni salidas independientes. |
| 18 GENERATIVE | Ratios, cambios por ciclo/semilla, recurrencia y memoria | Evolución acotada; no reconstruye el arreglo original de 768 beats. |
| 19 SHAPED ARP | Arpegio, potencias pares, silencios y registros | Capas compactas con tempo común. |
| 20 BUILD UP | Construcción progresiva por máscaras y capas | Secciones controladas por CYCLE/EVOLVE. |
| 21 BIT PAIRS | Dos expresiones de bits, ritmo cruzado y percusión | Pareja interna mono; espacio mediante buses. |
| 22 NESTED EQ | Funciones anidadas, producto de ondas, saturación y ruido acotado | Rutinas limitadas y composición compacta. |

## Generador

El corpus queda conservado como banco de referencia para nutrir GEN con familias de ecuaciones, roles, ritmos y estructuras reproducibles por semilla. Esta primera entrega introduce FORM y su composición interna; **GEN todavía no construye programas FORM nuevos a partir de las 22 fuentes**. No se sustituyen los arreglos existentes ni MY LOOP.

## Validación de v0.12

- Suite completa BYTE: aprobada. 22 presets reproducidos mediante controles de panel; 1.940.400 muestras coinciden bit a bit entre C nativo y WASM.
- 19 controles sonoros comparados a igual RMS, más RUN comprobado con PLAY/STOP; dos recorridos de morph de cinco posiciones y seis conexiones LIFE bidireccionales. Guardado de proyectos/USER y migración de favoritos PER4 a PER5 comprobados.
- Renderer: 22 estados, cinco dominios, extremos de COLOR y tres posiciones de MORPH sin errores de UBSan.
- Benchmark de host: 53 escenarios, una voz FORM por pista y hasta dos estados activos por voz. Peor relación tiempo de cálculo/audio: 0,010 con un estado, 0,012 con dos, 0,078 con seis pistas de dos estados y LIFE 100. Son mediciones del ordenador de desarrollo, **no porcentajes de CPU del dispositivo**. Datos: [form-cpu.csv](reports/form-cpu.csv); reproducible con `tests/form_cpu_test.c`.
- Firmware real compilado con las guardas de memoria activas: más de 20 KB libres en imagen y 9.248 bytes de margen del pool. Falta medir rendimiento y escucha en hardware físico.

La galería pública de v0.12 incluye los 22 estados y posiciones intermedias de morph, sin normalizar cada estado por separado.

Reproducir el benchmark (después de generar `build/gen` con el build):

```sh
mkdir -p build/form
cc -std=gnu99 -O2 -w -Wno-error=implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests tests/form_cpu_test.c -lm -o build/form-cpu
build/form-cpu
```

La suite `tests/run_byte_tests.sh` genera las muestras y métricas de FORM. Para obtener la galería OGG: ejecutar `build/byte-check/form 16`, `python3 tests/form_audio_test.py` y `python3 tools/form_gallery.py` (requiere ffmpeg).
