# Uso

**Estado:** Nada de esto ha funcionado en una llave real. En cada función se indica hasta dónde está comprobada.

## La regla para todo

- **No sale nada si no pulsas en la llave.**
- La pantalla de la llave indica **qué** estás autorizando: el servicio, el destinatario, el importe.
- Lo que la llave no puede mostrar de forma legible, no lo autoriza.
- Un programa dañino en el ordenador puede preguntar. No puede pulsar el botón, y no puede cambiar lo que dice la pantalla.

## El botón

La llave tiene **un** botón y tres formas de pulsarlo.

| Pulsación | Duración | Significado |
| --- | --- | --- |
| corta | menos de 0,8 segundos | página siguiente. En la página de la pregunta: **No.** |
| larga | desde 0,8 segundos | a la página de la pregunta. Ahí: **Sí.** |
| doble | dos veces corta, seguidas y rápidas | volver a leer todo desde la página 1 |

Así transcurre una autorización:

1. **Leer.** Cada página muestra el texto. Con una pulsación corta pasas de página.
2. **Decidir.** Una pulsación larga muestra la página de la pregunta. Ahí: larga = Sí, corta = No.
3. **Borrar: se pregunta dos veces.** Dos pulsaciones largas en dos páginas de pregunta.

Una pulsación larga en una página de lectura **no** autoriza nada. Solo lleva a la página de la pregunta. Nadie autoriza algo que no ha visto.

Después de 30 segundos sin pulsar, cuenta como "No".

Los nombres y direcciones largos **nunca se cortan.** Se reparten en varias páginas. Una dirección de destinatario cortada sería peor que no tener pantalla.

**Estado:** En una pantalla real nunca se ha mostrado nada todavía. Si 0,8 segundos se sienten bien en la mano, nadie lo sabe.

## Iniciar sesión en una página web (passkey)

**Estado:** Nunca probado en un navegador.

1. En la página elige "Iniciar sesión con llave de seguridad" o "Passkey".
2. Si el ordenador pide el PIN: introduce el PIN.
3. La pantalla de la llave indica el servicio.
4. Lee. **Pulsa largo** para ir a la página de la pregunta, ahí **pulsa largo** para Sí.

Bueno saber:

- Una passkey vale **solo para la dirección auténtica** de la página. En una página imitada no funciona.
- La llave entiende las passkeys, el inicio de sesión U2F más antiguo y la extensión con la que programas como Bitwarden, BitLocker y LUKS derivan una clave (`hmac-secret`).
- **Las páginas antiguas que solo saben U2F** posiblemente rechacen la llave. No envía ninguna certificación.
- Algunas páginas de empresas exigen un certificado de certificación. La llave no lo tiene.
- Por NFC, cada autorización también necesita el botón.

## Acercar al teléfono (NFC)

**Solo con corriente USB.** La llave no tiene batería. NFC funciona solo mientras está enchufada a un puerto USB: el teléfono va hacia la llave, no al revés. Acercar la llave suelta al teléfono ya no funciona. No se ha comprobado si basta un simple cargador USB sin ordenador.

**Estado:** nunca probado con un teléfono real. El alcance no se ha medido.

- Sostén la llave **más cerca del lector que una tarjeta.**
- Se debe a la forma del dispositivo. No es un fallo. En la carcasa pequeña la antena es más pequeña que la de una tarjeta.
- La distancia exacta solo se sabrá tras medir el dispositivo terminado. Por eso aquí no hay ninguna cifra.
- Si no funciona enseguida: desplaza la llave un poco, despacio. No presiones más fuerte.
- Incluso por NFC, cada autorización necesita el botón de la llave.

## Obtener una contraseña

**Estado:** comprobado contra el dispositivo de prueba, nunca contra una llave real.

### En la interfaz

1. Inicia `osk` y abre la dirección en el navegador.
2. **Desbloquea** la llave (PIN).
3. Busca la entrada y ábrela.
4. Elige **Mostrar.**
5. Confirma en la llave.
6. Con **Copiar** la pones en el portapapeles. Se vacía de nuevo después de unos **20 segundos.**

### Por comando

El PIN nunca se pasa como parámetro. Llega por la entrada estándar, porque los parámetros son visibles para otros programas.

```powershell
osk unlock
```

```powershell
osk list
```

```powershell
osk get "Mein Konto"
```

Si un nombre coincide con varias entradas, el programa no adivina. Dice que el nombre no es unívoco.

### Rellenar en el navegador

Hay una extensión del navegador. Rellena un campo de inicio de sesión **solo en la dirección guardada.** Para otra dirección se niega. Eso es la protección contra páginas imitadas, no un fallo de comodidad. En una página sin cifrar (sin https) no entrega nada.

**Estado:** Un campo de inicio de sesión se rellenó en un Chromium real, con el dispositivo de prueba. Con una llave, nunca.

### Partes públicas sin pulsar

La parte **pública** de una clave SSH, de un certificado o de una clave PGP sale **sin** pulsar en la llave. Es a propósito: si tuvieras que pulsar por cada nombre, pronto pulsarías sin mirar. Entonces la única pulsación que cuenta no valdría nada: la de antes de firmar.

## Un código para el inicio de sesión en dos pasos (2FA)

**Estado: abierto.**

