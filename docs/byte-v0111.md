# BYTE v0.11.1 — corrección aritmética de FUNC

Se corrigen dos errores confirmados en los helpers musicales: la envolvente de cuatro beats de `chord` desbordaba su producto de 32 bits y se fragmentaba en lugar de decaer continuamente; la multiplicación de armónicos de `note` podía desbordar con COLOR alto. Ahora ambas operaciones están acotadas sin añadir división de 64 bits a la envolvente. No se aumenta la ganancia ni se modifican las composiciones, LIFE o GEN.

El reporte «FUNC no suena o es casi ruido imperceptible» todavía no se reproduce en el recorrido probado. En v0.11 pública, arranque limpio → EDIT+PRESETS hasta MATH → PRESETS hasta FUNC SONG → PLAY produjo RMS 0,01036 y pico 0,04633 en la salida WebAudio real, sin normalización. Coincidió con el motor native; FUNC BEAT también produjo señal tonal y percusiva. Cambiar WAVE a FUNC desde BYTE BENT, MATH BASS e INIT FLOAT produjo audio. Estas mediciones no sustituyen una prueba en el dispositivo del usuario: falta identificar plataforma, programa/estado y recorrido exacto del caso silencioso.

`tests/func_music_test.c` recorre todas las edades de las envolventes y compara los armónicos con una referencia de precisión ampliada. `tests/funcsong_web_test.c` arranca el emulador desde cero, selecciona presets mediante botones/encoder y renderiza salida sin normalizar; no fuerza filtros, envolventes o niveles para obtener audio. Se comprueban además transporte/composición y paridad native/WASM. La corrección conserva el archivo de 22 referencias, APK y perfiles FM.

Verificación final: UBSan sin errores; 529200 frames de cada preset SONG/BEAT idénticos entre native y WASM usando el recorrido de botones/encoder. AC79: imagen 552152/584336 B, data+BSS 95632/98304 B, pool libre 9248 B y todas las guardas aprobadas.
