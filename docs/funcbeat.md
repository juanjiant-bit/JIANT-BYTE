# FUNC — composición por ecuaciones (BYTE v0.11)

Seleccionar **MATH → FUNC SONG** o **FUNC BEAT** con PRESET y pulsar **PLAY**. Una función produce la pieza completa; STOP libera su voz. El patrón del secuenciador no reinicia la función en cada paso. El tempo controla el reloj musical y ROOT transpone la composición.

En las páginas del motor:
- **RUN: PLAY** (por defecto) arranca la fórmula con PLAY, en una voz por pista, al reloj del transporte. Las teclas y los pasos del secuenciador (hasta 64, con su RATE, grabables) le cambian la altura; cada nota la vuelve a golpear sin cortar la fórmula. Parado, una tecla la hace sonar como en TRIG.
- **RUN: TRIG** cada nota del teclado o del secuenciador hace sonar la fórmula a su altura y con su gate. Con PLAY corriendo, el reloj es el del transporte (la fórmula sigue donde va la canción); parado, arranca desde la nota.
- **RUN: PLAY** sigue PLAY/STOP y usa una voz de transporte por pista (era SONG). El teclado no duplica la pieza; usar ROOT para transponerla.
- **RUN: FREE** corre siempre, sin teclas ni transporte, con su propio reloj, en la tónica (ROOT).
- **Motor FORM (BYTE 0.17)**: vuelve FORM 2 tal cual sonaba (los 22 estados de FORM 1 y los 14 de FORM 2, su arreglo y su MORPH compositivo; renders idénticos a los de entonces). Páginas:
  - **FORM**: RUN (TRIG / PLAY / FREE), ROOT, STATE A, STATE B.
  - **SOUND**: MORPH, COLOR, STYLE, DRUMS.
  - **RULES**: GROOVE (incluye EUCLID, BREAKS, POLY...), DENSITY, SEED, EVOLVE.
  - **NOTES**: MODE (SEQ: la melodía del estado; ARP: los tonos del acorde arpegiados como en STRUM 332, el orden por SEED; EUCLID: la melodía solo en euclideanos fundidos por SEED, con acentos), OCTAVE, CHORD, VOICING.
  - **HARMONY**: HARMONY, PROGRESS, SCALE. **TIMBRE**: DRIVE, WOBBLE, TONE, DETUNE. **CLOCK**: RATE, CYCLE, SWING (el LEN y el RATE del secuenciador quedan libres: 64 pasos).
- **FORM: GROWL / HALFTIME / ACID / REESE / HOUSE / STRUM / JUST / PLUCK / NIBBLE / BITS** (las diez primeras recetas de FORM 4, de las referencias de Juan; en todas D es la semilla y el patrón de acción es eu()). Las dos primeras: GROWL, dos formantes por sincronismo con wobble trapezoidal (A la vocal, B el wobble de 1/4 a 1/16, C el drive, D la semilla); HALFTIME, wobble de velocidad cambiante con ring y el sub dentro de la saturación, bombo y caja a medio tiempo (A la profundidad, B el ring, C el drive, D la semilla). Su patrón de acción es `eu(paso, semilla)`: cada 4 compases dos ritmos euclideanos elegidos al azar de una tabla y combinados (O, Y, XOR o uno por compás); las notas, de una tabla fundida por la semilla y la sección.
- **FORM: SONG / BEAT / CUSTOM** permite explorar las dos composiciones y regresar al código propio. Cambiar FORM o preset no borra el texto CUSTOM.
- **OCT+** abre el editor; OCT+ aplica, OCT− cancela. Editar una plantilla y aplicarla la convierte en CUSTOM. El emulador tiene además editor de texto y selector SONG/NOTE debajo del instrumento.
- **Guardar el proyecto** conserva código, controles y secciones. El formato corto de sonidos USER no contiene el programa y muestra un aviso para guardar el proyecto.

## SONG: armonía, melodía y secciones

Este es el programa real del preset, editable y dentro del límite de 255 caracteres:

```c
func{let bt=P%64;let r=bt*(1+int(D*3));return sat(chord(n-12+"0573"[int(bt/8)]-48,bt%8,A)*.5+note(n+"0247"[int(r)]-48,r%1,A)*.5+(bt>8&&bt<48)*(kick(bt%1)*.75+snare((bt+1)%(2-B))*.5+hat(bt*(2+int(C*3)*2)%1)*.75));}
```

El ciclo tiene 64 beats. Una tabla cambia la armonía cada ocho beats; otra da las notas de la melodía. La batería entra después del beat 8 y sale en el 48. Todo esto está escrito en la expresión: no hay una canción fija escondida en los helpers de C. A 120 BPM, el ciclo dura 32 segundos.

