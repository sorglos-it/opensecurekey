# Uso

**Stato:** Niente di tutto questo è stato eseguito su una chiave vera. Per ogni funzione è indicato fino a che punto è stata verificata.

## La regola per tutto

- **Non esce nulla, se non premi sulla chiave.**
- Lo schermo della chiave dice **che cosa** autorizzi: il servizio, il destinatario, l'importo.
- Ciò che la chiave non può mostrare in modo leggibile, non lo autorizza.
- Un programma dannoso sul computer può fare una richiesta. Non può premere il pulsante, e non può cambiare ciò che dice lo schermo.

## Il pulsante

La chiave ha **un** pulsante e tre modi di premerlo.

| Pressione | Durata | Significato |
| --- | --- | --- |
| breve | meno di 0,8 secondi | pagina successiva. Nella pagina della domanda: **No.** |
| lunga | da 0,8 secondi in su | alla pagina della domanda. Lì: **Sì.** |
| doppia | due volte breve, in rapida successione | rileggere tutto dalla pagina 1 |

Così si svolge un'autorizzazione:

1. **Leggere.** Ogni pagina mostra il testo. Con una pressione breve sfogli.
2. **Decidere.** Una pressione lunga mostra la pagina della domanda. Lì: lunga = Sì, breve = No.
3. **Cancellare chiede due volte.** Due pressioni lunghe su due pagine di domanda.

Una pressione lunga su una pagina di lettura **non** autorizza nulla. Porta solo alla pagina della domanda. Nessuno autorizza qualcosa che non ha visto.

Dopo 30 secondi senza pressione vale come "No".

I nomi e gli indirizzi lunghi **non vengono mai tagliati.** Scorrono su più pagine. Un indirizzo di destinatario tagliato sarebbe peggio di nessuno schermo.

**Stato:** Su uno schermo vero non è mai stato mostrato niente. Se 0,8 secondi si sentano giusti in mano, nessuno lo sa.

## Accedere a un sito web (passkey)

**Stato:** Mai provato in un browser.

1. Sul sito scegli "Accedi con chiave di sicurezza" oppure "Passkey".
2. Se il computer chiede il PIN: inserisci il PIN.
3. Lo schermo della chiave indica il servizio.
4. Leggi. **Premi a lungo** per la pagina della domanda, lì **premi a lungo** per Sì.

Buono a sapersi:

- Una passkey vale **solo per l'indirizzo vero** del sito. Su un sito contraffatto non funziona.
- La chiave capisce le passkey, il vecchio accesso U2F e l'estensione con cui programmi come Bitwarden, BitLocker e LUKS ricavano una chiave (`hmac-secret`).
- **I vecchi siti che sanno solo U2F** potrebbero rifiutare la chiave. Non invia nessuna attestazione.
- Alcuni siti aziendali richiedono un certificato di attestazione. La chiave non lo ha.
- Anche via NFC ogni autorizzazione richiede il pulsante.

## Prendere una password

**Stato:** verificato con il dispositivo di prova, mai con una chiave vera.

### Nell'interfaccia

1. Avvia `osk` e apri l'indirizzo nel browser.
2. **Sblocca** la chiave (PIN).
3. Cerca la voce e aprila.
4. Scegli **Mostra.**
5. Conferma sulla chiave.
6. Con **Copia** la metti negli appunti. Dopo circa **20 secondi** gli appunti vengono svuotati di nuovo.

### Con un comando

Il PIN non viene mai passato come parametro. Arriva dall'input standard, perché i parametri sono visibili ad altri programmi.

```powershell
osk unlock
```

```powershell
osk list
```

```powershell
osk get "Mein Konto"
```

Se un nome corrisponde a più voci, il programma non tira a indovinare. Dice che il nome non è univoco.

### Far compilare dal browser

Esiste un'estensione per il browser. Compila un campo di accesso **solo sull'indirizzo salvato.** Per un altro indirizzo si rifiuta. È la protezione dai siti contraffatti, non un difetto di comodità. Su una pagina non cifrata (senza https) non consegna nulla.

**Stato:** Un campo di accesso è stato compilato in un vero Chromium, con il dispositivo di prova. Con una chiave mai.

### Parti pubbliche senza pressione

La parte **pubblica** di una chiave SSH, di un certificato o di una chiave PGP arriva **senza** pressione sulla chiave. È voluto: se premessi per ogni nome, presto premeresti senza guardare. Allora l'unica pressione che conta non varrebbe più niente: quella prima di firmare.

## Un codice per l'accesso a due fattori (2FA)

**Stato: aperto.**

- La chiave può conservare il valore di partenza per i codici monouso, come voce "Codice unico (tempo)" o "Codice unico (contatore)".
- La batteria tiene in funzione l'orologio. I codici basati sul tempo hanno bisogno dell'ora.
- **Come il codice finale di sei cifre venga mostrato nell'interfaccia o sulla chiave non è descritto nella documentazione tecnica.** Su questo qui non scrivo nulla.
- Il comando `osk codes` **non** mostra questi codici, ma l'elenco dei numeri di uscita per gli script.
- KeePassXC e VeraCrypt lavorano con un procedimento Yubico. Questo manca ancora: **con lo stato attuale nessuno dei due programmi funziona.**

