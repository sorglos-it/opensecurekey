# Cuando algo no funciona

**Estado:** Nada de esto se ha vivido en una llave real. Los mensajes proceden del programa. Los consejos generales están marcados como tales.

## La llave se ha perdido

Esta es la sección que importa. Léela **ahora**, no cuando ya haya pasado.

### Primero: ¿qué puede hacer quien encuentre la llave?

- **Nada, sin el PIN.**
- Los intentos fallidos los cuenta el chip de seguridad. Borrar no pone el contador a cero.
- Incluso con el PIN, cada autorización necesita el botón de la llave.
- **No hay bloqueo remoto.** De eso no dice nada la documentación técnica.
- Consejo general, no de la llave: si tienes dudas, cambia las contraseñas de las cuentas más importantes.

### Después: ¿qué tienes? Busca tu caso.

#### Caso 1: Tienes una segunda llave (camino "repuesto posible")

1. Coge la segunda llave.
2. Puede hacer lo mismo que la perdida, **para todo lo que estaba marcado como "se puede replicar".**
3. Tu frase de cartera no estaba entre eso. Nunca pasa a una segunda llave.
4. Crea lo antes posible una **nueva** segunda llave. Si no, vuelves a tener solo una.
5. La parte pública de las claves SSH, de certificado y PGP por ahora **no** está en la segunda llave. Guárdala de nuevo, o un programa no podrá listar la clave.

#### Caso 2: Tienes una copia de seguridad y la frase secreta principal

1. Consigue una llave nueva.
2. Configúrala. Mira [installation.md](installation.md).
3. Carga la copia de seguridad. Para eso necesitas el archivo **y** la frase secreta principal.
4. Si ya no tienes la frase secreta principal pero tienes sobres: **tres de cinco** (o el número que elegiste) abren el archivo sin la frase secreta principal.
5. Ten en cuenta: el programa avisa de que cargar sustituye el contenido del dispositivo.

**Abierto:** Si una llave nueva puede volver a usar a partir de la copia de seguridad también tus **passkeys**, no está decidido en la documentación técnica. Para las passkeys confía en la segunda llave.

#### Caso 3: No tienes nada de eso, y elegiste "solo esta llave"

- Para los accesos creados con ella **no hay repuesto.**
- En cada servicio tienes que empezar de cero, por el camino que el servicio ofrezca para accesos perdidos.
- La llave misma no puede ayudarte en eso.

#### Caso 4: Todo perdido: las dos llaves y la frase secreta principal

- **Quien pierde la frase secreta principal y todas las llaves, lo pierde todo.**
- Nadie puede recuperar el contenido. Tampoco el proyecto. No hay ninguna puerta trasera que se pueda robar.

### Así te preparas

Estas cuatro cosas, lo mejor es hacerlas en la propia configuración:

1. **Emparejar la segunda llave** (solo con el camino A).
2. **Guardar la copia de seguridad**, guardada por separado de la frase secreta principal.
3. **Hoja de emergencia:** frase secreta principal en papel, en un lugar seguro. Además anotas dónde está la copia de seguridad.
4. **Sobres** para personas de tu confianza: una parte por sobre, tres de cinco abren.

**Nunca** guardes todo en el mismo lugar. El archivo y la frase secreta principal juntos abren todo.

El orden de tu seguridad:

- Segunda llave + copia de seguridad + hoja: un percance no es un drama.
- Solo segunda llave: un incendio o el robo del bolso se lleva las dos.
- Nada de eso: perder la llave es una pérdida total.

## La llave no se reconoce

En el programa, en **Dispositivos**, pone "No se encontró ningún dispositivo."

Consejos generales (no probados en la llave):

1. Probar otro puerto USB.
2. Otro cable, o directamente al ordenador, sin concentrador.
3. Desconectar la llave, volver a conectarla, reiniciar el programa.
4. ¿Quizá la batería está vacía? Dejar que se cargue.

Qué más cuenta:

- **macOS:** Sin la forma especial de construir (cgo), el programa arranca, pero **no encuentra ninguna llave real** y lo dice.
- Si quieres usar la llave de prueba, escribe su dirección en los ajustes. Mira [settings.md](settings.md).
- El identificador de la llave (número de fabricante y de producto) es **provisional** y todavía no está acordado con el firmware.

