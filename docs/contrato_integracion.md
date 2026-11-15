# Contrato de integración: cifrado, stub e inserter

Este documento fija una primera versión del contrato de la issue #4. Es una
base de trabajo para #2 y #6, no una descripción de una integración ya
implementada ni una garantía de compatibilidad con cualquier ELF. Las
decisiones pueden revisarse si la integración demuestra que son insuficientes.

## Alcance y selección de la región

- El objetivo inicial son ejecutables Linux x86-64 ELF64 `ET_EXEC` y PIE
  `ET_DYN`, incluidos los enlazados dinámicamente, siempre que superen las
  validaciones de integración. Los demás archivos se rechazan explícitamente.
- El empaquetador selecciona una única región contigua de código respaldada
  por bytes del archivo y mapeada en memoria ejecutable. Como punto de partida,
  puede usar `.text` si existe y puede verificarse su correspondencia con un
  `PT_LOAD` adecuado; no se presupone que las secciones estén presentes.
- La región elegida debe permanecer sin utilizar ni modificar desde la carga
  hasta que el stub tome el control. En particular hay que considerar el
  cargador dinámico, relocaciones y código ejecutado antes del entry. Si no se
  puede garantizar para una entrada, se rechaza: cifrar a ciegas `.text` o todo
  un segmento ejecutable no es suficiente para garantizarlo.
- Se cifra un prefijo de longitud múltiplo de cuatro; los últimos 0–3 bytes,
  si existen, quedan sin cifrar. No se insertan bytes de padding en el código
  existente ni se sobreescriben bytes vecinos. El prefijo debe tener entre 8 y
  131068 bytes, ambos inclusive; si no cumple estos límites se rechaza por
  ahora. No se trunca silenciosamente una región grande ni se divide en bloques:
  cualquiera de esas opciones requeriría ampliar el contrato.
- El stub y sus datos no pueden estar dentro de la región cifrada. El entry
  original debe seguir siendo alcanzable después del descifrado. El empaquetador
  comprueba los rangos de archivo y memoria, los desbordamientos y la ausencia
  de solapamientos peligrosos antes de escribir la salida.

## Cifrado y datos parcheados

Se usa el BTEA implementado en `src/encryption/btea.c` y su descifrado en el
stub, sobre palabras `uint32_t` little-endian. La clave contiene 16 bytes,
interpretados como cuatro palabras little-endian en el mismo orden por ambos
lados. Se genera una clave nueva con una fuente aleatoria del sistema; si no se
obtienen los 16 bytes, se aborta. El empaquetador la muestra según el subject.
La función actual `fill_randomly` todavía no comunica errores ni lecturas
incompletas: no basta con llamarla para cumplir este requisito.

`btea_encrypt` recibe `n` en **palabras**, no bytes. El contrato inicial usa
`2 <= n <= 32767`, por el `short` de la implementación C actual en la
plataforma objetivo; el empaquetador valida antes de convertir y llamar. La
longitud parcheada en el stub es `n * 4` **en bytes**. El límite se aplica al
contrato completo aunque el stub pueda admitir otros valores. No se cifra con
una longitud y se descifra con otra.

Los campos del blob se escriben en little-endian:

| Campo ASM | Tamaño | Valor |
| --- | ---: | --- |
| `stub_key` | 16 bytes | Clave BTEA. |
| `enc_delta` | 8 bytes | `encrypted_vaddr - stub_vaddr`, delta con signo. |
| `enc_len` | 8 bytes | Longitud cifrada en bytes. |
| `entry_delta` | 8 bytes | `original_entry_vaddr - stub_vaddr`, delta con signo. |

`stub_vaddr` es la dirección virtual ELF del primer byte del blob, es decir,
`stub_base`/`_start`, y `original_entry_vaddr` es el `e_entry` original. Los
deltas se calculan con aritmética comprobada, deben caber en 64 bits con signo
y se codifican en complemento a dos. No son offsets dentro del archivo. En PIE,
stub, región y entry deben pertenecer al mismo objeto cargado: la base de carga
común se cancela al sumar los deltas en ejecución. Hay que comprobar también
que las direcciones y rangos resultantes sean válidos para el ELF elegido.

## Offsets del blob

No se deben copiar a mano offsets numéricos: cambian al editar el ASM. El
proceso de construcción de la integración debe obtener `stub_key`,
`enc_delta`, `enc_len` y `entry_delta` del **mismo objeto ensamblado** del que
se extrae `.text`, restando el símbolo `_start`, como hacen las pruebas
aisladas. Debe fallar si falta algún símbolo, hay relocaciones incompatibles
con el blob, un campo se sale del blob o los campos se solapan. Los metadatos y
el blob deben regenerarse juntos al cambiar el stub; no se reutilizan offsets
de otra compilación. El mecanismo concreto de generación corresponde a #6.

## Páginas, permisos y errores

El stub actual asume páginas de 4096 bytes, cambia temporalmente todas las
páginas que cubren la región a RWX y después las deja RX. Para esta primera
integración se aceptan únicamente rangos cuyas páginas puedan restaurarse
íntegramente a RX sin cambiar los permisos requeridos por otros datos o
mapeos; se rechazan los casos que compartan páginas con datos que deban ser
escribibles o tengan otros permisos incompatibles. El empaquetador debe
verificar también el rango redondeado a páginas y sus desbordamientos. Soportar
otros tamaños de página o permisos originales requiere ampliar el diseño.

Como política objetivo, si falla cualquiera de los dos `mprotect`, el stub
termina con error y no salta al entry original; tampoco descifra si falla el
primero. El comportamiento ante fallo de `write` debe decidirse y probarse en
#2. **Esto aún no está implementado:** el stub actual ignora los resultados
de las syscalls. #2 debe implementar y probar esta política antes de considerar
seguro el flujo integrado.

## Responsabilidades y validación

- **Empaquetador/inserter:** valida ELF, región, límites, páginas y direcciones;
  genera y muestra la clave; cifra el prefijo; inserta el blob real en un
  segmento cargable ejecutable; parchea los cuatro campos; conserva el entry
  original y establece el nuevo. Debe rechazar entradas que no pueda manejar y
  no modificar el archivo original.
- **Stub:** imprime el mensaje, cambia permisos, descifra con los parámetros
  recibidos, restaura permisos y salta al entry original, preservando el estado
  de entrada documentado en el README. Recibe parámetros ya validados; las
  comprobaciones de errores en ejecución son responsabilidad de #2.
- **Pruebas:** las pruebas aisladas existentes cubren el algoritmo, los deltas
  de ambos signos y el estado de entrada, pero no la carga ELF. #6 debe probar
  ejecutables PIE y no PIE, dinámicos, argumentos, entorno, salidas y código de
  retorno, además de entradas rechazadas y casos que afecten páginas. #5
  verifica finalmente los requisitos del subject de extremo a extremo.

Las restricciones de selección, permisos y límites son deliberadamente
conservadoras. Si dejan fuera un caso requerido por el subject, habrá que
ampliar el diseño y sus pruebas; rechazarlos no demuestra por sí solo que el
mandatory esté completo.
