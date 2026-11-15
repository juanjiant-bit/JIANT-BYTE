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
| `lp(x,k)` `hp(x,k)` | filtros de un polo con estado, dentro de la fórmula |
| `hold(x,k)` | retención (sample and hold, reducción de frecuencia) |

Con memoria y estado, un efecto es una fórmula como cualquier otra: `I + z(T*4410&16383)*0.6` es un eco,
`sin(I*3+O2)` es una distorsión cruzada con la pista 2. Las operaciones con estado funcionan bien en la ejecución por
bloques, porque cada instrucción recorre sus 32 muestras en orden. Lo único que no se puede es que una muestra se
realimente en sí misma dentro de la misma fórmula: la realimentación pasa por `z(k)` o por un bloque de retraso.

La reverb larga es el caso más caro: escrita como fórmula puede no entrar en CPU. Si no entra, queda como primitiva
(`rev(x,k)`) que las fórmulas llaman, y su sonido lo sigue gobernando la fórmula.

**Lo que hay que medir en la fase 1**: instrucciones por muestra y por voz frente a las fórmulas fijas actuales
(`lofi_byte`, `lofi_float`), en el host con valgrind y en el binario del chip (`tests/target_budget.py`).

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
