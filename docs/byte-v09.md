# BYTE v0.9 — MATH automatizable y presets directos

En MATH USER, la matriz modificaba SEED/MUTA durante el bloque de audio, pero el generador de fórmulas leía después los valores originales. Ahora el audio comunica los valores efectivos y el hilo principal genera, valida y publica la fórmula completa. Los parámetros guardados y el texto original permanecen intactos.

## Controles

- Elegir **MATH → WAVE: USER**. En **MOD MATRIX**, seleccionar **SEED** o **MUTA** como destino y LFO, ENV u otra fuente. SEED recorre 0–191; MUTA, 0–127. Sin texto personalizado, SEED elige la fórmula generada; con texto personalizado, participa en la elección de mutaciones cuando MUTA es mayor que cero.
- La automatización de fórmulas es discreta: se conserva la solicitud más reciente, con un máximo de una actualización cada 32 ms por pista y cada 16 ms entre todas las pistas. No se compila dentro del callback de audio. Una transición de 128 muestras suaviza los cambios; no convierte el intercambio de fórmulas distintas en una modulación continua de oscilador.
- **PRESET** carga inmediatamente el sonido siguiente/anterior y vuelve al principio/final del mismo motor. La página actual se mantiene y se muestra el nombre del sonido.
- **Mantener EDIT y girar PRESET** salta de motor, como Jiant FM 49.
- **SAVE → PRESETS**, o mantener **SAVE y girar PRESET**, abre explícitamente la lista. Las notas sostenidas continúan al probar presets y se liberan normalmente.

LIFE conserva el comportamiento de v0.8 y GEN sigue con LIFE en cero. Esta actualización no cambia el APK ni los motores FM.

## Verificación

`sh tests/run_byte_tests.sh` incluye pruebas de audio LFO→SEED y ENV→MUTA, rutas con cantidad cero idénticas al bypass, límites de parámetros, coalescencia de solicitudes, parámetros originales y rutas conservados al guardar, y gestos PRESET/EDIT/SAVE. Se comprueban además notas sostenidas y su liberación al cambiar preset.

Las compilaciones y medidas en computadora no sustituyen una prueba de CPU/audio en el instrumento físico.

Resultado de esta entrega: suite BYTE completa y generador USER aprobados; audio nativo/WASM idéntico en 88.200 frames. Firmware AC79: imagen 544.744 B, BSS/data 95.424/98.304 B y pool con 12.000 B libres. El rebuild USER promedió 0,041 ms en el host; esa cifra no mide la CPU del dispositivo.
