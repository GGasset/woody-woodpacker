# woody-woodpacker
Create a simple virus that infects and encrypts ELF64 binaries using C

## Stub y pruebas

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
