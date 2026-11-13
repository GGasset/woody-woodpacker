# woody-woodpacker
Create a simple virus that infects and encrypts ELF64 binaries using C

## Estado de entrada del stub

El stub preserva todos los registros generales x86-64, incluido `rdx` (que el
cargador dinámico puede usar para entregar la función de finalización), y
restaura `rsp` y el contenido original de la pila antes de saltar al entry
original. Esto conserva `argc`, `argv`, el entorno, el vector auxiliar y la
alineación con los que fue invocado. Utiliza espacio por debajo de `rsp`
para guardar registros y la dirección de salto; no preserva su contenido previo.

No preserva los flags ni el estado de registros SIMD/FPU. Además, la prueba es
aislada: no demuestra todavía la integración con el empaquetador. Los errores
de `write` y `mprotect`, y la restauración de permisos originales distintos de
R-X, no están gestionados. Como los cambios en el stub modifican sus offsets,
la integración debe obtener de nuevo los offsets de los datos parcheables.

Para comprobar el salto, los registros y la pila en las rutas con y sin
descifrado, ejecutar `make test-stub-entry` (requiere Linux x86-64, Python 3,
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

## Knowledge required
### ELF64 binaries structure

### Encryption algorithms

### Execute arbitrary assembly instructions stored as plain text during runtime
