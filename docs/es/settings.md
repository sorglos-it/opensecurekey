# Ajustes

**Estado:** Los ajustes del programa están comprobados contra el dispositivo de prueba. Los ajustes en la propia llave todavía no existen en una llave real.

Hay pocos ajustes. Es a propósito.

## En el programa `osk`

Abre en la interfaz la página **Ajustes.** Se guarda en el archivo `config_osk.json` **junto al programa**, no en el sistema.

| Ajuste | Significado | Valor por defecto | Valores permitidos |
| --- | --- | --- | --- |
| Idioma | Idioma de la interfaz | `de` | `de`, `en`, `fr`, `it`, `es` |
| Nombre de este programa | El nombre que el programa da a la llave. Acaba en el registro. | `osk` | Texto de hasta 64 caracteres |
| Puerto | Conexión en la que escucha la interfaz | `8101` | Número de 1024 a 65000 |
| Dirección del dispositivo de prueba | Solo para probar: dirección de la llave de prueba | vacío | por ejemplo `127.0.0.1:8201`, y solo en este ordenador |

Bueno saber:

- **El nombre es una afirmación.** La llave lo escribe en el registro, pero no demuestra nada. Solo la propia llave puede negarse a algo.
- Si el puerto está ocupado, el programa busca uno libre a partir del valor ajustado. Si no encuentra ninguno, lo dice.
- **Deja vacía la dirección del dispositivo de prueba** mientras uses una llave real. El dispositivo de prueba es una herramienta de pruebas. No guardes ahí nunca secretos reales. Su PIN es `123456`, y no cifra nada.
- Si el archivo de ajustes está dañado o es demasiado grande, el programa arranca igualmente. Toma los valores por defecto y dice por qué.

Ejemplo (`config_osk.json`):

```json
{
  "lang": "de",
  "port": 8101,
  "appName": "osk",
  "simAddress": ""
}
```

## Por cada entrada en la llave

Cada entrada tiene tres interruptores. Mira [usage.md](usage.md), sección "Tipos de entrada".

- se puede replicar
- solo ordenadores emparejados
- protegido contra escritura

## En la propia llave

Estas cosas las ajustas **en la llave**, no en el programa. **Estado:** todavía no probado en ningún sitio.

| Ajuste | Valor por defecto | Qué cambia |
| --- | --- | --- |
| Perfil y PIN | hasta 4 perfiles, cada uno con su PIN | Quién ve qué |
| Función de toque NFC (abrir una dirección o teclear un código único) | se puede desactivar | Un teléfono abre una dirección al acercarlo |
| Firmar a ciegas | desactivado | La llave firma también cosas que no puede mostrar de forma legible. La pregunta en la pantalla se ve entonces claramente distinta. |
| Autorización de servidor | desactivada | Un servidor puede firmar sin pulsar, para claves indicadas, como máximo 24 horas |
| Bluetooth | solo para el programa de administración | No para iniciar sesión en el navegador. Emparejar solo con comparación de números y pulsación en la llave. |

Bueno saber:

- **Todo lo arriesgado está desactivado** hasta que lo actives a propósito.
- **NFC siempre necesita el botón.** Incluso cuando NFC dura poco. Eso protege de un ataque desde el bolsillo en el que alguien retransmite la conexión por radio.
- La dirección de radio de Bluetooth cambia para que no se pueda seguirte.

## Idioma en la llave

La pantalla de la llave conoce los mismos cinco idiomas. Si falta un texto, usa inglés, nunca un hueco vacío.

## Cambiar el idioma

Cambia el idioma en el programa, en **Ajustes.** Los archivos de idioma están en la carpeta `lang`, uno por idioma.
