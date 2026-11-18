# BYTE v0.13

FORM pasa al primer lugar del catálogo y a la pista 1 de los proyectos nuevos. Sus cinco páginas muestran ecuaciones y morph, eventos de percusión, estructura temporal, motivos/acordes y capas. Los sonidos y la selección de los proyectos guardados se conservan.

PRESETS recorre todos los sonidos visibles de FORM, DRUM-X, WAVE y MATH en una lista continua. EDIT + PRESETS salta al primer sonido del motor siguiente o anterior. Los IDs de motores y la numeración MIDI no cambian.

Los punch-in toman números, perfiles armónicos y fase de otras voces superpuestas. Se mantienen pulsados, funcionan con LIFE en cero y no reescriben el proyecto. Los diez códigos grabados conservan sus posiciones; ENSEMBLE en G5 es adicional para interpretación en vivo.

- [FORM: controles, pantallas y navegación](form-engine.md)
- [Punch-in: teclas y comportamiento](punch-formulas.md)

Validación: checks BYTE de fórmulas, FUNC, LIFE, generación/FUGA, DRUM-X, voces, MY LOOP, controles, navegación y persistencia aprobados. La prueba anterior que esperaba volver al inicio del mismo motor fue actualizada para comprobar los límites de la lista continua. Los veinte controles alteran su panel y los títulos de los 22 estados caben en el área de dibujo. Los 22 presets reproducidos desde el arranque FORM coinciden en 1.940.400 frames entre C y WASM. Firmware AC79 compilado con todas las guardas activas: imagen 567.260/584.336 bytes, pool libre 9.248 bytes, RAM code 32.608/34.816 bytes. Rendimiento y escucha en hardware físico pendientes.
