# Cómo trabaja la llave por dentro

**Estado:** Todo lo de aquí es un borrador y está comprobado en el ordenador. Nada funcionó en una llave real.

## Dos chips, dos tareas

- **El chip de radio (nRF52840)** habla con el ordenador y con el teléfono.
- **El chip de seguridad (SE051)** guarda los secretos. No entrega ninguna clave.

¿Por qué dos? La protección del chip de radio ya fue burlada en el pasado con picos de tensión. Por eso un chip de radio roto no revela nada por sí solo.

Además hay:

- **Memoria (64 MB):** la caja fuerte, cifrada, y el registro.
- **Pantalla:** muestra lo que autorizas.
- **Un botón:** esa es tu autorización.
- **Sin batería:** la llave funciona solo con corriente USB. El ordenador pone en hora el reloj de los códigos únicos cuando la enchufas.

## Vías hacia el ordenador

- USB como dispositivo de inicio de sesión (FIDO2)
- USB como lector de tarjetas inteligentes (PIV, OpenPGP)
- USB como teclado (**previsto, todavía no construido**)
- Canal USB para el programa de administración
- Unidad USB, solo de lectura, con los programas dentro (**previsto**)
- NFC (solo mientras el USB alimenta la llave)
- Bluetooth, solo para el programa de administración (solo mientras el USB alimenta la llave)

## ¿Por qué sin batería?

La llave no tiene batería, a propósito. Funciona solo mientras está enchufada a un puerto USB.

- El reloj de los códigos únicos basados en el tiempo no necesita alimentación continua. Lo pone en hora el ordenador al enchufar la llave, y el código nace solo en el momento de usarlo. **Borrador, todavía no construido.**
- Para la marca de tiempo de una firma, la llave pide la hora a un servidor de marcas de tiempo. No usa su propio reloj.
- Una batería solo habría dado NFC y Bluetooth sin ordenador. En contra había 4 a 5 mm más de longitud y una pieza de desgaste con pocos años de vida, en una llave que debe durar décadas.

**A qué renuncias:**

- El chip de radio no puede sacar corriente del campo NFC. Por eso NFC y Bluetooth existen solo mientras la llave está enchufada a un ordenador.
- Acercar la llave suelta al teléfono ya no funciona.
- Bluetooth hacia tu propia aplicación solo funciona mientras la llave está enchufada a un ordenador. Con la llave en el llavero ya no funciona.
- Una segunda llave se empareja solo por USB en el ordenador. Para eso ambas llaves están enchufadas al mismo ordenador.

Es una decisión tomada a propósito. La técnica está en `docs/architecture.md`.

## Los dos caminos de la configuración

Esta es la técnica detrás de la pregunta única de [installation.md](installation.md).

### "Solo esta llave"

- Para cada acceso nace en el chip de seguridad **un par de claves propio.** La parte privada no existe **en ningún otro sitio.**
- No hay nada que robar y nada que desempaquetar.
- El precio: cada acceso ocupa espacio en el chip. El número de servicios es **finito.** Cuántos son depende de la versión de chip suministrada (46 o 104 kB) y **no está medido.** El programa debe avisar a tiempo.
- No hay un segundo dispositivo para estos accesos.

### "Repuesto posible" (replicado)

- Un secreto principal se crea al emparejar **fuera de los dos chips** y se escribe en ambos.
- El precio: queda un instante en la memoria del chip de radio.
- Después de escribirlo, el firmware **solo puede usarlo**, no leerlo, no sobrescribirlo, no sacarlo. Eso no es disciplina, sino que lo establece así el chip.

### ¿Por qué no se puede cambiar?

Un secreto que ha nacido en el chip no lo abandona nunca, ni siquiera cifrado. Por tanto no se puede replicar después. Un secreto principal replicado tiene que nacer **antes** de existir, en el lugar adecuado.

Ninguno de los caminos es en general más seguro. Perder la única llave destruye más de lo que suele causar una llave robada.

## El PIN

- La llave nunca recibe el PIN en texto claro, solo un valor de comprobación derivado de él.
- El **chip de seguridad** lo comprueba y cuenta los intentos fallidos. Borrar la memoria no pone el contador a cero.
- Tres PIN incorrectos seguidos bloquean hasta que desconectas la llave.
- PIV y OpenPGP usan **el mismo** PIN. Los PIN de administración (PUK, PIN de administrador) están en la caja fuerte, cada uno con su propio contador.
- Cada intento con un PIN de administración pregunta en la pantalla. Así es correcto.

## La caja fuerte

- Cada secreto está cifrado por separado (AES-256-GCM).
- **Corte de corriente:** Cada cambio se escribe en un orden fijo. Después de cualquier corte está el estado antiguo **o** el nuevo, y todo es legible. Esto se probó en cada punto de escritura.
- La llave reparte la escritura por toda la memoria. Así no se desgasta en un solo punto.
- **Buscar sin abrir:** Algunas entradas llevan una marca corta. Dice a qué servicio pertenece la entrada. Así la llave encuentra rápido una passkey. La marca está a su vez cifrada.
- La búsqueda tiene que recorrer todas las partes de una lista. **Cuánto tarda eso en el chip real no está medido.**
- Una actualización del firmware no pierde datos.

