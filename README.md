# woody-woodpacker
Create a simple virus that infects and encrypts ELF64 binaries using C

El [contrato inicial de integración](docs/contrato_integracion.md) recoge las
decisiones y restricciones previstas para conectar cifrado, stub e inserter;
la integración completa todavía está pendiente.

## Estado de entrada del stub

El stub preserva todos los registros generales x86-64, incluido `rdx` (que el
cargador dinámico puede usar para entregar la función de finalización), y
restaura `rsp` y el contenido original de la pila antes de saltar al entry
original. Esto conserva `argc`, `argv`, el entorno, el vector auxiliar y la
alineación con los que fue invocado. Utiliza espacio por debajo de `rsp`
para guardar registros y la dirección de salto; no preserva su contenido previo.

No preserva los flags ni el estado de registros SIMD/FPU. Además, la prueba es
aislada: no demuestra todavía la integración con el empaquetador. Los errores
de `write` se ignoran y no se restauran permisos originales distintos de RX.
Como los cambios en el stub modifican sus offsets,
la integración debe obtener de nuevo los offsets de los datos parcheables.

Para comprobar el salto, los registros y la pila tras el descifrado, ejecutar
`make test-stub-entry` (requiere Linux x86-64, Python 3,
NASM y `ld`). La prueba compara las entradas desde `argc` hasta el terminador
del vector auxiliar, con argumentos y variables de entorno de ejemplo; no
compara los bytes de las cadenas u otros datos apuntados por esas entradas.
La ruta de descifrado comprueba que el bloque cambia, no que el resultado BTEA
sea correcto. Para probar una versión anterior del stub se puede ejecutar
`python3 tests/test_stub_entry.py --stub /ruta/al/stub.asm`.

## Stub y pruebas de descifrado

`make` genera `src/stub/stub.bin`. El stub necesita que el futuro inserter
parchee la clave, la longitud y los desplazamientos antes de ejecutarlo.

Para comprobarlo de forma aislada, desde la raíz:

```bash
make test-stub
```

Dependencias: Linux x86-64 con páginas de 4096 bytes, Python 3, NASM,
compilador C, GNU binutils (`nm`, `objcopy` y `readelf`) y `make`. El entorno
debe permitir `mprotect` con permisos de escritura y ejecución. Las pruebas
compilan en un directorio temporal y no necesitan el inserter.

Se cifra con `btea_encrypt` del proyecto y se verifica el contenido completo
descifrado, el mensaje exacto y la ejecución del destino parcheado. Se prueban
2, 3, 16, 1024, 2049 y 32767 palabras, con datos y destino tanto antes como
después del stub. Los datos empiezan cerca de un límite de página; los tamaños
mayores cubren múltiples páginas. El máximo probado corresponde al límite
positivo del parámetro `short` de la implementación C. Esta prueba aislada no
valida la integración ELF ni la preservación del estado inicial del proceso.

El contrato provisional acepta longitudes cifradas múltiplos de cuatro entre
8 y 131068 bytes inclusive. El stub rechaza otras longitudes y los desbordamientos
del rango redondeado a páginas. Si falla cualquiera de los dos `mprotect`,
termina el proceso con código 1 sin saltar al entry; si falla el primero,
tampoco descifra. Se ignoran los errores de `write`: el mensaje es informativo.
Las pruebas comprueban los permisos en `/proc/self/maps`, los desbordamientos y
simulan errores de `mprotect` y `write` con seccomp. También comprueban el
contenido antes y después del descifrado. Requieren que seccomp esté permitido
y que `/proc/self/maps` sea accesible.

El stub presupone páginas de 4096 bytes y restaura siempre RX, no consulta los
permisos originales. El futuro inserter debe validar que todas las páginas
afectadas pueden restaurarse íntegramente a RX, además de verificar rangos,
direcciones y solapamientos; de lo contrario debe rechazar el ELF. Este diseño
se basa en el contrato propuesto en la PR #9 y está pendiente de integración.

## Knowledge required
### ELF64 binaries structure

### Encryption algorithms

### Execute arbitrary assembly instructions stored as plain text during runtime