- La llave puede guardar el valor inicial de los códigos de un solo uso, como entrada "Código único (tiempo)" o "Código único (contador)".
- La llave no tiene batería. El ordenador pone en hora el reloj al enchufarla (**previsto, todavía no construido**). Si el reloj del ordenador va mal, los códigos también. Los códigos basados en el tiempo necesitan la hora.
- **Cómo se muestra el código de seis cifras ya terminado en la interfaz o en la llave no está descrito en la documentación técnica.** Sobre eso no escribo nada aquí.
- El comando `osk codes` **no** muestra estos códigos, sino la lista de los números de salida para scripts.
- KeePassXC y VeraCrypt trabajan con un procedimiento de Yubico. Eso todavía falta: **Ninguno de los dos programas funciona con el estado actual.**

## Firmar un archivo o un mensaje

**Estado:** Núcleo de cálculo escrito y comprobado en el ordenador. Con un programa real de tarjeta inteligente (GnuPG, OpenSC) nunca probado.

La llave se comporta como una **tarjeta inteligente.** Para ello Windows, Linux y macOS no necesitan ningún controlador.

- Puede presentarse como **tarjeta PIV** y como **tarjeta OpenPGP.**
- Las claves están en el chip de seguridad. No salen nunca de él.
- **Cada firma necesita el botón.** La pantalla indica el servicio.
- En PIV, el PIN debe volver a comprobarse **inmediatamente antes de cada** firma en la posición "9C".
- Se admiten curvas elípticas (P-256 y P-384). **RSA por ahora no** (el núcleo de cálculo lo rechaza).

Procedimiento:

1. Pide al programa (por ejemplo GnuPG) que firme con la clave de la tarjeta.
2. Introduce el PIN cuando te lo pidan.
3. En la llave lee el nombre, **pulsa largo**, **pulsa largo.**

### Por SSH y Git

**Estado:** Comprobado con el dispositivo de prueba. `git push` a través del agente funcionó en Windows y Linux.

1. Inicia el agente:

```powershell
osk agent
```

2. Muestra las claves de la caja fuerte:

```powershell
osk keys
```

3. Cada firma la confirmas en la llave.

Para programas que hablan PKCS#11 hay una biblioteca (`osk-pkcs11`, como .dll y .so). Con ella `openssl` comprobó una firma como "Verified OK", con el dispositivo de prueba.

### Dar un secreto a un programa

```powershell
osk run NAME=eintrag -- programm
```

El secreto va solo al programa iniciado. La shell de al lado no lo ve.

## Iniciar sesión en el ordenador

**Esto todavía no funciona.**

- Iniciar sesión en Windows, Linux o el Mac con la llave: no construido.
- Desbloquear el disco duro y la llave como teclado (la llave escribe tu contraseña): no construido.
- Estas partes necesitan derechos de administrador una vez para configurarse. Eso es cosa del sistema operativo.

Solo están construidos y comprobados con el dispositivo de prueba: agente ssh, línea de comandos, PKCS#11, extensión del navegador y una interfaz local para programas propios.

## Cartera de criptomonedas (Bitcoin y Ethereum)

**Estado:** Comprobado en el ordenador. **Nunca se envió un pago real.**

- La llave solo firma un pago si **el destinatario, el importe y la comisión** se pudieron leer en la pantalla.
- Si falta uno de ellos, la llave dice **No**, sin preguntar al chip.
- **No legibles** son, entre otros, los contratos, los importes de tokens y los Typed Data. La llave solo firma estas cosas si en la llave has **activado** "firmar a ciegas".
- Nunca se firma un pago a medias. Si una parte falla, falla todo.
- No admitido: entradas Taproot.
- Tu frase de cartera se crea de forma que no se copie a una segunda llave y que solo vaya a ordenadores emparejados.

## Programa solo con llave (dongle) y servidor

**Estado:** Solo con el dispositivo de prueba. El lado de la llave todavía no existe.

- **Nivel 1** (la llave dice "estoy aquí") detiene a quien solo **copia** un programa. Quien sabe desmontar un programa, quita la comprobación.
- **Nivel 2** (los datos están cerrados con una clave del chip) es en principio más fuerte: sin la llave los datos no se pueden leer.
- Un servidor puede firmar con la llave, en lugar de con un archivo de clave. **Sin autorización** un servidor se queda parado, porque nadie pulsa a las 3 de la madrugada. La autorización está **desactivada**, nombra claves, caduca **como máximo a las 24 horas** y se registra. **Con** autorización, un programa dañino en ese servidor puede firmar.

Más sobre esto: [how-it-works.md](how-it-works.md).

## El registro de la llave

La llave lleva un registro cifrado propio. Muestra **qué programa** obtuvo **cuándo** qué secreto. Está en la llave, no en el ordenador. Contiene los últimos pocos cientos de eventos.

```powershell
osk log
```

El nombre del programa en el registro es una **afirmación** del programa, no una prueba.

Los secretos nunca están en el registro.

## Varias personas con una llave

Una llave tiene hasta **cuatro perfiles**, cada uno con su propio PIN. El PIN de uno no abre ningún otro. Un perfil bloqueado no muestra nada, ni siquiera los nombres de las entradas. Espacio: **768 entradas por perfil**, **3072 en la llave.**

## Tipos de entrada

Acceso, Código único (tiempo), Código único (contador), Clave SSH, Certificado, Clave PGP, Clave API, Códigos de recuperación, Frase de cartera, Clave de licencia, Tarjeta bancaria, Identidad, Wi-Fi, Nota, Archivo.

Cada entrada lleva tres marcas que pones tú mismo:

| Marca | Efecto |
| --- | --- |
| se puede replicar | pasa a tu segunda llave, y solo allí |
| solo ordenadores emparejados | solo se entrega a un ordenador emparejado |
| protegido contra escritura | no se puede cambiar ni borrar, ni siquiera por ti, hasta que lo levantes |
