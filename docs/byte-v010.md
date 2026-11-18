# BYTE v0.10 — FUNC y extremo de LIFE

MATH incorpora **FUNC**: programas propios con variables locales, estado por voz y pequeñas funciones, ejecutados por la VM nativa del instrumento y por el mismo código en WASM. No es un intérprete de JavaScript. [Sintaxis, controles, límites y ejemplos](funcbeat.md).

**Inicio:** seleccionar el preset **FUNC INIT**, o MATH → WAVE: FUNC. A/B/C/D se asignan en el programa y son destinos reales de la matriz. En la página del motor, OCT+ abre el editor; OCT+ aplica, OCT− cancela. En el navegador hay además un editor de texto FUNC debajo del instrumento. Guardar **el proyecto** conserva el programa y sus controles; el registro corto de sonidos USER no contiene programas FUNC y avisa que se debe guardar el proyecto.

LIFE mantiene la respuesta de 0–75. En el último cuarto, FRAG redistribuye los fragmentos espectrales recibidos hacia múltiplos armónicos, NUM utiliza los valores intercambiados para variar esa distribución y LINK incorpora la llegada de golpes y la superposición de voces. Todos los donantes activos participan; FUNC usa copias de estado durante el análisis, sin adelantar la memoria real. La transformación máxima está limitada por la altura de la nota para evitar reintroducir una capa de ruido agudo. Una sola voz sigue sin intermodularse consigo misma. GEN continúa con LIFE en cero.

## Evidencia

- FVM/FUNC: estado por muestra, invariancia ante tamaño de bloque, aislamiento entre voces, reset, tono 440Hz, argumentos de helpers evaluados una vez, controles y rechazo de programas fuera de presupuesto.
- UI/proyectos: error de compilación conserva el programa anterior; matriz A/B/C/D; cambio USER↔FUNC; seis programas de 255 caracteres guardados y recargados íntegros.
- Audio FUNC: 88200 frames idénticos entre compilación nativa y WASM; nota real y muestras finitas. Editor web verificado con aplicar, error y lectura posterior.
- LIFE: red entre motores y mismas voces, bypass y fuentes silenciadas, reloj base, respuesta gradual y controles independientes. En la escena WAVE+FUNC+DRUM, la comparación previa/posterior es idéntica hasta 75. El extremo transforma el sonido sostenido y los golpes kick/snare/clap cambian el receptor durante sus primeros 100ms, incluyendo el kit clásico. La comparación se normaliza por RMS para separar timbre de volumen; no sustituye una evaluación de escucha en el instrumento físico.
- `tests/life_extreme_test.py` publica métricas y tres WAV del receptor aislado: LIFE 0/75/100. Los donantes no se mezclan en esos WAV, así se puede oír su efecto sobre esa voz.

Ejecutar `sh tests/run_byte_tests.sh`, compilar `tests/funcbeat_web_test.c` como los otros tests web y correr `node web/emu/func_test.mjs build/emu/felucca.wasm NATIVE.f32`. La suite también cubre generación, FUGA, MY LOOP, voces, DRUM-X y automatización USER.

Presupuestos AC79 comprobados por el build: imagen 549520 B de 584336; BSS/data 95616 B de 98304; pool 9248 B libres; LOWBUF 32660 B de 32768. No hay compilación de programas en el callback de audio, bucles libres ni asignación dinámica. El límite de dos palabras de estado incluye `state`, `ph` y filtros con memoria. La carga real de CPU y la experiencia de edición siguen pendientes de comprobar en el hardware físico.