## Firmare un file o un messaggio

**Stato:** nucleo di calcolo scritto e verificato sul computer. Con un vero programma per smart card (GnuPG, OpenSC) mai provato.

La chiave si comporta come una **smart card.** Per questo Windows, Linux e macOS non hanno bisogno di nessun driver.

- Può presentarsi come **scheda PIV** e come **scheda OpenPGP.**
- Le chiavi stanno nel chip di sicurezza. Non lo lasciano mai.
- **Ogni firma richiede il pulsante.** Lo schermo indica il servizio.
- Con PIV il PIN deve essere ricontrollato **immediatamente prima di ogni** firma nello slot "9C".
- Sono supportate le curve ellittiche (P-256 e P-384). **RSA finora no** (il nucleo di calcolo lo rifiuta).

Procedura:

1. Chiedi al programma (per esempio GnuPG) di firmare con la chiave presente sulla scheda.
2. Inserisci il PIN, quando richiesto.
3. Sulla chiave leggi il nome, **premi a lungo**, **premi a lungo.**

### Con SSH e Git

**Stato:** Verificato con il dispositivo di prova. `git push` tramite l'agente ha funzionato su Windows e Linux.

1. Avvia l'agente:

```powershell
osk agent
```

2. Mostra le chiavi nella cassaforte:

```powershell
osk keys
```

3. Ogni firma la confermi sulla chiave.

Per i programmi che parlano PKCS#11 esiste una libreria (`osk-pkcs11`, come .dll e .so). Con essa `openssl` ha verificato una firma come "Verified OK", con il dispositivo di prova.

### Dare un segreto a un programma

```powershell
osk run NAME=eintrag -- programm
```

Il segreto va solo al programma avviato. La shell accanto non lo vede.

## Accedere al computer

**Questo non è ancora possibile.**

- Accedere a Windows, Linux o al Mac con la chiave: non costruito.
- Sbloccare il disco e usare la chiave come tastiera (la chiave digita la tua password): non costruito.
- Per queste parti serve una volta il diritto di amministratore per la configurazione. Questo spetta al sistema operativo.

Costruiti e verificati con il dispositivo di prova sono solo: agente ssh, riga di comando, PKCS#11, estensione per il browser e un'interfaccia locale per programmi propri.

## Portafoglio di criptovalute (Bitcoin ed Ethereum)

**Stato:** Verificato sul computer. **Non è mai stato inviato un pagamento vero.**

- Un pagamento la chiave lo firma solo se **destinatario, importo e commissione** erano leggibili sullo schermo.
- Se ne manca uno, la chiave dice **No**, senza interrogare il chip.
- **Non leggibili** sono tra l'altro contratti, importi di token e Typed Data. Cose di questo tipo la chiave le firma solo se sulla chiave hai **attivato** "firmare alla cieca".
- Non viene mai firmato mezzo pagamento. Se una parte non va, fallisce tutto.
- Non supportati: input Taproot.
- La tua frase del portafoglio viene creata in modo da non essere copiata su una seconda chiave e da andare solo a computer accoppiati.

## Programma solo con la chiave (dongle) e server

**Stato:** Solo con il dispositivo di prova. Il lato della chiave non esiste ancora.

- Il **livello 1** (la chiave dice "ci sono") ferma chi **copia** soltanto un programma. Chi sa smontare un programma rimuove il controllo.
- Il **livello 2** (i dati sono chiusi con una chiave del chip) è in linea di principio più forte: senza la chiave i dati non sono leggibili.
- Un server può firmare con la chiave invece che con un file di chiave. **Senza autorizzazione** un server resta fermo, perché nessuno preme alle 3 di notte. L'autorizzazione è **disattivata**, indica le chiavi, scade dopo **al massimo 24 ore** e viene registrata. **Con** l'autorizzazione, un programma dannoso su quel server può firmare.

Altro su questo: [how-it-works.md](how-it-works.md).

## Il registro della chiave

La chiave tiene un proprio registro cifrato. Mostra **quale programma ha preso quale segreto e quando.** Sta nella chiave, non sul computer. Contiene gli ultimi eventi, per un totale di alcune centinaia.

```powershell
osk log
```

Il nome del programma nel registro è un'**affermazione** del programma, non una prova.

I segreti non compaiono mai nel registro.

## Più persone su una chiave

Una chiave ha fino a **quattro profili**, ognuno con il proprio PIN. Il PIN di uno non apre nessun altro. Un profilo bloccato non mostra nulla, nemmeno i nomi delle voci. Spazio: **768 voci per profilo**, **3072 nella chiave.**

## Tipi di voce

Accesso, Codice unico (tempo), Codice unico (contatore), Chiave SSH, Certificato, Chiave PGP, Chiave API, Codici di ripristino, Frase del portafoglio, Chiave di licenza, Carta bancaria, Identità, Wi-Fi, Nota, File.

Ogni voce porta tre segni, che imposti tu:

| Segno | Effetto |
| --- | --- |
| può essere copiata | va sulla tua seconda chiave, e solo lì |
| solo computer accoppiati | viene consegnata solo a un computer accoppiato |
| protetta da scrittura | non si può modificare né cancellare, nemmeno da te, finché non la sblocchi |
