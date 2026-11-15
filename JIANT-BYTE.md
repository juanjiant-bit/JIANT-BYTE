# JIANT BYTE — plan

Firmware alternativo para el M-VAVE FM-1 donde todo el sonido sale de fórmulas: bytebeat y floatbeat a fondo.
Es un fork de JIANT FM V10.7.36 (commit `7c771ab`, con toda su historia). JIANT FM es un fork de
[Felucca](https://github.com/hugelton/Felucca) 1.1.5.1 (Hügelton Instruments) y toma su sistema de canciones de
[SLOOP](https://github.com/isod89/sloop-fm1). Sigue siendo GPL-3.0-only.

Estado: **plan, sin código todavía**. Cada fase se aprueba antes de empezarla.

---

## 1. La idea

Un instrumento donde una fórmula de `t` es a la vez el ritmo, la melodía y el timbre. En JIANT FM, MATH es un
motor más, con 32 fórmulas bytebeat y 32 floatbeat fijas (`firmware/src/eng_lofi.c`). En JIANT BYTE las fórmulas son
el sistema entero:

- **Motores**: todos los sonidos salen de fórmulas que se pueden escribir, guardar, mutar y secuenciar.
- **Drum machine**: cada lane es una fórmula que reinicia `t` en cada golpe.
- **Generador**: inventa fórmulas, las muta y cruza dos entre sí.
- **Efectos punch-in**: actúan sobre el tiempo (`t`), no solo sobre el audio.
- **Secuenciadores**: bloquean las variables de la fórmula por paso, y una fórmula también puede ser el secuenciador.
- **Estética demoscene / pixel**: fuente pixel 5x7, alto contraste, rasters y glitch, y `t` y las máscaras en binario
  a la vista.

## 2. Qué se conserva de JIANT FM

Todo lo que ya está probado por debajo de la música:

| Capa | Archivos |
|---|---|
| HAL, arranque, interrupciones, los dos núcleos | `firmware/hal/`, `crt0.S`, `main.c`, `cpu_safe.c` |
| Audio, mezcla, buses de efectos, master | `audio.c`, `fx.c` (se recorta), `master.c` |
| Voces y su presupuesto | `voice.c` |
| Almacenamiento con copia A/B y autosave | `storage.c`, `storage_hw.c`, `settings_persist.c` |
| Proyectos, canciones, TIMELINE, partes | `project.c`, `song_*.c` (el modelo de datos cambia, ver 6) |
| Secuenciador de 64 pasos, locks, motion, swing, ratchets | `seq.c`, `motion.c`, `seq_live.c` |
| MIDI (USB y TRS), clock, NRPN | `midi_*.c` |
| USB (audio de entrada, stems, consola) | `usb.c` |
| Instalador y actualización | `ota.c`, `ota_hw.c`, `firmware/loader/`, `tools/fm1pkg_make.py` |
| Emulador web y suite de tests | `web/emu/`, `tests/` |
| Dibujo de bajo nivel | `lcd.c`, `gfx.c` |

## 3. Qué se reemplaza

Medido en V10.7.36 (`build/felucca.elf`, tamaños por módulo; aproximado: incluye lo que se copia a RAM):

| Sale | Flash aprox. | Lo reemplaza |
|---|---|---|
| Motores: ANALOG, PHASE, VOICE (+ `sing.c` y el diccionario), WHEEL, STRING, WEST, FM6, ADD, ANIMAL, formantes | ~75 KB | Motores de fórmulas (sección 5) |
| DRUM-X (`drumx_voice.c`, `eng_drum.c`) | ~11,5 KB | Drum machine de fórmulas |
| GENE, el generador de géneros (`gen.c`, `gen_loop.c`, `gen_tables.c`, mapas de batería) | ~50 KB | Generador de algoritmos |
| Visualizaciones de motores (`ui_engvis.c`) | ~31 KB | Visualizaciones de bits y de la onda |
| Pantallas y estilo (`ui_*.c`, fuentes, íconos, keycaps, paletas) | ~130 KB | La interfaz demoscene / pixel |
| Punch-ins actuales (`perform.c`) | ~8 KB | Punch-ins sobre `t` |

Hoy V10.7.36 deja **1.988 B de flash** y **184 B de RAM estática** libres. Sacar lo de arriba libera del orden de
150 KB de flash antes de sumar lo nuevo. EXT (la entrada USB) y SWARM (Bluetooth) quedan en duda: ver 9.

## 4. La máquina virtual de fórmulas (FVM)

El corazón. El chip no tiene FPU (todo el DSP de JIANT es punto fijo Q15, `firmware/src/dsp.c`), así que el
floatbeat se hace en punto fijo.

**Un solo lenguaje, dos tipos.** La sintaxis es la de C y JavaScript, como en los bytebeats que circulan. Cada
expresión tiene un tipo que se decide al compilar, sin costo en tiempo de ejecución:

- **INT**: entero de 32 bits sin signo, con la semántica del bytebeat clásico de C. `t*(42&t>>10)` suena igual que
  siempre.
- **FIX**: punto fijo Q16.16 con signo, para el floatbeat. Un literal con punto (`0.5`) es FIX.
- Si se mezclan, INT pasa a FIX. Las operaciones de bits (`& | ^ << >> ~`) usan la parte entera, como `ToInt32` en
  JavaScript. INT × FIX conserva los 32 bits de `t`; la parte entera da la vuelta, pero la fracción sigue exacta, y
  eso alcanza para las funciones periódicas.
- La salida depende del tipo final: INT suena como bytebeat (`& 255`), FIX como floatbeat (recortado a −1..1).

**Variables:**

| | Tipo | Qué es |
|---|---|---|
| `t` | INT | el reloj de la fórmula, al ritmo de la nota (C4 ≈ 8 kHz, como hoy en MATH) |
| `w` | FIX | la fase de la nota en vueltas: `sin(w)` es la nota afinada |
| `T` | FIX | segundos desde la nota |
| `a b c d` | INT | las cuatro variables de knob o de paso, 0..255 |
| `A B C D` | FIX | las mismas, 0..1 |
| `n` `v` | INT | la nota y la velocidad |
| `s` | INT | el paso del secuenciador |
| `x` `X` | INT / FIX | la salida de un bloque antes (realimentación) |

**Funciones**: `sin tri saw sqr` con período de una vuelta. Con un argumento INT dan un byte (256 por vuelta); con
uno FIX dan −1..1. También `abs min max int fix rnd`, y las tablas de texto del bytebeat: `"36364689"[t>>11&7]`.

**Ejecución vectorizada por bloques.** Cada instrucción procesa 32 muestras de una vez. El costo de interpretar se
paga una vez por bloque y no por muestra; lo que queda es la aritmética. Además el compilador pliega constantes y usa
operandos inmediatos (`t>>10` es una sola instrucción), y la división o el módulo por cero dan 0. Las fórmulas
se guardan como texto y se compilan al cargar.

**Lo que hay que medir en la fase 1**: instrucciones por muestra y por voz frente a las fórmulas fijas actuales
(`lofi_byte`, `lofi_float`), en el host con valgrind y en el binario del chip (`tests/target_budget.py`).

## 5. Lo que se construye sobre la FVM

**Motores:**
- **BYTE**: bytebeat de 8 bits, con CHIP (bits), GRIT (retención), LOOP, MOTION y mutaciones de `t`, como el MATH
  actual pero con fórmulas propias.
- **FLOAT**: floatbeat en Q16.
- **TONE**: la fórmula se renderiza a una tabla de onda por nota. Es barato y permite polifonía real.
- **FM-F**: una fórmula modula la fase de otra.
- **GLITCH**: granular sobre `t`, con saltos, ventanas y retrocesos.

**Drum machine.** Cada lane es una fórmula con `t` reiniciado en el golpe, su envolvente, y un morph A↔B entre dos
fórmulas o entre sus variables. Los mutes por grupo (KICK, SNARE, HAT, PERC) se conservan de JIANT FM.

**Generador de algoritmos:**
- arma fórmulas con una gramática;
- las muta (cambia una constante, un operador o un desplazamiento);
- cruza dos fórmulas;
- bloquea partes de una fórmula para que no muten;
- descarta lo que sale en silencio, con DC o saturado, con un análisis corto antes de proponerlo.

**Punch-in sobre el tiempo.** Como el sonido es una función de `t`, el efecto toca `t`:
- stutter (`t & ~mask`)
- reverse
- tape-stop (`dt` que frena)
- retrigger
- XOR y AND sobre la señal
- la salida de vuelta en `t`

Entran al pulso, como los punch-ins de hoy.

**Secuenciadores:**
- pasos que bloquean `a b c d` y saltos de `t`;
- **fórmula como secuenciador**: una expresión (`t>>12`, `"…"[s]`) genera notas cuantizadas a la escala.

## 6. Datos: proyectos y almacenamiento

- **Identidad del paquete propia**, para que el instalador y la detección no la confundan con JIANT FM
  (`tools/fm1pkg_make.py --product`).
- **Magia de almacenamiento propia** (hoy `ST_MAGIC` "FELU", `firmware/src/storage.c:16`). JIANT BYTE no lee
  proyectos de JIANT FM ni al revés, así ninguno de los dos interpreta mal los datos del otro. Instalar BYTE encima de
  FM borra en la práctica las canciones de FM, así que el instalador tiene que hacer el backup primero.
- **Un proyecto guarda el texto de cada fórmula.** Cuántas fórmulas y de qué largo entran por proyecto se decide con
  números en la fase 4.

## 7. La estética

- La fuente pixel 5x7 que ya existe (`tools/gen_px_font.py`) como base, más un tamaño grande pixelado para los
  valores.
- Paletas de alto contraste: negro con un color de fósforo, y una invertida.
- Rasters, scanlines y glitch como lenguaje visual, sin que tapen la lectura.
- A la vista en vivo: `t` en binario, la máscara activa, la fórmula y la onda.
- Las pantallas se redibujan con el cuidado de JIANT: la suite de render chequea texto cortado, solapado o fuera de
  pantalla en cada pantalla y cada paleta (`tests/ui_render.c`).

## 8. Fases

Cada fase termina sonando y probada en el emulador web antes de pensar en flashear. Un commit (o PR) por fase.

| Fase | Qué | Listo cuando |
|---|---|---|
| 0 | Este plan; renombre del producto (identidad, magia, versión, splash) | El firmware se llama JIANT BYTE, compila y pasa la suite |
| 1 | FVM: compilador y VM vectorizada; las 64 fórmulas actuales portadas a texto | Bit a bit igual que `lofi_byte` / `lofi_float`; costo medido en host y en el chip |
| 2 | Motores BYTE y FLOAT sobre la FVM, reemplazando MATH; salen los motores de JIANT FM | Suenan en el emulador; flash liberada medida |
| 3 | Drum machine de fórmulas, con mutes por grupo | Un kit entero de fórmulas sonando desde el secuenciador |
| 4 | Proyectos con fórmulas; edición de fórmulas en el aparato | Guardar, cargar y backup de un proyecto con fórmulas propias |
| 5 | Generador de algoritmos | Genera, muta y cruza; el filtro descarta silencio, DC y saturación |
| 6 | Punch-ins sobre `t`; secuenciadores de fórmula | Stutter, reverse y tape-stop al pulso; fórmula como secuenciador |
| 7 | Estética demoscene / pixel en todas las pantallas | Suite de render limpia en cada pantalla y paleta |
| 8 | TONE, FM-F y GLITCH | Si entran en CPU y flash |

## 9. Decisiones abiertas

- **EXT y SWARM**: ¿se quedan la entrada de audio USB y el instrumento Bluetooth? Ocupan flash (`eng_input.c` ~7 KB,
  más la radio).
- **Editor de fórmulas en el aparato**: con 4 knobs y teclas, ¿por tokens (elegir operador, variable o número) o solo
  el editor web y el generador?
- **El editor web y el instalador**: se adaptan para BYTE (otra identidad), o BYTE usa los de JIANT FM con un modo.
- **Seguir los arreglos de JIANT FM**: cada arreglo de la base se porta a mano (es un repo aparte). Ver de vez en
  cuando los cambios de `codex/*` en JIANT.

## 10. Estado de la base (verificado el 2026-11-15)

- V10.7.36 compila: imagen 582.348 B de 584.336 B; RAM `.data+.bss` 98.120 B de 98.304 B.
- Suite completa: pasan las 219 renders de referencia bit a bit, la salud del audio, las voces, el CPU, el
  almacenamiento, el backup, la interfaz, cada motor y el emulador (2,0 % del tiempo real).
- Fallan 6 tests, los mismos en la estable V10.7.23. Son tests que quedaron atrás del firmware o problemas del
  entorno, no defectos nuevos:
  - el guardado de preset del emulador (la secuencia SAVE, OCT+, OCT+ cambió en V10.2);
  - T4 en los stems del emulador;
  - `tests/text_spacing_test.py`, que no lee el formato actual de las fuentes;
  - el mapa de navegación de `docs/`, desactualizado;
  - fx-rest, que falla solo en la build de referencia `-DFX_REST_OFF`;
  - la app Android, que no compila sin red a Maven.