Mensajes al respecto:

| Mensaje | Significado | Qué hacer |
| --- | --- | --- |
| No se encontró ningún dispositivo. | Ninguna llave responde. | Mira arriba. |
| No se pueden listar los dispositivos de este ordenador. | El sistema se niega a dar la lista. | Reinicia el programa. Si no ayuda, avísalo con el texto entre paréntesis. |
| No se puede abrir el dispositivo. | Posible causa: otro programa usa la llave, o el sistema no lo permite. | Cierra los otros programas que usan la llave. |
| No se puede leer del dispositivo. / No se puede escribir en el dispositivo. | La conexión se ha cortado. | Vuelve a conectar la llave. |
| El dispositivo no respondió a tiempo. / tardó demasiado. | Quizá la llave espera tu pulsación. | Mira la pantalla de la llave. |
| La conexión con el dispositivo está perturbada. | Como arriba. | Vuelve a conectar la llave. |
| Este sistema operativo aún no es compatible. | Todavía no hay camino para este sistema. | Usa Windows o Linux. |
| El dispositivo usa otra versión del protocolo. | El programa y el firmware no encajan. | Actualiza el programa o el firmware. |

## El PIN está bloqueado

- **Tres PIN incorrectos seguidos** bloquean la llave hasta que la **desconectas y la vuelves a conectar.** Después puedes intentarlo de nuevo.
- El contador está en el chip de seguridad. Volver a instalar o borrar la memoria **no** lo pone a cero.
- **Cuántos intentos fallidos hay en total** antes de que el PIN se bloquee para siempre, y qué ocurre entonces, **no** lo dice la documentación técnica. Por eso no adivines. Busca primero el PIN.
- Para el lado de la tarjeta inteligente (PIV, OpenPGP) hay además un PUK y un PIN de administrador. Están en la caja fuerte, cada uno con su propio contador.

| Mensaje | Significado | Qué hacer |
| --- | --- | --- |
| El dispositivo está bloqueado. | Todavía no has desbloqueado. | **Desbloquear.** La autorización la pide la llave en su propia pantalla. |
| Falta la aprobación en el dispositivo. | No has confirmado (o con "No"), o tardó más de 30 segundos. | Inténtalo de nuevo. Pulsa largo, después pulsa largo. |

### Restablecer

En una emergencia se puede restablecer la parte de inicio de sesión (FIDO).

- **Esto es definitivo.** El secreto principal se renueva, **todo inicio de sesión antiguo queda sin valor.** Tienes que registrarte de nuevo en cada servicio.
- Restablecer **no** funciona por NFC.
- Borrar en la llave pregunta dos veces.
- Si eso también libera un PIN bloqueado, no está descrito en la documentación técnica.

## El navegador no acepta la llave

**Estado:** Nunca probado en un navegador. Esta lista solo conoce los **límites conocidos.**

- **Las páginas antiguas que solo saben U2F** quizá no acepten la llave. No envía ningún certificado de certificación.
- **Las páginas de empresas con certificado obligatorio** quizá la rechacen. Se certifica a sí misma.
- La llave se anuncia como **FIDO 2.0 y 2.1.** Los comandos para administrar passkeys (listar, borrar) y para registrar huellas dactilares en el navegador **no existen.**
- La llave **no informa de ninguna verificación de usuario integrada** (`uv`) mientras no haya un sensor dentro. Así es correcto. El PIN se informa por separado.
- Por **Bluetooth** **no** hay inicio de sesión en el navegador. Los navegadores no admiten FIDO por Bluetooth de forma fiable. Usa USB o NFC.
- El **contador de firmas** empieza en cero cada vez que se enciende. Una página que insista en contadores crecientes podría tomar la llave por una copia.
- El **identificador del fabricante** (AAGUID) es provisional.

Mensajes de la extensión del navegador:

| Mensaje | Significado | Qué hacer |
| --- | --- | --- |
| Esta entrada pertenece a otra direccion. | La entrada está guardada para otra página. | **Esta es la protección contra el phishing.** Comprueba la dirección. Si es correcta, cambia la dirección en la entrada. |
| Esta pagina no esta cifrada. Aqui no va ningun secreto. | La página no usa https. | No rellenar. |
| El canal hacia el navegador se ha cortado. | La extensión ha perdido la conexión. | Reinicia el navegador. |
| El archivo de registro no se pudo escribir. | La extensión no pudo registrarse. | Comprueba los permisos de la carpeta. |
| Ese no es un identificador de extension. / No conozco este navegador. | Dato incorrecto al registrar. | Comprueba el identificador y el nombre del navegador. |