## La copia de seguridad

- Un archivo cifrado a partir del cual se puede configurar una llave nueva.
- Su clave sale de la **frase secreta principal**, no del chip. Si no, la copia dependería de la llave que se perdió.
- En la llave los valores de cálculo se mantienen pequeños. Por eso la fortaleza tiene que venir de la frase secreta principal. Por eso ocho palabras.
- Los sobres dividen la clave del archivo (Shamir).

## Una segunda llave

- Las dos llaves generan una clave de vida corta e intercambian solo la parte pública.
- De ahí nace el número de seis cifras. Quien se coloca en medio genera **dos números distintos.** Esto se probó.
- Sin el número confirmado no sale ni un byte de la llave.

## Qué protege a la llave de quién

| Quién | Qué obtiene |
| --- | --- |
| encuentra la llave | nada sin el PIN |
| abre la llave y examina la placa | La línea entre los chips está cifrada **en cuanto se han sustituido las claves de fábrica** (mira abajo). La memoria está cifrada. |
| tiene un programa dañino en el ordenador | Cada autorización necesita el botón, y la pantalla indica el servicio. El programa no puede pulsar. |
| gestiona una página web imitada | Las passkeys están ligadas a la dirección auténtica. La extensión solo rellena en la dirección guardada. |
| desvía NFC entre el teléfono y la llave | sigue necesitando la pulsación en la llave |
| copia un servidor | La clave del servidor se queda en el chip. **Solo si el chip suministrado puede hacerlo.** |

## Lo que la llave no puede hacer

Aquí se dice sin embellecer.

### No protege de

- **Alguien que te obliga a pulsar el botón.** Ningún dispositivo puede detectarlo.
- **Un firmware modificado.** El chip de seguridad protege de ataques al chip, no de un firmware que te miente. Contra eso solo ayuda el arranque firmado. Tiene que estar ahí **antes** de activar la protección de lectura.
- **Que alguien lea tu PIN.** Lo escribes en el ordenador, así que un programa dañino puede leerlo. No puede usarlo, porque cada autorización necesita el botón.
- **La pérdida de todo.** Mira abajo.

### Huella dactilar

- La primera fase de desarrollo **no tiene sensor de huella dactilar.** Autorizas con PIN y botón.
- El sitio para un sensor está previsto en la placa.
- En FIDO2 una huella dactilar **sustituye** al PIN. No es una protección adicional.
- **Un reconocimiento de que el dedo está vivo no está demostrado en ningún sensor examinado.** Quien teme un dedo moldeado no gana nada con ninguno de estos sensores.
- Los datos de huella dactilar nunca estarían en la caja fuerte y nunca podrían pasar a una segunda llave.
- Ningún navegador ni página web puede registrar un dedo. Eso ocurriría en la propia llave.

### Pérdida

**Quien pierde la frase secreta principal y todas las llaves, lo pierde todo.**

Si pierdes a la vez la llave, la segunda llave, el archivo de copia de seguridad y los sobres, el contenido se ha ido. Nadie puede ayudar. No hay ninguna puerta trasera que se pueda robar.

### Otras cosas que conviene saber

- **Ninguna prueba externa.** Nadie fuera del proyecto ha examinado el cifrado.
- **No se ha ejecutado ninguna prueba FIDO.**
- **El chip viene con claves de fábrica conocidas públicamente.** En la primera puesta en marcha **hay que** sustituirlas. Hasta entonces la línea entre los chips no está protegida. Está descrito y previsto, pero **nunca ejecutado en un chip.**
- **Quien puede reescribir la memoria directamente** no puede leer nada. Pero puede reinstalar un estado más antiguo. Contra eso falta todavía un contador en el chip.
- El **contador de firmas** del inicio de sesión empieza en cero cada vez que se enciende. Algunas páginas podrían tomarlo por una clave copiada.
- La llave envía una **autocertificación**, no un certificado del fabricante. Eso es bueno para la privacidad, pero algunas páginas de empresas exigen más.
- Si el chip suministrado permite el **acuerdo de claves** (ECDH) depende de su configuración de fábrica. Si no lo permite, **desaparece el modo servidor con la clave en el chip.** Firmar sigue funcionando. El programa lo dice entonces con honestidad, en lugar de parecer un error de PIN.
- **Mac:** nunca probado.
- **El espacio de memoria en el firmware** es escaso. Si caben todas las funciones, está abierto.

## Programa y llave hablan entre sí

- El programa `osk` habla con la llave a través de un canal USB propio.
- Cada respuesta de la llave se trata como hostil: formatos estrictos, límites duros de tamaño y profundidad, y el número de secuencia tiene que coincidir.
- La interfaz local solo escucha en tu ordenador. Cada solicitud tiene que venir de la propia dirección. Una página ajena en el mismo navegador no llega hasta ahí.
- Cada solicitud indica el nombre del programa. La llave lo escribe en el registro. Sigue siendo un nombre, no una prueba.
