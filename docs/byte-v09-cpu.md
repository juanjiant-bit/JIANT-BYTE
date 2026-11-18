# Auditoría CPU y voces — BYTE v0.9

Auditoría del firmware `60f36af`, 2026-11-18. No se encontró un fallo funcional en los límites de voces ni en las pruebas de carga realizadas. No es una certificación de rendimiento del FM-1: no hubo instrumento físico conectado. No se cambia ni se publica un firmware nuevo por esta auditoría.

## Voces y protección

- **VOICES 1–8 es un límite por pista**, no una reserva de ocho voces para cada una. Se comprueba al asignar voces y al renderizar. Al reducirlo, las sobrantes salen mediante el fade existente y después dejan de ejecutar síntesis. POLY, UNISON y DRUM-X respetan el límite; el valor se guarda con el proyecto.
- Existe un presupuesto global compartido: **8 normalmente, 6 en ECO y hasta 12 en FULL** sólo con dos núcleos sanos y margen medido. La admisión de 12 exige 16 mitades de audio con promedio inferior a 50%; se cierra por encima de65%, si actúa el protector o se pierde el segundo núcleo. Cerrar la admisión no corta de golpe notas existentes.
- El presupuesto global admite excepciones para conservar ataques importantes y voces principales. Además, las voces que están terminando su fade siguen procesando unos milisegundos: un pico de12 activas con presupuesto8 no significa12 voces sostenidas. Las filas del CSV distinguen máximos activos y máximos en fade; esos máximos no tienen por qué coincidir en el mismo instante.
- Kick, snare y clap están protegidos frente al robo global/por sobrecarga. El límite explícito de la propia pista sigue mandando: bajarlo puede cortar capas de un kit. Para batería simultánea, mantener más voces que para una línea monofónica.
- El costo inicial por motor es una estimación. En el hardware, el reparto entre núcleos y la predicción incorporan el tiempo real medido de cada pista, incluyendo su procesamiento LIFE. La agregación de donantes está fuera del temporizador por pista, pero dentro del tiempo total de audio y de su protector.

## Mediciones

58 escenarios nativos, cada uno con3 segundos de audio: 2/4/6 pistas, límites1/4/8, LIFE0/50/100, con/sin automatización USER. Batería con ataques simultáneos, notas superpuestas, colas, reverb y delay. Los escenarios normales combinan DRUM-X, WAVE y MATH; USER sustituye las pistas melódicas por MATH USER con LFO→SEED y ENV→MUTA. Se mide la ruta real `mix_block`; la generación de fórmulas se mide por separado en `math_poll`.

| Pistas, límite8 por pista | LIFE0: ms por segundo de audio | LIFE100: ms por segundo de audio |
|---|---:|---:|
| 2, presupuesto global8 | 8,17 | 27,37 |
| 4, presupuesto global8 | 8,79 | 26,39 |
| 6, presupuesto global8 | 8,45 | 27,37 |
| 6, presupuesto12 admitido para estrés | 11,60 | 45,61 |

Estos tiempos pertenecen al **host nativo**, no a la CPU del instrumento. La similitud de 2/4/6 pistas en presupuesto8 es esperable: se reparte el mismo total de voces, no se multiplican las voces por el número de pistas. Los escenarios de12 fuerzan la admisión para medir carga; no simulan un detector de margen físico.

El máximo medido fue45,84 ms/s de audio más6,32 ms/s de generación de fórmulas. USER realizó185 reconstrucciones en3 s en el caso más activo (~62/s entre todas las pistas). No compila dentro del callback: las solicitudes se agrupan, cada pista espera al menos32 ms y el conjunto al menos16 ms. La publicación copia bytecode ya validado con interrupciones deshabilitadas; en la ruta normal, el worker del segundo núcleo termina antes de que vuelva a ejecutarse main.

**LIFE es el costo adicional dominante:** con presupuesto12, LIFE100 cuesta aproximadamente3,9× el audio de LIFE0 en este escenario. Reducir su intensidad no reduce proporcionalmente la CPU: la captura/proyección de fórmulas sigue trabajando mientras LIFE esté activo. Una única voz también prepara su representación aunque aún no haya otro donante. Es una oportunidad de optimización, no un fallo de asignación; esta auditoría no cambia el sonido.

Se adjunta además un contraste WASM/Node en seis escenarios. El mayor caso de esa tabla fue69,23 ms/s de audio más11,86 ms/s de main. Sus temporizadores y JIT no son equivalentes al hardware ni a la medición nativa; se reportan por separado. Los CSV contienen p95 del bloque, voces activas, fades, reconstrucciones y RMS, además del tiempo medio. Son mediciones de esta máquina, no umbrales universales.

## Comprobaciones y reproducción

Se repitieron las pruebas existentes `voice_limit` y `byte_automation`: límites, retirada al bajar VOICES, UNISON, DRUM-X, persistencia, automatización efectiva, coalescencia y parámetros originales intactos. La auditoría del protector verifica presupuesto8/12/ECO6, un pico aislado sin disparo, sobrecarga sostenida, recuperación y protección de las tres voces principales de batería. La histéresis de admisión de12 se revisó en `audio.c`; no se afirma haber medido su reloj en hardware.

```sh
cc -O2 -w -Wno-error=implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests tests/audit_byte_cpu.c -lm -o build/byte-check/audit_byte_cpu
build/byte-check/audit_byte_cpu --guards
python3 tests/audit_byte_cpu.py
```

Para WASM, con el SDK de Emscripten activado:

```sh
emcc -O2 -w -Wno-error=implicit-function-declaration -ffp-contract=off -Ibuild/gen -Ifirmware/src -Itests -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sENVIRONMENT=node -sEXIT_RUNTIME=1 tests/audit_byte_cpu.c -lm -o build/byte-check/audit_byte_cpu.js
node build/byte-check/audit_byte_cpu.js 6 8 100 1 0
```

Argumentos: pistas, límite por pista, LIFE, automatización USER, admisión forzada de12. Tablas: [nativo](byte-v09-cpu.csv), [WASM](byte-v09-cpu-wasm.csv).

Para arreglos densos, **VOICES1 en bajos/solistas y2–4 en acordes** evita gastar voces innecesarias; conservar suficiente polifonía para las capas de batería. En el instrumento, comprobar el indicador CPU/ABOUT y posibles cortes con la canción real. No se recomienda aumentar el presupuesto global sin esa medición física.
