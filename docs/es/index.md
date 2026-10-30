# OpenSecureKey – Manual

**Este dispositivo todavía no se puede comprar, y hasta ahora nadie lo ha usado.**

Todo lo que dice este manual describe lo que se está construyendo. Nada de esto ha funcionado nunca en una llave real. Todavía no hay ninguna llave construida. Donde algo solo se ha comprobado en el ordenador, está indicado.

## ¿Qué es?

Una llave de seguridad para el llavero. Guarda tus accesos, y no sale nada si tú mismo no pulsas el botón en la llave.

En concreto, esto significa:

- Te inicia sesión en páginas web sin que escribas una contraseña (passkeys).
- Guarda contraseñas, códigos, claves y certificados.
- Puede comportarse como una tarjeta inteligente.
- Puede firmar pagos de Bitcoin y Ethereum.
- Puede dejar que un programa funcione solo mientras la llave está conectada (dongle).

La llave tiene una pantalla pequeña propia. Ahí aparece **qué** estás autorizando en ese momento. El ordenador no puede engañarte en eso.

## Cómo está organizado este manual

| Archivo | Para qué sirve |
| --- | --- |
| [installation.md](installation.md) | Primera configuración, paso a paso. Ahí está la pregunta que ya no se puede cambiar. |
| [usage.md](usage.md) | El día a día: iniciar sesión, obtener una contraseña, un código, firmar. |
| [settings.md](settings.md) | Lo que se puede ajustar. |
| [how-it-works.md](how-it-works.md) | Cómo trabaja la llave por dentro. Y lo que **no** puede hacer. |
| [troubleshooting.md](troubleshooting.md) | Cuando algo no funciona. Y: **cuando la llave se ha perdido.** |
| [development.md](development.md) | Para quien construye el proyecto por su cuenta. |

## Lo más importante de entrada

- **Una pregunta de la configuración ya no se puede cambiar:** ¿debe ser posible una llave de repuesto más adelante? Léela con calma. Mira [installation.md](installation.md).
- **Sin llave de repuesto y sin copia de seguridad, perder la llave es una pérdida total.**
- **Quien pierde la frase secreta principal y todas las llaves, lo pierde todo.** No hay puerta trasera para esto. Mira [troubleshooting.md](troubleshooting.md).
- **La hoja de emergencia durante la configuración no se puede omitir.** Justamente por eso. Mira [installation.md](installation.md).
- **No hay ninguna revisión externa** y **ninguna prueba en hardware real.** Por ahora no confíes en nada que no puedas reemplazar.

## Lo que la llave no puede hacer

Explicado con detalle en [how-it-works.md](how-it-works.md). En resumen:

- En la primera fase de desarrollo **no tiene sensor de huella dactilar.** Y para ninguno de los sensores examinados está demostrado que reconozca una huella falsa.
- **No** protege de que alguien te obligue a pulsar el botón.
- **No** protege de un firmware modificado.
- **No** te salva si la frase secreta principal y todas las llaves han desaparecido.
- **No entrega ninguna clave privada como archivo. Nunca.** El software que exige obligatoriamente un archivo de clave (`.pem`, `.pfx`) no puede manejarla de ninguna manera. El certificado puede salir, la clave no. Mira [usage.md](usage.md).
- No conoce **ningún perfil para passkeys.** Al cambiar de perfil te registras de nuevo en los servicios.
- **«Enchufar y arrancar» significa esperar más de un minuto.** La llave entrega sus datos despacio.
- En la tarjeta de memoria prevista, **uno de tres modos de funcionamiento no cifra.** Y una firma demuestra que el contenido no ha cambiado, no su autoría. Mira [how-it-works.md](how-it-works.md).

## Estado de las partes

| Parte | Estado |
| --- | --- |
| Dispositivo (placa, carcasa) | todavía no construido |
| Firmware | escrito, probado en el ordenador con dispositivos de prueba, nunca en la llave |
| Programa `osk` en el ordenador | comprobado contra un dispositivo de prueba, nunca contra una llave real |
| Windows y Linux | compilado y probado con el dispositivo de prueba |
| macOS | nunca probado |
| Navegador y páginas web | nunca probado |

Técnica más detallada (en inglés, para desarrolladores): [`docs/`](../) en la carpeta del proyecto.