| Control | FUNC SONG |
| --- | --- |
| A | Voicing menor/mayor y registro de los acordes; color de la melodía |
| B | Cadencia de la caja: de una vez cada dos beats a una vez por beat, con ritmos intermedios |
| C | Hats: 2, 4, 6 u 8 subdivisiones por beat |
| D | Melodía: 1, 2, 3 o 4 pasos por beat |

## BEAT: patrones derivados de operaciones

```c
func{let bt=P%32;let st=int(bt*4);return sat(note(n-24+(st^(st>>2))%5*3,bt*4%1,A)*.8+(bt<24)*(kick(bt%(1.5+D*.5))*.8+snare((bt+1)%(2-B))*.4+hat(bt*(2+int(C*3)*2)%1)*.6));}
```

Aquí XOR, desplazamientos y restos producen las notas, sin una tabla de melodía. La percusión deja espacio en el último cuarto del ciclo. A modifica los armónicos del bajo; B, la caja; C, los hats; D mueve el periodo del bombo entre 1,5 y 2 beats. Los cuatro controles también son destinos de la matriz de modulación.

## Helpers y relojes

Cada helper devuelve audio; el programa decide cuándo sumarlo, repetirlo o silenciarlo.

| Expresión | Resultado |
| --- | --- |
| `note(midi, edad, color)` | Nota con envolvente de un beat; color 0..1 añade armónicos |
| `chord(raiz, edad, color)` | Cuatro notas, envolvente de cuatro beats; color selecciona voicing/registro |
| `kick(edad)` | Bombo con caída de tono y envolvente de medio beat |
| `snare(edad)` | Caja tonal + ruido determinista, duración 3/8 de beat |
| `hat(edad)` | Percusión de ruido, duración 1/8 de beat |
| `P` | Beats por muestra: transporte en SONG, en NOTE el transporte si corre, si no el tiempo desde la nota |
| `p` | Reloj de transporte por bloque, conservado para fórmulas anteriores |
| `t`, `T` | Muestras enteras y segundos desde que arrancó esta voz/programa |
| `w` | Fase de la nota, en vueltas |
| `n`, `v`, `freq`, `sr` | Nota MIDI, velocidad, frecuencia y sample rate interno |
| `A B C D` / `a b c d` | Controles 0..1 / enteros 0..254 |

La edad se expresa en beats. `kick(P%1)` repite un bombo cada beat; `note(60,P*4%1,A)` repite una nota cuatro veces por beat. Edad negativa o mayor que la duración devuelve silencio. Los osciladores de notas/acordes usan muestras enteras para mantener la afinación al cambiar el tempo. `P` vuelve a cero tras 32768 beats; los ciclos de los ejemplos encajan en ese recorrido.

Los helpers no consumen palabras de estado. Su presupuesto es de **ocho componentes simultáneos por programa**: chord cuenta cuatro y cada otro helper, uno. El motor incluye ese trabajo en la estimación de carga, además de las mediciones de audio del dispositivo.

## Funciones propias y memoria

El lenguaje mantiene las expresiones de BYTE, tablas de caracteres, comparaciones, ternarias, operaciones de bits y llamadas como sin/tri/sqr/saw, ph, lp, hp y rnd. `sin()` usa vueltas. Los nombres de builtins y controles están reservados; por ejemplo, usar `bt` en lugar de declarar `b`.

```c
func{state q;fn tone(z)=sin(z)+q*.4*sin(z*2);q=q+(A-q)*.002;return tone(w);}
```

`let` crea una variable local por muestra. `state` comienza en cero, continúa entre muestras y se reinicia en una nota nueva o al aplicar código. Los helpers `fn` tienen un argumento, lo evalúan una sola vez y pueden llamar a helpers anteriores. `ph(rate)` mantiene un reloj de fase de alta precisión; es preferible a sumar incrementos diminutos a un estado Q16.16.

También se pueden superponer varias llamadas a una función propia, dentro del presupuesto de instrucciones:

```c
func{fn partial(z)=sin(w*z)*.1;return partial(1)+partial(3)+partial(5)+partial(7)+partial(9);}
```

## Alcance

Esta es una versión nativa y acotada para componer dentro de BYTE, ejecutada por la misma VM en hardware y WASM. Usa Q16.16, no un intérprete JavaScript. Las referencias JavaScript/Bytebeat sirven para traducir ideas; no se importan sus arrays, closures, buffers de delay ni programas completos sin adaptación. FUNC devuelve una señal mono que pasa por PAN y los efectos/buses existentes del instrumento.

Límites: 255 caracteres ASCII por pista, 32 bytes de tablas, seis locales, dos palabras de estado (compartidas entre state/ph/lp/hp), dos definiciones de helpers, cuatro niveles de llamadas, 64 instrucciones y profundidad acotada. Sin bucles libres, recursión ni asignación dinámica. Compilación fuera del callback; un error conserva el programa anterior. LIFE analiza copias del estado y no adelanta la voz real.

[Ejemplos de audio v0.11](https://juanjiant-bit.github.io/JIANT-BYTE/audio/v011/)
