# BYTE v0.7 — LIFE como intercambio de timbre

El chirrido tenía dos causas: valores negativos que se promovían a enteros sin signo al combinar donantes, y fragmentos de tiempo/constantes tratados como señal de audio y aplicados al reloj con demasiada profundidad. Esta versión corrige la aritmética y sustituye esa ruta por intercambio de formas de onda de banda limitada.

En **HOME → LIFE**, siguen disponibles **LIFE / NUM / FRAG / LINK**. Todos los motores se conectan entre sí: WAVE ↔ MATH (BYTE/FLOAT) ↔ DRUM-X, también entre notas polifónicas de la misma pista. No hay parejas fijas ni un motor central. GEN sigue iniciando con LIFE en 0 y se conserva la corrección de FUGA.

| Control | Comportamiento actual |
|---|---|
| LIFE | Profundidad con curva progresiva: recorrido suave/medio amplio y mayor transformación cerca del máximo. 0 es bypass exacto. |
| NUM | Comparte números de las fórmulas y A/VAR como coeficientes suaves de forma. Modifica la relación entre fundamental y armónicos mediante una curva cúbica acotada; no cambia divisores, máscaras, shifts ni multiplicadores del reloj. |
| FRAG | Evalúa expresiones completas de forma de onda de los donantes, elimina DC y conserva hasta ocho armónicos. Combina esos perfiles y los reconstruye a la fase del receptor, interpolando con su sonido. Seno, triángulo, cuadrada y fórmulas con parciales producen caracteres distintos. |
| LINK | Deforma suavemente la forma afinada según los números, el balance armónico y las envolventes de las otras voces. La profundidad se limita según los armónicos activos y las transiciones se suavizan. |

Un punto de partida: **LIFE 40–60, NUM 40, FRAG 70, LINK 35**, con dos sonidos distintos superpuestos. Para escuchar el intercambio de formas claramente, aislar **FRAG 100**, NUM/LINK 0, y recorrer LIFE. Cambiar la fórmula de una voz donante cambia el carácter que reciben las demás.

La batería clásica, que no tiene fórmula editable, participa mediante COLOR y su envolvente; recibe un COLOR temporal. Los fragmentos corresponden a voces con una expresión de forma de onda válida. Las fórmulas, presets y proyectos no se reescriben. Mute, solo, voces terminadas y LIFE por pista en 0 retiran su participación.

## Qué se evalúa

Cada voz donante se evalúa una vez por bloque de control sobre un ciclo normalizado. Se admiten subexpresiones completas de forma de onda, incluida una fórmula completa si es segura; se excluyen operadores con estado (`ph`, `lp`, `hp`), tablas y lecturas de otras pistas. Un resultado entero completo se interpreta como bytebeat; una variable de fase o una constante FIX aislada no se convierte en audio.

La proyección conserva hasta ocho armónicos y descarta DC. El receptor limita los parciales según su frecuencia, suaviza coeficientes y profundidad, y mantiene su reloj original. Los dos bancos de perfiles evitan depender del orden de render. No hay compilación ni asignación dinámica en el audio. La evaluación adicional tiene el límite de 64 instrucciones por voz/bloque, más una proyección fija de 32 puntos; no se vuelve a ejecutar el código de cada donante en cada receptor.

Es una interpretación tonal del fragmento, no una copia literal de sus irregularidades de reloj o de sus componentes fuera de banda. Si una fórmula no produce una forma segura, no dona FRAG; puede seguir aportando números y actividad.

## Comparaciones de audio

Mismas notas, ocho segundos, con **RMS igualado por archivo** para que el volumen no oculte el cambio de timbre:

- [LIFE 0](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v07/life-0.wav)
- [LIFE 50](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v07/life-50.wav)
- [LIFE 100](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v07/life-100.wav)

Una nota receptora, solo FRAG, donantes distintos, también igualados en RMS:

- [Seno](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v07/shape-0.wav)
- [Triángulo](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v07/shape-1.wav)
- [Cuadrada](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v07/shape-2.wav)
- [Parciales 1/3/5](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v07/shape-3.wav)

## Evidencia y límites

`tests/life_sound_test.c` y `.py` verifican el recorrido 0/25/50/75/100 a igual RMS, contraste armónico, afinación y energía aguda. En los tres casos de red probados, la energía por encima de 6 kHz queda por debajo del **0,031%**. En v0.6, WAVE llegaba al **54%**. Las formas de contraste mantienen más del 95% de su energía en armónicos de la nota receptora; las formas brillantes elevan su centroide sin generar una capa de hiss. Estas cifras describen esos casos de prueba, no todas las fórmulas posibles.

Hay regresiones explícitas de coeficientes negativos, bypass, seis conexiones dirigidas, todos los donantes simultáneos, retirada de notas, INT/FIX, fuentes inalteradas y fase original. El firmware real compila con los guards de memoria normales; native/WASM coincide en 88.200 frames con una canción densa. Benchmark WASM local: unos 53 ms por segundo de audio; no equivale al uso de CPU del hardware.

La evaluación fue mediante renders y análisis de señal. Falta escucha y medición en el instrumento físico. El emulador, instalador y catálogo Android BYTE se actualizan juntos; el APK y los motores de Jiant FM se conservan.
