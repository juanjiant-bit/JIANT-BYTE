# BYTE v0.11 — una función que compone

FUNC INIT de v0.10 era un oscilador didáctico: A añadía un segundo armónico, B/C daban modulación lenta y D atenuaba. Los controles funcionaban, pero no ofrecían composición desde la ecuación.

La versión actual añade FUNC SONG y FUNC BEAT, con modo RUN SONG integrado en PLAY/STOP. El programa define melodía, acordes, percusión, repetición y secciones; el secuenciador no lo reataca en cada paso. La alternativa RUN NOTE conserva instrumentos y texturas por voz. FORM permite explorar plantillas sin borrar CUSTOM; aplicar texto nuevo sustituye CUSTOM de forma transaccional. Los proyectos conservan RUN/FORM/código/controles.

SONG tiene patrones de notas y armonía; BEAT deriva notas y ritmos con XOR, divisiones, restos y condiciones. Los helpers de C sólo sintetizan componentes acotados, no contienen el arreglo. A–D modifican armonía/registro, caja, hats y ritmo melódico; en BEAT D controla el periodo del bombo. [Código y controles completos](funcbeat.md).

## Verificación

`tests/funcsong_test.c` reproduce 33 segundos: más de 64 beats con una sola voz y contador de muestras continuo; STOP, reinicio, cambio de tempo, teclado aislado en SONG, presupuesto de helpers, conservación CUSTOM al explorar y guardado de proyecto. Genera composiciones y variaciones de A–D; `tests/funcsong_test.py` compara audio a igual RMS y envolventes rítmicas, y comprueba entrada/salida de secciones. La suite BYTE cubre además FVM/FUNC, LIFE, GEN, FUGA, MY LOOP, voces, DRUM-X y automatización.

`tests/funcbeat_web_test.c` y `web/emu/func_test.mjs` comparan NOTE y SONG entre native/WASM mediante el flujo real de arranque, compilación, nota o PLAY. El editor conserva el programa al rechazar errores. Se verifican los hashes del emulador, instalador y catálogo publicados.

El compilador mantiene 255 caracteres/64 instrucciones/dos estados. Los helpers tienen presupuesto adicional de ocho componentes por programa (un acorde cuenta cuatro). La estimación de admisión y reparto entre cores incluye ese coste; el target conserva las guardas de memoria. No hay compilación, loops libres ni memoria dinámica en audio. El reloj musical interpola con enteros, sin divisiones de 64 bits en su bucle de muestras.

Esta entrega conserva LIFE v0.10 y GEN LIFE0. No cambia el APK ni FM. La escucha y carga de CPU en hardware físico siguen pendientes; las mediciones en computadora no equivalen a CPU del dispositivo.

El build AC79 ocupa 552120/584336 B, con data+BSS 95632/98304 B y 9248 B libres en el pool. La paridad native/WASM cubre 88200 frames NOTE y 529200 frames SONG.

[Archivo de las 22 fórmulas de referencia](references/bytebeat-floatbeat-funcbeat.txt): conserva las expresiones y sus créditos, junto con criterios para nutrir GEN con familias de ritmos, armonías y estructuras. Esta entrega no importa JavaScript arbitrario ni incorpora automáticamente las 22 referencias al generador.
