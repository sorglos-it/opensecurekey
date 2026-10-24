# Construir y desarrollar por tu cuenta

Este capítulo es para quien construye el proyecto por su cuenta. La técnica detallada está en inglés en la carpeta [`docs/`](../) (por ejemplo `architecture.md`, `threat-model.md`, `vault.md`, `build.md`).

**Estado:** Todavía no existe ninguna llave. Se construye y se prueba en el ordenador, con dispositivos de prueba para el chip, la memoria y la pantalla.

## Carpetas

| Carpeta | Contenido | Lenguaje |
| --- | --- | --- |
| `apps/firmware` | Software dentro de la llave (nRF52840 + SE051) | C / Zephyr |
| `apps/desktop` | Programa de administración `osk` con interfaz local | Go |
| `apps/web` | Extensión del navegador | JavaScript |
| `shared/` | Protocolo, formato de la caja fuerte, biblioteca `osk-guard` | |
| `hardware/` | Esquema, asignación de pines, notas de diseño | |
| `cad/` | Carcasa | |
| `tools/` | Herramientas de construcción y de comprobación | Python |

## Lo que necesitas

| Parte | Herramienta | Sin esta herramienta |
| --- | --- | --- |
| todas | Python 3 | no funciona nada |
| `apps/desktop` | Go 1.24 o más nuevo | se omite |
| `apps/web` | Node.js 20 o más nuevo (solo para comprobar) | Construir funciona, comprobar se omite |
| `apps/firmware` | nRF Connect SDK (Zephyr, `west`) | se omite |

"Omitido" significa siempre: **no construido, no comprobado.**

## Comprobar todo

```powershell
python tools/check.py
```

- Comprueba lo que se puede sin hardware.
- Al final dice cuántos pasos se ejecutaron, qué se omitió y cuántas pruebas se ejecutaron.
- El código de salida solo es 0 si nada falló **y** se ejecutó al menos una prueba.

## Construir

```powershell
python tools/build.py desktop
```

```powershell
python tools/build.py web
```

```powershell
python tools/build.py firmware
```

Los resultados están en `dist/`.

El programa `osk` también se puede construir directamente. No necesita ningún paquete externo ni cgo (salvo para macOS).

```powershell
go build -C apps\desktop -trimpath -ldflags "-s -w" -o ..\..\dist\osk.exe .
```

## Probar sin llave

La llave de prueba (el dispositivo de prueba) funciona solo en la memoria. No cifra nada y solo finge que has pulsado el botón. **No guardes ahí nunca secretos reales.**

```powershell
go run -C apps\desktop .\tests\simulator -port 8201
```

Después escribes `127.0.0.1:8201` en **Ajustes → Dirección del dispositivo de prueba.** El PIN del dispositivo de prueba es `123456`.

Las pruebas del programa:

```powershell
go test -C apps\desktop ./...
```

## Cuatro niveles de "comprobado"

Cada afirmación sobre "comprobado" se refiere a uno de cuatro niveles:

1. escrito
2. compilado
3. en la imagen
4. ha funcionado en la llave

Este proyecto está en el **nivel 2, además de ejecuciones en el ordenador con dispositivos de prueba.** Los niveles 3 y 4 faltan en todas partes.

## Decisiones y su motivo

| Decisión | Motivo |
| --- | --- |
| Dos chips: el chip de radio habla, el chip de seguridad guarda los secretos | La protección de lectura del chip de radio ya fue burlada. |
| Dos caminos de configuración, elegibles una sola vez | El chip de seguridad no deriva ninguna clave en sí mismo. Un secreto nacido en el chip no se puede replicar nunca. |
| Pantalla y botón en cada autorización | El ordenador no debe decidir lo que autorizas. |
| Ninguna autorización sin una pantalla que funcione | No podrías ver lo que autorizas. |
| Los nombres y direcciones nunca se cortan | Un destinatario cortado es peor que no tener pantalla. |
| Nivel 1 sin sensor de huella dactilar | En FIDO2 sustituye al PIN, no lo complementa. Ningún sensor cumplía las cuatro condiciones. |
| Espacio de actualización en la memoria externa | La memoria interna del chip de radio es escasa. |
| Letra propia y comprobadores propios para el protocolo | Ninguna biblioteca ajena de gráficos o de análisis, límites duros, sin memoria dinámica. |
| Ningún código ajeno | Todo está escrito contra la especificación. El cifrado viene siempre de bibliotecas comprobadas. |

## Cinco idiomas

- Ningún texto está fijo en el código.
- Cada idioma es un archivo en la carpeta `lang/`: `de.json`, `en.json`, `fr.json`, `it.json`, `es.json`.
- Una prueba informa de las claves que faltan como error.
- Si falta un texto, vale el inglés.

## Montaje

**Nuevo:** esta sección es nueva. Aquí se añadirán más normas de montaje cuando el hardware esté sobre la mesa.

- La lámina de ferrita bajo la batería debe medir **exactamente 19,75 mm** de ancho, **no más ancha, ni un poco.**
- Si no, la celda se inclina.

## Colaborar

Nunca cambies solo la técnica. Cuando cambian la función, el manejo, los ajustes o la estructura, la documentación forma parte: `docs/` y este manual.
