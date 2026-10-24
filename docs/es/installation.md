# Configuración

**Estado:** Todavía no existe la llave. Estos pasos describen el proceso previsto. El programa `osk` solo funciona por ahora contra un dispositivo de prueba. Todavía no existe un asistente de configuración que guíe los pasos 3 a 5: los textos están escritos, el programa que los usa no.

## Lo que necesitas

- la llave
- un ordenador con Windows o Linux (macOS: nunca probado)
- un navegador
- tiempo para leer con calma la pregunta del paso 3

**No** necesitas ninguna instalación y **no** necesitas derechos de administrador para el programa `osk`.

## Paso 1: Conseguir el programa

- `osk` es **un solo archivo** por sistema operativo.
- Guarda sus ajustes **junto a sí mismo**, no en el sistema.
- Todavía no hay un programa terminado para descargar. No se ha construido ninguna versión (release). Quien quiera construirlo por su cuenta lee [development.md](development.md).

## Paso 2: Conectar la llave, iniciar el programa

1. Conecta la llave a un puerto USB.
2. Inicia `osk`.
3. El programa escribe una dirección, por ejemplo `http://127.0.0.1:8101/`.
4. Abre esa dirección en el navegador.
5. En **Dispositivos** debe aparecer tu llave. Si ahí pone "No se encontró ningún dispositivo.", te ayuda [troubleshooting.md](troubleshooting.md).

La dirección solo lleva a tu propio ordenador. Desde fuera nadie llega a esa página.

## Paso 3: La pregunta única

La interfaz te hace primero esta pregunta:

> **¿Debe ser posible una llave de repuesto más adelante?**

**Esta respuesta ya no se puede cambiar.** No es una advertencia de precaución, es técnica: un secreto que ha nacido dentro del chip de seguridad no sale nunca más. Por eso no se puede copiar después a una segunda llave.

Hay dos caminos. Los dos tienen un precio.

### Camino A: "Sí, repuesto posible"

- **Esto obtienes:** Podrás crear más adelante una segunda llave que haga lo mismo. Si pierdes una, usas la otra y vuelves a entrar en todo.
- **Esto pagas:** Tu secreto principal se crea **una vez fuera del chip de seguridad** durante la configuración. En ese único momento, un programa dañino en tu ordenador podría leerlo. Nunca más después.

### Camino B: "No, solo esta llave"

- **Esto obtienes:** Todo se queda para siempre en el chip de seguridad. Ningún secreto sale de él, en ningún momento.
- **Esto pagas:** **No hay repuesto.** Si la llave se pierde o se rompe, ya no puedes entrar en los accesos creados con ella. Tendrías que empezar de cero en cada servicio.

### Lo que recomienda el programa

- **Para el uso diario:** repuesto posible. Quien pierde su única llave pierde de lo contrario todos los accesos.
- **Para una cartera de criptomonedas:** Para eso puedes crear más adelante un **segundo perfil sin repuesto.**

Ninguno de los dos caminos es el más seguro en general. Depende de qué temas más: ese único momento en el ordenador, o la pérdida de la única llave.

### Cambiar de idea

- Solo se puede así: **crear un perfil nuevo** y **registrarte otra vez en cada servicio.**
- Una llave admite hasta **cuatro perfiles.** Cada uno tiene su propio PIN.

Antes de seguir tienes que marcar: "Entiendo que esto ya no se podrá cambiar".

## Paso 4: PIN

- Sin PIN no funciona nada. Quien encuentre la llave no llega a nada sin el PIN.
- El PIN tiene al menos **4 caracteres.** Las diéresis (como ä, ö, ü) cuentan como un carácter cada una.
- Cuántos intentos fallidos hay en total antes de que el PIN se bloquee para siempre, **todavía no está decidido.** Tres fallos seguidos bloquean por ahora solo hasta que desconectas la llave. Más información en [troubleshooting.md](troubleshooting.md).
- Los intentos fallidos los cuenta el **chip de seguridad.** Nadie puede poner el contador a cero borrando la memoria.
- **Abierto:** Todavía no está descrito dónde se fija el PIN por primera vez.

## Paso 5: Asegurarte antes de usar la llave

