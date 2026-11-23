# BYTE v0.14 — CPU liviana y FORM 2

Dos frentes en esta versión: que BYTE use poca CPU (el FM-1 se reiniciaba al morphear FORM con DENSITY alto, y con
cuatro pistas el audio se rompía) y que FORM pase de 22 estados cerrados a un motor que compone por capas, desde una
progresión armónica hasta un beat electrónico completo.

## CPU

Medido con `tests/cpu_vg.c` (valgrind, instrucciones por muestra). La referencia es la CPU TEST de JIANT FM V10.7.23
(2864); BYTE estaba en 3254.

| Sección | Antes | v0.14 |
|---|---|---|
| CPU TEST (`song/cputest`) | 3254 | **2817** |
| CPU TEST con LIFE 100 | 9104 | 4816 |
| FORM solo (`eng/FORM.A`) | 2186 | **1453** |
| FORM con MORPH entre A y B (`eng/FORM.AB`) | 3757 | **2047** |
| WAVE | 4718 | 3578 |

Qué cambió:
- **LIFE**: el aditivo reescrito (los slews una vez por llamada, los parciales por recurrencia, los armónicos a media
  frecuencia), la captura de cada voz una vez cada cuatro bloques, y nada cuando LIFE está apagado.
- **Media frecuencia** (de JIANT FM V10.7.56): FORM, MATH y WAVE pueden correr a 22 kHz. FORM lo hace siempre; los otros
  cuando el guard se dispara. Con VH_SH cada motor ajusta sus constantes a la tasa (el hold y los polos de MATH, el tick
  de FORM, los slews de LIFE). Las colas soltadas a media frecuencia pasan por un pasabajos y las que bajan de −60 dB se
  cortan. VPOL queda apagado en BYTE: la canción pide más voces de las que la media frecuencia devuelve.
- **FORM**: sin divisiones por variable en su bucle por muestra (recíprocos, desplazamientos, la posición del ciclo
  llevada de muestra en muestra), y con el guard disparado suena un solo estado, el más cercano.
- **La reverb** (de JIANT FM): ROOM, ROOM con MOD / WIDE y la red de CLOUD a 22 kHz; los granos de CLOUD sin división.
- **El guard baja LIFE**: cuando la carga pasa el 85 %, todas las pistas menos la seleccionada pierden LIFE (con un
  deslizamiento, y lo recuperan unos 2 s después de que se calma).
- **La modulación cruzada** (O1..O6) copia la salida de una pista solo si alguna fórmula la lee.
- WAVE con una tabla en caché para las voces de unísono; BALANCE apagado.

## FORM 2

Las referencias de Juan (`docs/FORM-REFERENCIAS.md`) entran como **genes**: cada estado combina una armonía, un
voicing, una melodía, un ritmo, un timbre, una forma y una mezcla. Los 22 estados de antes quedan como estaban.

**14 estados nuevos**: STRUM 332, HOUSE M9, CHIRP, DRIFT, BITPACK, BUILD, RISER, FILLS, SUITE, DEGREES, NIBBLE, GROWL,
WOBBLE y ACID.

**Armonía por grados**: HARMONY elige entre la del estado (AUTO), CLASSIC, DEGREES, FAMILY, PEDAL y BITS. Fuera de
CLASSIC los acordes se apilan con los grados de la escala de la canción (SCL) y la melodía sigue al acorde. PROGRESS
elige la familia de progresiones (AXIS i–♭VI–♭III–♭VII, II-V-I, VAMP, DESCEND, FOURTHS, EPIC, POP, CIRCLE) o la deja a
SEED. EVOLVE hace sustituciones entre secciones.

**Voicings**: CLOSE, LEAD, OPEN, +BASS, DROP, QUARTAL, NEAR (cada acorde cerca del anterior) y WIDE.

**Bajo de carácter** (STYLE): WOBBLE, REESE, GROWL (formantes por sincronismo), RING, SCREECH y ACID (filtro resonante
con acento, slide y squelch). DRIVE satura la mezcla de FORM; TONE da el carácter (formantes, resonancia, desafinación);
WOBBLE la velocidad del wobble (de 1/2 a 1/16, STEPS, JUMPS, BAR).

**Ritmo y forma**: ratchets, fills al final de la vuelta, riser antes del cambio, entradas de a una (BUILD) y sidechain
del bombo.

**El MORPH compone**: entre A y B, cada paso toma la melodía de uno u otro y cada sección la progresión, según la
probabilidad que marca la perilla. En 0 y 127 suena un solo estado, como siempre.

**El dado**: SELECT tira todo FORM de forma coherente (estados, timbre, armonía, reglas, reloj); SEQ + SELECT solo la
composición. CANCEL deshace.

**Páginas, por importancia**:

| Página | Controles |
|---|---|
| STATES | STATE A, STATE B, MORPH, COLOR |
| RULES | GROOVE, DENSITY, SEED, EVOLVE |
| TIMBRE | STYLE, DRIVE, TONE, WOBBLE |
| HARMONY | HARMONY, PROGRESS, CHORD, VOICING |
| LAYERS | DRUMS, DETUNE, OCTAVE, LEVEL |
| CLOCK | RUN, DIV, CYCLE, SWING |

La tónica y la escala son las de SCL, las de toda la canción. La página ARP no aparece en FORM: sus lugares son los
genes. Todos los valores de FORM son destinos de la matriz de modulación.

## Verificación

La suite BYTE (`tests/run_byte_tests.sh`) pasa completa. FORM: los 36 estados suenan sin recortar, cada control cambia
el sonido (`tests/form_audio_test.py`), el MORPH intermedio no es un crossfade, los presets de usuario guardan los genes,
las 24 perillas de sus páginas cambian la pantalla, y 40 tiradas del dado quedan en rango y suenan. La reverb
(`tests/reverb_test.c`) entra en la suite. La CPU se mide en la computadora: no equivale a la del FM-1, que hay que
probar en el aparato.

## Pendiente

FORM BEATS (kits por estilo, una 4ª línea de percusión, articulación de hats, estados por género), la armonía compartida
con el generador (FORM como rol del generador, sin duplicados, FORM → STEPS), y el editor web de BYTE.