## Mensajes de entradas y de la copia de seguridad

| Mensaje | Significado | Qué hacer |
| --- | --- | --- |
| Ya existe una entrada con ese nombre. | Nombre duplicado. | Elige otro nombre. |
| No queda espacio en el dispositivo. | La memoria del perfil está llena (768 entradas). | Borra entradas antiguas. |
| El dispositivo no conoce esta entrada. | Entrada borrada o mal nombrada. | Vuelve a cargar la lista. |
| El nombre coincide con varias entradas. | No es unívoco. | Nómbrala con más precisión. |
| Este dispositivo todavía no puede hacerlo. | La función falta en este firmware. | Actualiza el firmware, si hay uno más nuevo. |
| El archivo de copia está vacío o es demasiado grande. | Archivo equivocado. | Elige otro archivo. |
| Los datos del dispositivo no tienen la forma esperada. | Los datos están dañados, o proceden de un firmware **más nuevo.** Una versión más nueva nunca se lee mal, sino que no se abre. | Actualiza el firmware. Si no ayuda, carga la copia de seguridad. |
| La imagen del firmware está vacía. | No se eligió ningún archivo. | Elige un archivo. |
| No se puede leer el archivo de ajustes. / es demasiado grande. / contiene un error. | El archivo `config_osk.json` está dañado. | Bórralo. Entonces el programa toma los valores por defecto. |

## Mensajes del agente ssh y de las interfaces

| Mensaje | Significado | Qué hacer |
| --- | --- | --- |
| Este sitio ya esta ocupado. | Ya se está ejecutando otro agente o servicio. | Cierra el otro. |
| Esta clave esta cifrada con una contrasena. | La clave SSH de la caja fuerte todavía está protegida. | Guárdala descifrada en la caja fuerte. |
| Esta clave es demasiado corta y ya no es segura. | Clave débil. | Crea una clave nueva. |
| No puedo manejar este tipo de clave. | No admitido. | Elige otro tipo. |
| Se pidio esta clave, pero no esta en la caja fuerte. | El programa pide una clave que no tienes. | Guarda la clave en la caja fuerte. |
| El acceso no fue autorizado. / No llego respuesta a la pregunta sobre la autorizacion. | Has rechazado o no has pulsado nada. | Inténtalo de nuevo. |

Los números de salida para scripts los muestra:

```powershell
osk codes
```

## Mensajes de la protección por dongle

| Mensaje | Significado | Qué hacer |
| --- | --- | --- |
| No se ha encontrado ninguna llave de seguridad. Conéctela. | La llave no está conectada. | Conectarla. |
| Está conectada una llave distinta de la configurada. | El programa pertenece a otra llave. | Conecta la llave correcta. |
| El archivo no se puede abrir: clave incorrecta o archivo modificado. | La llave no encaja con el archivo cerrado, o el archivo se ha modificado. | Usa la llave que lo cerró. |
| La licencia ha caducado. | La licencia era auténtica y ha terminado. | Consigue una licencia nueva. |
| En esta llave de seguridad no hay licencia para el producto. | No hay licencia para el producto. | Guarda la licencia en la llave. |
| Este chip no puede hacer acuerdo de claves. | El chip se configuró así de fábrica. El modo servidor con la clave dentro del chip queda descartado. Firmar sigue funcionando. | Esto no se puede cambiar. |

## Esto todavía no funciona

Para que no busques mucho:

- **KeePassXC y VeraCrypt** con la llave
- **Iniciar sesión en el ordenador,** desbloquear el disco duro, la llave como teclado
- una hoja de emergencia ya impresa por el programa
- **macOS**

## Si no lo encuentras

Avísalo en el proyecto en GitHub (sorglos-it/opensecurekey) e indica: sistema operativo, versión de `osk`, el mensaje exacto y qué hiciste antes. Las contraseñas y los PIN no van nunca en un aviso.
