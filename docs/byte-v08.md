# BYTE v0.8 — LIFE con recorrido audible

LIFE v0.7 multiplicaba su intensidad por el master, cada control, la envolvente del donante y el nivel de su pista. En canciones de GEN, LIFE 50 acababa aportando apenas 2–3% de fragmentos. NUM, además, aplicaba una cúbica que en fórmulas suaves se parecía más a cambiar el volumen que el timbre.

Ahora las envolventes y LEVEL ponderan qué notas participan en la red; no vuelven a apagar su profundidad. Todas las conexiones WAVE↔MATH↔DRUM-X y las notas superpuestas de un mismo motor siguen activas. GEN sigue arrancando con LIFE 0.

- **LIFE** conserva el taper amplio: su valor interno a 50 es19%, a 75 es49%, a 100 es100%. La mezcla de formas usa ese recorrido de manera audible: con NUM 65/FRAG 60/LINK 40, la profundidad objetivo de FRAG/LINK es aproximadamente 24% a 50 y 64% a 100. No depende del volumen de la pista donante. Los ataques/salidas se suavizan.
- **NUM** intercambia números de las fórmulas/VAR como coeficientes de una expresión de segundo/tercer armónico, afinada a la nota receptora. Cambia el timbre también cuando la fórmula original tiene poca amplitud. Los números no se escriben en divisores, máscaras ni relojes.
- **FRAG** intercambia formas armónicas normalizadas procedentes de expresiones válidas. Se conserva la limitación a ocho armónicos y el límite por frecuencia de la nota.
- **LINK** deforma esa forma con fase y profundidad limitadas; no vuelve a introducir audio crudo en el reloj.

La redistribución armónica lleva una compensación de energía suave y limitada a 1.5×, con protección de pico: el extremo no debe confundirse con simplemente bajar el volumen. Las fórmulas y patches originales permanecen intactos. LIFE 0 y los tres controles en 0 siguen siendo bypass exacto. Una voz sin otra nota activa no tiene con quién intercambiar; la introducción solista de FUGA es un ejemplo de ese comportamiento correcto.

## Escuchar y usar

En **HOME → LIFE**, probar **LIFE 50, NUM 65, FRAG 60, LINK 40**, luego subir LIFE a 75–100. Hacen falta notas superpuestas; las líneas de la página muestran las conexiones activas. Para comparar formas, subir FRAG y tocar dos sonidos distintos.

[Comparaciones GEN a igual RMS: LIFE 0 /50 /100](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v08/). Son frases del generador real (seed 37, energy 65), no sólo senos de laboratorio: Minimal Techno, IDM Ambient y FUGA después de que entra otra voz. Se iguala el volumen de cada frase completa, sin modificar sus dinámicas internas.

## Verificación

- Recorrido UI→GEN→PLAY→LIFE 0/25/50/75/100 con controles por defecto. FUGA se renderiza 72 s y cruza la entrada polifónica; LIFE no se pierde al cambiar de sección.
- Frente a v0.7, el cambio a igual RMS en LIFE 50 crece de 0.014 a 0.092 en Minimal y de 0.020 a 0.138 en Ambient. FUGA llega a 0.168 después de la introducción. En 100: 0.332/0.778/0.616 respectivamente. Son medidas de diferencia, no una escala perceptual universal; los audios permiten evaluar el resultado.
- El RMS sin igualar en 50 queda en 98–99% del original en esas frases; en 100, 78–91%. Las pruebas de red tonal mantienen energía por encima de 6 kHz inferior a 0.009%; no se acepta un simple cambio de volumen como modulación.
- Regresiones de donante activo pero suave, seis direcciones, múltiples fuentes, retirada de notas, bypass, fuentes sin mutar, fase, control de agudos, FUGA, MY LOOP y límite de voces.

La prueba física de sonido/CPU en el instrumento sigue pendiente. Esta actualización afecta BYTE; no cambia el APK ni los motores FM.

Reproducir las comparaciones: `sh tests/run_byte_tests.sh` y `sh tests/run_life_song_tests.sh`.
