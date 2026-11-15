# JIANT BYTE — plan

Firmware alternativo para el M-VAVE FM-1 donde todo el sonido sale de fórmulas: bytebeat y floatbeat a fondo.
Es un fork de JIANT FM V10.7.37 (commit `c623362`, con toda su historia). JIANT FM es un fork de
[Felucca](https://github.com/hugelton/Felucca) 1.1.5.1 (Hügelton Instruments) y toma su sistema de canciones de
[SLOOP](https://github.com/isod89/sloop-fm1). Sigue siendo GPL-3.0-only.

Estado: **plan, sin código todavía**. Cada fase se aprueba antes de empezarla.

---

## 1. La idea: todo es una ecuación

Una identidad nueva del sistema: **todo lo que pasa en JIANT BYTE lo rige una ecuación**. En JIANT FM, MATH es un
motor más, con 32 fórmulas bytebeat y 32 floatbeat fijas (`firmware/src/eng_lofi.c`). En JIANT BYTE no hay motores,
LFOs, matriz de modulación, mezclador ni efectos escritos aparte: hay fórmulas de floatbeat (y bytebeat) en cada
lugar de la cadena, todas en el mismo lenguaje y la misma máquina.

| Rol | Qué hace la fórmula | Reemplaza en JIANT FM |
|---|---|---|
| **Fuente** | el sonido de una pista o de un lane de batería | los 12 motores, DRUM-X |
| **Modificador** | una señal de control que mueve variables de otras fórmulas | LFOs, envolventes de modulación, la matriz (`mod.c`) |
| **Cross-mod** | una fórmula que lee la salida de otras pistas | (no existe) |
| **Mezcla** | la suma de las pistas, como una ecuación del bus | el mezclador fijo |
| **Efecto** | una fórmula sobre la señal que entra, con memoria del pasado | los efectos de inserto y de envío (`fx.c`) |
| **Punch-in** | una fórmula de efecto que entra al pulso mientras se mantiene una tecla | los punch-ins (`perform.c`) |
| **Secuencia** | una fórmula que genera notas, pasos o variaciones | parte de los generadores |
| **Generación** | fórmulas que inventan, mutan, cruzan y mezclan fórmulas | GENE (`gen.c`) |

Tres principios más ordenan todo:

1. **Del tono al caos.** Cada fórmula, y el sistema entero, va de lo más tonal y controlable a lo más salvaje y
   extremo, con un solo control continuo. En el extremo tonal suena afinado, en la escala, a tempo y con un nivel
   estable. En el extremo salvaje corre libre, se realimenta, se rompe y satura. Ver 5.
2. **El tiempo de cada ecuación se controla.** De qué reloj sale `t`, a qué velocidad corre, dónde arranca, dónde se
   repite y cómo se deforma. Todo se puede bloquear por paso, automatizar o mover con otra ecuación. Ver 5.
3. **Todo lo que ya hace JIANT FM se queda.** El secuenciador, las canciones, la performance, MIDI, USB y el
   backup siguen ahí, y cada uno puede quedar gobernado por ecuaciones. Ver 2.

La interfaz también responde a esto: estética **demoscene / pixel**, con fuente 5x7, alto contraste, rasters y
glitch, y la ecuación, `t` y las máscaras en binario siempre a la vista.

## 2. Qué se conserva de JIANT FM

**Todo lo que JIANT FM ya hace para componer y tocar se queda**, y cada cosa puede pasar a recibir ecuaciones:

| De JIANT FM | Cómo entra la ecuación |
|---|---|
| Secuenciador de 64 pasos, piano roll y grilla de batería, componentes de paso, locks y automatización, copy/paste | cada paso puede bloquear variables (`a b c d`), saltos de `t` y la fórmula entera |
| EUCLID, MODES, MUTATE, PLAYHEADS, STRETCH, CHANCE, RATCH, MICRO, swing | la probabilidad, los golpes, el ratchet o el desplazamiento pueden salir de una fórmula (`"…"[s]`, `rnd`, `s%3`) |
| ARP, escalas, acordes, SCENE | la escala cuantiza la salida de las fórmulas de nota (el extremo tonal) |
| Canciones: 6 × 4 partes, escenas, SONG REC, TIMELINE, PARTS, CLIPS | cada parte puede cambiar fórmulas, modificadores, mezcla y caos |
| GLO › LIVE (MANO, RIELES), macros, el DJ filter | las macros pasan a mover variables y el control de caos |
| MIDI USB y TRS, clock, NRPN, velocidad y aftertouch de las teclas, OCT BEND | `v`, el aftertouch y cada CC llegan a las fórmulas como variables |
| USB audio (stems de cada pista, la entrada de la computadora), Bluetooth MIDI, PROX | la entrada es `i` en las fórmulas de efecto |
| Backup, editor web, instalador | también guardan y editan fórmulas |

GENE (el generador de géneros) y los motores salen; ver 3. Sus ideas no se pierden: el generador de ecuaciones
puede escribir una canción entera a partir de una lista de géneros en el extremo tonal.

Y todo lo que ya está probado por debajo de la música:

| Capa | Archivos |
|---|---|
| HAL, arranque, interrupciones, los dos núcleos | `firmware/hal/`, `crt0.S`, `main.c`, `cpu_safe.c` |
| Audio, el cableado de los buses, master (limiter) | `audio.c`, `fx.c` (solo el cableado: los efectos pasan a fórmulas), `master.c` |
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

Medido en V10.7.36, V10.7.37 suma ~1 KB (`build/felucca.elf`, tamaños por módulo; aproximado: incluye lo que se copia a RAM):

| Sale | Flash aprox. | Lo reemplaza |
|---|---|---|
| Motores: ANALOG, PHASE, VOICE (+ `sing.c` y el diccionario), WHEEL, STRING, WEST, FM6, ADD, ANIMAL, formantes | ~75 KB | Motores de fórmulas (sección 5) |
| DRUM-X (`drumx_voice.c`, `eng_drum.c`) | ~11,5 KB | Drum machine de fórmulas |
| GENE, el generador de géneros (`gen.c`, `gen_loop.c`, `gen_tables.c`, mapas de batería) | ~50 KB | Generador de algoritmos |
| Visualizaciones de motores (`ui_engvis.c`) | ~31 KB | Visualizaciones de bits y de la onda |
| Pantallas y estilo (`ui_*.c`, fuentes, íconos, keycaps, paletas) | ~130 KB | La interfaz demoscene / pixel |
| Punch-ins actuales (`perform.c`) | ~8 KB | Punch-ins de fórmula |
| Matriz de modulación (`mod.c`) | ~4 KB | Modificadores de fórmula |
| Efectos fijos (`fx.c`: reverbs, delay, chorus, buses) | ~44 KB | Efectos de fórmula, con primitivas de memoria (ver 5) |

Hoy V10.7.37 deja **912 B de flash** y **184 B de RAM estática** libres. Sacar lo de arriba libera del orden de
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

**Lo que la FVM necesita para cumplir todos los roles** (sección 1), además de las fuentes:

| | Qué es |
|---|---|
| `i` `I` | la señal que entra (INT / FIX), en las fórmulas de efecto y de mezcla |
| `o1`..`o6` `O1`..`O6` | la salida de cada pista, para cross-mod y para la ecuación de mezcla |
| `m1`..`m4` `M1`..`M4` | los modificadores: fórmulas de control que corren una vez por bloque, al ritmo del tempo |
| `p` `P` | la posición en el compás y en la canción, para que todo pueda ir a tempo |
| `z(k)` | la señal de hace `k` muestras, leída de una memoria propia de la fórmula (delay, comb, flanger, eco) |
| `lp(x,k)` `hp(x,k)` | filtros de un polo con estado, dentro de la fórmula (hechos en la fase 1, con `ph(f)`) |
| `hold(x,k)` | retención (sample and hold, reducción de frecuencia) |

Con memoria y estado, un efecto es una fórmula como cualquier otra: `I + z(T*4410&16383)*0.6` es un eco,
`sin(I*3+O2)` es una distorsión cruzada con la pista 2. Las operaciones con estado funcionan bien en la ejecución por
bloques, porque cada instrucción recorre sus 32 muestras en orden. Lo único que no se puede es que una muestra se
realimente en sí misma dentro de la misma fórmula: la realimentación pasa por `z(k)` o por un bloque de retraso.

La reverb larga es el caso más caro: escrita como fórmula puede no entrar en CPU. Si no entra, queda como primitiva
(`rev(x,k)`) que las fórmulas llaman, y su sonido lo sigue gobernando la fórmula.

**Fase 1, hecha** (`firmware/src/fvm.c`, `firmware/src/fvm_bank.c`, `tools/gen_fvm_bank.py`, `tests/fvm_test.c`):
- El compilador y la máquina, como se describen arriba. Los operadores se escriben una sola vez (`FVM_OPS2`,
  `FVM_OPS1`) y los usan tanto el plegado de constantes como la máquina, así que no pueden divergir.
- Llamadas con estado propio: `ph(f)`, un oscilador de `f` Hz con su fase en 32 bits (exacto también a velocidad de
  LFO), y `lp(x,k)` / `hp(x,k)`. Hasta 8 por fórmula.
- Límites: 64 instrucciones, pila de 12 bloques, 64 bytes de tablas de texto. Un programa ocupa ~600 B; el espacio de
  trabajo de la máquina, 1,5 KB por núcleo.
- **Las 48 bytebeat de MATH, bit a bit**: el texto sale del C de `lofi_byte` tal cual, y el test compara cada fórmula
  con cada VAR (`a` 1..16) en `t` 0..2^20 y en tiempos al azar: ~17 millones de muestras por fórmula, ninguna distinta.
- **Las 48 floatbeat de MATH, reescritas en FIX**: no son bit a bit (MATH las escribió a mano en Q15 con atajos propios),
  pero suenan igual: correlación media 0,999 y mínima 0,987 con `lofi_float`, el mismo nivel.
- **El reloj** (`fvm_clock`): KEY LIBRE es el tiempo de MATH bit a bit (con VAR, BEND y LOOP); ALTURA deja el ritmo fijo
  y la altura en la tecla; FIJO ignora la tecla; RATE, START, LOOP, WARP y SYNC con la posición de la canción.
- **El costo**: 116 instrucciones por muestra en promedio (valgrind, x86-64), contra 50 de MATH escrito a mano (21 en
  bytebeat, 79 en floatbeat): ~2,3 veces. El costo en el chip se mide en la fase 2, cuando el motor la use.
- Compila para el chip (clang 4.0.1 de JieLi, `__divdi3` está en su `compiler-rt`); sin usar, no ocupa flash.
- Fuzz: 200.000 textos al azar no rompen el compilador, y un texto rechazado deja un programa vacío.

**Fase 2, hecha en parte** (rama `phase2-engines`):
- **Salen WHEEL, ADD y la síntesis de FM6.** Sus números (7, 13, 12) suenan como MATH (`engines.c` `ENG_LIVE`,
  `eng_idx`, `ui.c` `trk_eng`): un sonido de un patrón o de GEN nunca queda mudo y no se abre una página de un motor que
  ya no está. De FM6 quedan sus patches, por los almacenamientos. Flash: de 908 B a 19.824 B libres; RAM `.bss`: de
  98.120 B a 84.008 B.
- **MATH suena a través de la FVM.** BYTE y FLOAT leen su fórmula de `fvm_bank.c`, compilada por pista cuando cambia
  WAVE o ALGO. BYTE es bit a bit el de antes (sus renders de referencia no cambian); FLOAT, con sus fórmulas en FIX,
  cambia apenas (7 presets y 2 mezclas, rehechas en `tests/golden.txt`). Costo: valgrind +4..15 % en las secciones con
  MATH; en el chip, el loop de `lofi_byte_render` 205 (antes 180). La FVM y el banco cuestan 11 KB de flash y 7 KB de RAM.
- **CAOS y KEY** (`P_CAOS`, `P_KEY`, en los dos parámetros que dejó el ROTOR de WHEEL; página EDIT > CAOS en MATH):
  - CAOS 0 es la fórmula tal cual. Al subir, una parte creciente de los tramos de tiempo se pliega, desde 48 aparecen
    saltos y desde 96 la salida vuelve al tiempo.
  - KEY: LIBRE (como MATH), ALT5..ALT19 (ALTURA con su SPLIT: los bits de `t` desde ese en adelante vienen de la canción,
    el ritmo queda fijo y la tecla cambia la altura) y FIJO (la tecla solo dispara).
  - `tests/byte_test.c` lo comprueba sonando: con FIJO, C4 y G4 suenan idénticos; con ALT9, el ritmo coincide y la altura
    es la de la tecla (x1,43 de C4 a G4).
- **Costo por motor** (valgrind, 8 notas, `tests/cpu_baseline_vg.txt`): MATH BYTE 2.273 instrucciones por muestra,
  MATH FLOAT 3.768, ANALOG 2.334, WEST 2.590 (antes MATH no se medía: la lista lo buscaba por su nombre viejo, LOFI).
- **Falta en esta fase**: sacar ANALOG, PHASE, VOICE, STRING, WEST, ANIMAL y EXT, y reescribir los tests que prueban lo
  retirado. Hoy fallan, además de los 10 de la base: DIGITAL→FM6 y la UI con `FELUCCA_FM4=1`; GEN (volumen de los
  géneros); la UI (páginas de FM6, LOAD LITE/ECO con "FM6 dos voces", la lista de presets); CC74/CC71 sobre FM6 y WHEEL;
  DJ FILTER y MICRO "bit a bit como 0.9.0" y TRACKS (sus canciones de referencia tienen pistas de FM6 o WHEEL); la
  animación bajo carga; KEY SPLIT por MIDI.

## 5. Lo que se construye sobre la FVM

**Motores:**
- **BYTE**: bytebeat de 8 bits, con CHIP (bits), GRIT (retención), LOOP, MOTION y mutaciones de `t`, como el MATH
  actual pero con fórmulas propias.
- **FLOAT**: floatbeat en Q16.
- **WAVE**: wavetable paramétrico. Ver abajo.
- **FM-F**: una fórmula modula la fase de otra.
- **GLITCH**: granular sobre `t`, con saltos, ventanas y retrocesos.

**WAVE: wavetable paramétrico.** Las tablas no son muestras guardadas: las escribe una fórmula
`f(x, k)`, donde `x` es la posición dentro del ciclo (0..1) y `k` la posición en la tabla (0..1). Las variables
`a b c d` (y los modificadores) le dan forma.
- **Una tabla de 64 cuadros** de 256 muestras, recalculada por partes cuando cambia un parámetro, sin cortar el
  sonido. Un cuadro por bloque de control: la tabla entera se rehace en unos 46 ms.
- **POS** recorre los cuadros, con interpolación entre dos vecinos. Se puede bloquear por paso, modular con una
  fórmula o mover con CAOS.
- **Contra el aliasing**, versiones de la tabla por octava, filtradas al escribirlas (mipmaps), así suena limpio en
  el registro alto.
- **Polifonía**, porque leer una tabla cuesta poco y la fórmula corre al escribir la tabla, no por cada muestra.
- **Del tono al caos**: en el extremo tonal, `f` solo da formas periódicas y suaves. En el extremo salvaje, POS corre
  con una fórmula de audio (`O1`, `t`), las tablas se reescriben mientras suenan y `k` puede salir de `t`.
- **Ejemplos**: `sin(x+sin(x*2)*k*a/64)` es FM que crece con la tabla, `(x*256^t>>8&255)` un bytebeat congelado en
  ciclos, y `tri(x*(1+k*7))` una tabla de pulsos y sincs.
- **Inspirado en los wavetables de software tipo Serum, con un giro floatbeat**: cada pieza que en esos sintes es
  fija acá es una ecuación.

  | Pieza | En un wavetable clásico | En WAVE |
  |---|---|---|
  | La tabla | dibujada, importada o escrita con una fórmula de la fase | `f(x, k)`, y además **CAPTURA**: cada cuadro graba 256 muestras de un floatbeat o bytebeat corriendo en el tiempo, así una fórmula "canción" se vuelve una tabla que evoluciona |
  | WT POS | un knob y su modulación | POS es una ecuación: `p`, un modificador, `O2`, o `t>>12&63` (un bytebeat que recorre la tabla a ritmo) |
  | Warp | una lista fija de deformaciones de la fase (sync, bend, PWM, mirror, quantize, FM desde otro oscilador) | **WARP es una fórmula de la fase**: `g(x, t)`, con las clásicas de fábrica (`x*k` sync, `x^(1+k)` bend, `x&~m` quantize, `x+O2*k` FM) y cualquier otra, incluso con bits de `t` |
  | Dos osciladores y su mezcla | A + B, FM / AM / RM entre ellos | dos tablas, y la mezcla es una ecuación (`A*B`, `A^B`, `sin(A+B*k)`) |
  | Unísono | voces desafinadas con BLEND y ancho estéreo | cada voz del unísono con su desafinación y además su desfase de `t`: la misma fórmula, unos pasos corrida, en cada voz |
  | Sub y ruido | un sub senoidal y una tabla de ruido | el sub es una fórmula (`sin(w/2)` o un bytebeat a la octava de abajo) y el ruido es `rnd` o cualquier bytebeat ruidoso |

  CAOS también rige acá: en lo tonal, tablas suaves, warp leve y unísono afinado; en lo salvaje, captura de
  fórmulas caóticas, WARP con bits de `t` y POS a velocidad de audio.
- **RAM**: 64 × 256 × 2 B = 32 KB por tabla, más los mipmaps. Sale del pool que liberan los motores de JIANT FM; cuántas
  tablas a la vez (una por pista, o compartidas) se decide midiendo.

**Drum machine.** Cada lane es una fórmula con `t` reiniciado en el golpe, su envolvente, y un morph A↔B entre dos
fórmulas o entre sus variables. Los mutes por grupo (KICK, SNARE, HAT, PERC) se conservan de JIANT FM.

**Generador de ecuaciones.** Funciona para todos los roles (fuentes, modificadores, efectos, mezcla):
- arma fórmulas con una gramática propia de cada rol;
- las muta (cambia una constante, un operador o un desplazamiento);
- cruza dos fórmulas, o las mezcla (`f*(1-k)+g*k` con `k` movido por un modificador);
- bloquea partes de una fórmula para que no muten;
- descarta lo que sale en silencio, con DC o saturado, con un análisis corto antes de proponerlo.

**Del tono al caos.** Un control continuo, **CAOS** (0..127), por fórmula, por pista y global (una macro), que
el generador, las mutaciones y la propia máquina respetan:

| CAOS | Qué hace el sistema |
|---|---|
| 0 (tonal) | `t` atado a la nota y al tempo; las notas que salen de fórmulas, cuantizadas a la escala; niveles normalizados, DC quitado; el generador solo propone fórmulas periódicas y en tono |
| medio | `t` puede correr suelto del tempo; mutaciones de `t` (las de VAR de hoy); cross-mod moderado; la escala se relaja a notas cercanas |
| 127 (salvaje) | `t` libre o realimentado, saltos y plegados de tiempo, realimentación de la salida, cross-mod fuerte, reducción de bits, el generador sin restricciones de gramática |

Las funciones de ayuda de la FVM hacen de barandas en el extremo tonal y se aflojan con CAOS: `q(x)` cuantiza a la
escala, `w` sigue la nota, la salida pasa por el limiter del master siempre (nada de clipping duro por accidente).

**Tiempo de las ecuaciones.** Cada fórmula tiene su reloj, con estos controles (todos lockeables por paso,
automatizables y modulables por otra fórmula):

| Control | Qué hace |
|---|---|
| SYNC | de dónde sale `t`: la nota (afinado), el paso, el compás, la canción, o libre |
| RATE | la velocidad de `t`: multiplicadores y divisores del tempo (x1/16 .. x16) o continuos (MOTION de hoy) |
| START | dónde arranca `t` en cada nota o golpe (el `BYTE_T0` de hoy, ahora por paso) |
| LOOP | una ventana donde `t` se repite: de toda la canción hasta unas pocas muestras (un tono) |
| RESET | cuándo vuelve a cero: nunca, cada nota, cada paso, cada compás, o con un trigger de otra fórmula |
| WARP | deformación de `t`: swing de la fórmula, plegados (`t ^ t>>k`), mutaciones (VAR), velocidad modulada |

Así una misma fórmula puede ser un tono afinado (SYNC nota, LOOP corto), un loop a tempo (SYNC compás) o una textura
que nunca se repite (libre, CAOS alto).

**KEY: qué hacen las teclas con el tiempo.** En un bytebeat, la nota y el ritmo viven en el mismo `t`: hoy, en MATH,
una tecla más aguda acelera toda la fórmula, el timbre y el ritmo juntos. KEY elige qué mueve la tecla:

| KEY | Las teclas | El tiempo |
|---|---|---|
| LIBRE (como hoy) | aceleran o frenan toda la fórmula | `t` corre a la velocidad de la nota |
| ALTURA | cambian solo la altura | fijo, a tempo: el ritmo no cambia con la tecla |
| FIJO | solo disparan (o eligen variaciones con `n`) | fijo, a tempo |

En ALTURA, `t` se arma con dos relojes: los bits altos (el ritmo, la frase) vienen del tempo, y los bits bajos (el
timbre, la altura) vienen de la nota. Un control SPLIT elige en qué bit se separan. Es una generalización de lo que hace
MOTION hoy (`firmware/src/eng_lofi.c`, `lofi_byte_render`: los bits lentos de `t` a su propia velocidad). En las
fórmulas de floatbeat, `w` (la fase de la nota) lleva la altura y `T` o `p` llevan el tiempo, así que ALTURA se cumple
sola. KEY se puede bloquear por paso como el resto del reloj.

**Modificadores y cross-mod.** Cuatro modificadores por proyecto (`m1`..`m4`), cada uno una fórmula de control:
`sin(p*0.25)` es un LFO a tempo, `"0369"[p&3]` una secuencia de valores, `abs(O1)` un seguidor de la pista 1.
Cualquier fórmula los lee como variables. Cross-mod es lo mismo con audio: una pista lee `O1`..`O6` y modula su fase,
su tiempo o su amplitud con la salida de otra.

**Mezcla.** La salida de la máquina es una ecuación: por defecto la suma de las pistas, pero puede ser
`O1*(1-M1)+O2*M1` (un crossfade que mueve un modificador), `O1^O3` (bits mezclados), o cualquier cosa.

**Efectos.** Cada pista tiene una fórmula de inserto y el master una o dos, con `i`, `z(k)` y los filtros con estado.
Vienen de fábrica las ecuaciones de un delay, un comb, un flanger, un bitcrusher, un fold, un ring y una reverb
(como primitiva si hace falta), y todas se editan, mutan y cruzan como cualquier fórmula.

**Punch-in.** Un punch-in es una fórmula de efecto que entra al pulso mientras se mantiene su tecla, y sale sin
click. Sobre el audio (`z(k)` congelado, retrigger de la memoria, crush, XOR con otra pista) y sobre el tiempo de las
fuentes, porque el sonido es una función de `t`:
- stutter (`t & ~mask`)
- reverse
- tape-stop (`dt` que frena)
- retrigger
- la salida de vuelta en `t`

**Secuenciadores:**
- pasos que bloquean `a b c d` y saltos de `t`;
- **fórmula como secuenciador**: una expresión (`t>>12`, `"…"[s]`) genera notas cuantizadas a la escala.

**Fase 3, hecha en su primera forma** (`firmware/src/eng_wave.c`, motor 13, el número de ADD):
- Dos osciladores, A y B, cada uno una fórmula `f(w, A)` de un banco de 16 (SINE FM2 FMR TRISY SAWSQ DRIVE RING FOLD ODD
  PULSE STEP BITS FORMT NOISE SINTR SATUR): `w` es la fase del ciclo y `A` la posición POS, que recorre los "cuadros"
  que la fórmula dibuja. POS la mueve la matriz (SHAPE) y CAOS (un bytebeat del tiempo de la nota).
- WARP de la fase: OFF SYNC BEND MIRROR QUANT FM (B en la fase de A), con AMT. MIX de A a B, DETUNE (tres voces), SUB.
- 8 presets. `tests/wave_test.c`: afinado (A4 440 Hz), POS, cada WARP, DETUNE y SUB comprobados sonando.
- Costo: 4.721 instrucciones por muestra con 8 notas en el preset más pesado (tres voces, dos osciladores), unas dos
  veces ANALOG.
- **Cambio de diseño por la RAM**: la tabla no se guarda, la fórmula se evalúa por muestra (una tabla de 64 cuadros son
  32 KB y quedan ~3 KB libres). Por eso faltan CAPTURA (grabar un floatbeat en la tabla) y las copias por octava contra
  el aliasing: vuelven cuando la fase 7 libere las líneas de los efectos fijos (decenas de KB en el pool).

**Fase 4, hecha en su primera forma** (DRUM-X con lanes de fórmula, `firmware/src/drumx_voice.c`):
- Cada lane puede ser una fórmula (`dx_form`): en EDIT > SOUND 3, la perilla MODE sigue después de sus 32 modos con 16
  fórmulas de batería (SINE SAT FM METAL BITS XOR NOISE CHIRP STEP RING STUT FOLD CRUSH TONE2 HAT BYTE).
- La fórmula reemplaza al oscilador afinado y el resto del lane sigue siendo DRUM-X: la envolvente de DECAY, el ruido y
  su filtro, el MORPH A↔B (que mueve PITCH y COLOR), los SENDs y los mutes por grupo. En la fórmula, `w` son las vueltas
  desde el golpe a la altura del lane (con el barrido de PMOD), `t` 32 por vuelta, `T` los segundos desde el golpe y
  `A` el COLOR. El tiempo arranca de 0 en cada golpe (dos golpes suenan idénticos).
- Se guarda en el proyecto, en un bloque `'D' 'F'` nuevo (0 si no hay ninguno), como los SENDs y los LAYERs.
- Cada lane tiene su programa de hasta 24 instrucciones: un programa ya no reserva 64 (`fvm_prog_t` apunta a su propio
  espacio), y una tabla de texto repetida se guarda una sola vez.
- `tests/dxform_test.c`: las 16 fórmulas compilan, suenan acotadas y distintas del oscilador, `t` vuelve a 0 en cada
  golpe, DECAY sigue mandando, y el kit con sus fórmulas va y vuelve por el proyecto.
- Quedan 2,7 KB de flash y 2,5 KB de RAM: lo próximo es sacar más motores de JIANT FM.

**Fase 9, hecha en su primera forma** (`firmware/src/fvm_gen.c`; MATH WAVE = USER):
- Una pista de MATH en USER toca su propia fórmula: el texto de la pista (el lugar de las letras de VOICE, que el
  proyecto ya guarda) o, si no tiene, la que inventa el generador con ALGO como semilla (SEED, 0..191) y CAOS:
  - CAOS 0..42, tonal: floatbeats afinados (senos en razones enteras de la nota, la envolvente del paso, melodías de una
    tabla) y bytebeats melódicos;
  - CAOS 43..85: las familias del bytebeat clásico;
  - CAOS 86..127: un árbol de operadores al azar, más profundo cuanto más alto.
  A veces suma una capa del mismo tipo (un parcial afinado, un `|t>>N`) para que cada semilla sea la suya.
- Cada fórmula pasa por un juez antes de sonar (se corre sobre un tramo de tiempo, al principio y más tarde): se
  rechaza la que sale en silencio, trabada, con mucho DC o sin cambio, y se prueba la semilla siguiente.
- La misma semilla y la misma zona de CAOS dan siempre la misma fórmula: el proyecto guarda la perilla, no el texto.
- Corre en el loop principal (`math_poll`), con el espacio de trabajo prestado de `proj_scratch`; el audio solo
  recompila cuando cambia la versión del texto.
- `tests/gen_formula_test.c`: en las tres zonas, 189..192 de 192 semillas pasan el juez a la primera; 153..170 de
  192 son distintas; la zona tonal sale casi toda en floatbeat (174 de 192) y la salvaje toda en bytebeat; una pista
  USER suena, cambia con la semilla y toca el texto propio tal cual.
- Falta: mutar y cruzar dos fórmulas desde la interfaz, el mismo generador para modificadores, efectos y mezcla, y
  editar el texto en el aparato.

**Fase 8, primer paso: CAOS MAX** (`pfx.c`, la tecla F#4 del layer FX, que era RING):
- Mientras se mantiene, todas las pistas de fórmula (MATH, WAVE) van a CAOS 127. Como LIFE MAX, se aplica bloque a
  bloque y se repone al final de cada uno, así que nunca queda guardado: al soltar, cada pista vuelve a su CAOS.
- `tests/byte_test.c` lo comprueba a través de la mezcla completa: suena como CAOS 127 (correlación ~0,97; no bit a bit
  porque con una tecla del layer FX apretada su cadena corre, neutra pero sin saltearse) y el CAOS vuelve exacto.
- Los punch-in de buffer (REVERSE, TAPE STOP, REPEAT, GLITCH, FREEZE) ya actúan sobre el audio de las pistas de
  fórmula. Falta: punch-ins que deformen `t` directamente y los secuenciadores de fórmula.

## 6. Datos: proyectos y almacenamiento

- **Identidad del paquete** (`FM-1_909`, `tools/build.py` `PRODUCT`): por ahora la misma que JIANT FM.
  [VERIFICAR] El instalador la comprueba después de instalar (`tools/fm1_install.py`, `web/fm1ota.js`), y no está
  documentado si el FM-1 o el actualizador oficial rechazan otros números. Se cambia recién cuando se pruebe en el
  aparato. Mientras tanto, JIANT BYTE se distingue por el nombre: el editor recibe "JIANT BYTE v0.1"
  (`firmware/src/editor.c:489`), y el menú y el Bluetooth dicen JIANT BYTE.
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
| 1 | FVM: compilador y VM vectorizada; las 64 fórmulas actuales portadas a texto; el reloj de cada fórmula (SYNC, RATE, START, LOOP, RESET, WARP) | Bit a bit igual que `lofi_byte` / `lofi_float`; costo medido en host y en el chip |
| 2 | Motores BYTE y FLOAT sobre la FVM, reemplazando MATH; CAOS por fórmula y global; salen los motores de JIANT FM | Suenan en el emulador, de lo tonal a lo salvaje; flash liberada medida |
| 3 | WAVE, el wavetable paramétrico (tablas escritas por fórmula y CAPTURA, POS, WARP, dos osciladores, unísono, sub, mipmaps, polifonía) | Suena limpio en todo el teclado; RAM y CPU medidas |
| 4 | Drum machine de fórmulas, con mutes por grupo | Un kit entero de fórmulas sonando desde el secuenciador |
| 5 | Proyectos con fórmulas; edición de fórmulas en el aparato | Guardar, cargar y backup de un proyecto con fórmulas propias |
| 6 | Modificadores, cross-mod y la ecuación de mezcla; sale la matriz de modulación | Un LFO, un seguidor y un crossfade hechos con fórmulas |
| 7 | Efectos de fórmula con memoria (`z(k)`, filtros con estado); salen los efectos fijos | Delay, comb, crush y reverb como ecuaciones, con su CPU medida |
| 8 | Punch-ins de fórmula (audio y tiempo); secuenciadores de fórmula | Stutter, reverse y tape-stop al pulso; fórmula como secuenciador |
| 9 | Generador de ecuaciones para todos los roles | Genera, muta, cruza y mezcla; el filtro descarta silencio, DC y saturación |
| 10 | Estética demoscene / pixel en todas las pantallas | Suite de render limpia en cada pantalla y paleta |
| 11 | FM-F y GLITCH | Si entran en CPU y flash |

## 9. Decisiones abiertas

- **EXT y SWARM**: ¿se quedan la entrada de audio USB y el instrumento Bluetooth? Ocupan flash (`eng_input.c` ~7 KB,
  más la radio).
- **Editor de fórmulas en el aparato**: con 4 knobs y teclas, ¿por tokens (elegir operador, variable o número) o solo
  el editor web y el generador?
- **El editor web y el instalador**: se adaptan para BYTE (otra identidad), o BYTE usa los de JIANT FM con un modo.
- **Seguir los arreglos de JIANT FM**: cada arreglo de la base se porta a mano (es un repo aparte). Ver de vez en
  cuando los cambios de `codex/*` en JIANT.

## 10. Estado de la base (verificado el 2026-11-15)

- V10.7.37 compila: imagen 583.424 B de 584.336 B; RAM `.data+.bss` 98.120 B de 98.304 B.
- Suite completa (`tests/run_tests.sh`, con el emulador): pasan las 219 renders de referencia bit a bit, la salud del
  audio, las voces, el almacenamiento, el backup, los proyectos, MIDI, USB, el instalador, la interfaz y cada motor.
- Fallan 10 tests en la base V10.7.37, los mismos con la fase 0 (el renombre no agrega ninguno). Varios fallan igual
  en la estable V10.7.23; son tests que quedaron atrás del firmware o del entorno:
  - `text_spacing_test.py`, que no lee el formato actual de las fuentes;
  - el render de la interfaz (también con `FELUCCA_FM4=1`): 51 avisos de diseño ya presentes en V10.7.36;
  - dos de CPU (cpu5): cuatro pistas de una nota al 70..90 %, y fx-rest en la build de referencia `-DFX_REST_OFF`;
  - el costo de los loops de render en el binario del chip (`tests/target_budget.py`);
  - el mapa de navegación de `docs/`, desactualizado;
  - la app Android (JIANT Stems), que no compila sin red a Maven;
  - el emulador: el guardado de preset (la secuencia SAVE, OCT+, OCT+ cambió en V10.2) y T4 en sus stems.
- JIANT BYTE no se juzga contra esta lista: a medida que las fases reemplazan partes, sus tests se reescriben.