Este es el paso más importante. Hazlo **enseguida**, no "más tarde".

1. **Crear una segunda llave** (solo con el camino A). Mira abajo.
2. **Guardar una copia de seguridad.** Mira abajo.
3. **Escribir la hoja de emergencia.** Mira abajo.

Está previsto que el asistente de configuración te obligue a crear una segunda llave **o** una hoja de emergencia impresa. Todavía no está construido. Haz las dos cosas.

### Emparejar la segunda llave

1. Conecta las dos llaves.
2. En la interfaz elige **Emparejar un dispositivo de reserva.**
3. **Las dos llaves muestran el mismo número de seis cifras.** Compáralos.
4. Si los números son **iguales:** pulsa en las dos llaves.
5. Si son **distintos:** no pulses. Entonces hay alguien en medio. En ese caso no se transmite nada.

Lo que se transmite:

- Solo las entradas con la marca **"se puede replicar".**

Lo que **no** se transmite:

- todo lo demás
- una frase de cartera: se crea de forma que no se copie a una segunda llave

El ordenador y el enlace por radio entre las llaves solo ven datos cifrados.

**Laguna conocida:** La parte pública de las claves SSH, de los certificados y de las claves PGP por ahora **no** pasa a la segunda llave y no está en la copia de seguridad. La clave funciona allí, pero un programa solo puede listarla cuando la parte pública se ha vuelto a guardar.

### Guardar una copia de seguridad

1. En la interfaz abre **Copia de seguridad** y elige **Guardar una copia.**
2. Eliges una **frase secreta principal.** Tiene al menos **doce caracteres.** No cifras, sino caracteres.
3. Acepta la propuesta de la llave: **ocho palabras.** Eso es más difícil de adivinar que casi todo lo que inventan las personas, y más fácil de escribir en papel.
4. El archivo está cifrado. El ordenador no puede leerlo.
5. Guarda el archivo en un disco o en un NAS.

**Importante:**

- Guarda el **archivo y la frase secreta principal por separado.** Juntos abren todo.
- Con el archivo y la frase secreta principal se puede configurar una **llave completamente nueva.** El archivo no depende de la llave que lo escribió.
- El programa avisa al cargar: "La carga sustituye el contenido del dispositivo." Así que no cargues por error.

### Hoja de emergencia

Una hoja de emergencia es una hoja de papel que guardas **en un lugar seguro.** Contiene tu **frase secreta principal.**

Si quieres que también otras personas puedan acceder en una emergencia, se puede hacer con **sobres:**

- La llave divide la clave de la copia de seguridad en varias partes, por ejemplo cinco.
- Cada parte es **una línea de texto.** Imprímelas, un sobre cerrado por persona.
- **Tres de cinco** juntas abren la copia de seguridad, sin la frase secreta principal.
- **Un solo sobre no revela nada.** Ni siquiera una letra.
- Una parte mal copiada se nota. No sale un resultado falso.

Tú decides el número de partes y cuántas hacen falta.

**Abierto:** Una "hoja de emergencia" ya impresa y generada por el programa no está descrita como función. Escribe tú mismo tu frase secreta principal y el lugar donde está la copia de seguridad.

## Actualización

### Firmware (el software dentro de la llave)

1. En la interfaz abre **Firmware** y elige **Escribir el firmware.**
2. **No desconectes la llave mientras se escribe.**
3. La llave acepta **solo imágenes firmadas.**

Tu caja fuerte se conserva. Una versión más antigua de la caja fuerte se lee y se vuelve a escribir al usarla. Una caja fuerte de un firmware **más nuevo** ni siquiera se abre, en lugar de leerse mal.

**Estado:** Escribir en una llave real nunca se ha probado. La herramienta para ello (`tools/flash.py`) no está comprobada.

### El programa `osk`

Todavía no hay función de actualización. Sustituyes el archivo por uno más nuevo.

## Quitar

- **Programa:** borra el archivo `osk`. Los ajustes están al lado, no en el sistema.
- **Vaciar la llave:** Mira [troubleshooting.md](troubleshooting.md), sección "Restablecer". **Esto es definitivo.**
