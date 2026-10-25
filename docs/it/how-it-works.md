# Come lavora la chiave all'interno

**Stato:** Tutto qui è un progetto, verificato sul computer. Niente è girato su una chiave vera.

## Due chip, due compiti

- **Il chip radio (nRF52840)** parla con il computer e con il telefono.
- **Il chip di sicurezza (SE051)** custodisce i segreti. Non cede nessuna chiave.

Perché due? La protezione del chip radio in passato è già stata aggirata con picchi di tensione. Un chip radio rotto, quindi, da solo non rivela ancora nulla.

Ci sono inoltre:

- **Memoria (64 MB):** la cassaforte, cifrata, e il registro.
- **Schermo:** mostra che cosa autorizzi.
- **Un pulsante:** è la tua autorizzazione.
- **Nessuna batteria:** la chiave funziona solo con la corrente USB. Il computer imposta l'orologio per i codici unici quando la inserisci.

## Vie verso il computer

- USB come dispositivo di accesso (FIDO2)
- USB come lettore di smart card (PIV, OpenPGP)
- USB come tastiera (**previsto, non ancora costruito**)
- canale USB per il programma di gestione
- unità USB, di sola lettura, con i programmi sopra (**previsto**)
- NFC (solo finché l'USB alimenta la chiave)
- Bluetooth, solo per il programma di gestione (solo finché l'USB alimenta la chiave)

## Perché nessuna batteria?

La chiave non ha una batteria, per scelta. Funziona solo finché è inserita in una porta USB.

- L'orologio per i codici unici basati sul tempo non ha bisogno di alimentazione continua. Lo imposta il computer quando inserisci la chiave, e il codice nasce solo nel momento in cui lo usi. **Bozza, non ancora costruito.**
- Per il timestamp di una firma la chiave chiede l'ora a un server di timestamp. Non usa il proprio orologio.
- Una batteria avrebbe dato solo NFC e Bluetooth senza computer. Contro questo c'erano 4–5 mm di lunghezza in più e un componente soggetto a usura con pochi anni di vita, in una chiave che deve durare decenni.

**A che cosa rinunci:**

- Il chip radio non può prendere corrente dal campo NFC. Perciò NFC e Bluetooth esistono solo finché la chiave è inserita in un computer.
- Tenere la chiave libera accanto al telefono non funziona più.
- Il Bluetooth verso la tua app funziona solo finché la chiave è inserita in un computer. Con la chiave sul portachiavi non funziona più.
- Una seconda chiave si accoppia solo via USB al computer. Per questo entrambe le chiavi sono inserite nello stesso computer.

È una scelta fatta di proposito. La tecnica è in `docs/architecture.md`.

## Le due strade della configurazione

Questa è la tecnica dietro l'unica domanda di [installation.md](installation.md).

### "Solo questa chiave"

- Per ogni accesso nasce nel chip di sicurezza **una coppia di chiavi propria.** La parte privata **non esiste da nessun'altra parte.**
- Non c'è nulla da rubare e nulla da estrarre.
- Il prezzo: ogni accesso occupa spazio nel chip. Il numero di servizi è **limitato.** Quanti siano dipende dalla versione del chip fornita (46 o 104 kB) e **non è misurato.** Il programma dovrebbe avvisare in tempo.
- Per questi accessi non esiste un secondo dispositivo.

### "Riserva possibile" (copiata)

- Un segreto principale viene creato durante l'accoppiamento **fuori da entrambi i chip** e scritto in tutti e due.
- Il prezzo: per un breve momento sta nella memoria del chip radio.
- Dopo la scrittura il firmware può **solo usarlo**, non leggerlo, non sovrascriverlo, non estrarlo. Non è disciplina, è il chip che lo stabilisce così.

### Perché non si può cambiare?

Un segreto nato nel chip non lo lascia mai, nemmeno cifrato. Quindi non si può mai copiare in seguito. Un segreto principale copiato deve nascere **prima** della sua creazione nel posto giusto.

Nessuna delle due strade è in assoluto più sicura. Un'unica chiave persa distrugge più di quanto di solito faccia una chiave rubata.

## Il PIN

- La chiave non riceve mai il PIN in chiaro, solo un valore di controllo ricavato da esso.
- Lo controlla il **chip di sicurezza** e conta i tentativi sbagliati. Nessuna cancellazione della memoria azzera il contatore.
- Tre PIN sbagliati di fila bloccano finché non scolleghi la chiave.
- PIV e OpenPGP usano **lo stesso** PIN. I PIN di gestione (PUK, PIN amministratore) stanno nella cassaforte, ciascuno con un proprio contatore.
- Ogni tentativo con un PIN di gestione chiede conferma sullo schermo. È giusto così.

## La cassaforte

- Ogni segreto è cifrato singolarmente (AES-256-GCM).
- **Mancanza di corrente:** Ogni modifica viene scritta in un ordine fisso. Dopo ogni interruzione c'è lo stato vecchio **oppure** quello nuovo, e tutto è leggibile. Questo è stato provato in ogni singolo punto di scrittura.
- La chiave distribuisce la scrittura su tutta la memoria. Così non si consuma in un punto solo.
- **Cercare senza aprire:** Alcune voci portano un breve contrassegno. Dice a quale servizio appartiene la voce. Così la chiave trova una passkey in fretta. Il contrassegno è a sua volta cifrato.
- La ricerca deve scorrere tutte le parti di un elenco. **Quanto duri sul chip vero non è misurato.**
- Un aggiornamento del firmware non perde dati.

## La copia di sicurezza

- Un file cifrato da cui si può configurare una chiave nuova.
- La sua chiave deriva dalla **frase segreta principale**, non dal chip. Altrimenti la copia di sicurezza dipenderebbe dalla chiave andata persa.
- Sulla chiave i valori di calcolo sono tenuti piccoli. La forza deve quindi venire dalla frase segreta principale. Per questo otto parole.
- Le buste dividono la chiave del file (Shamir).

## Una seconda chiave

- Entrambe le chiavi generano una chiave di breve durata e si scambiano solo la parte pubblica.
- Ne nasce il numero di sei cifre. Chi si trova in mezzo genera **due numeri diversi.** Questo è stato provato.
- Senza il numero confermato non esce nemmeno un byte dalla chiave.

## Che cosa protegge la chiave, e da chi

| Chi | Che cosa ottiene |
| --- | --- |
| trova la chiave | niente senza il PIN |
| apre la chiave ed esamina la scheda | La linea tra i chip è cifrata, **non appena le chiavi di fabbrica sono state sostituite** (vedi sotto). La memoria è cifrata. |
| ha un programma dannoso sul computer | Ogni autorizzazione richiede il pulsante, e lo schermo indica il servizio. Il programma non può premere. |
| gestisce un sito web contraffatto | Le passkey sono legate all'indirizzo vero. L'estensione compila solo sull'indirizzo salvato. |
| inoltra l'NFC tra telefono e chiave | richiede comunque la pressione sulla chiave |
| copia un server | La chiave del server resta nel chip. **Solo se il chip fornito lo sa fare.** |

## Che cosa la chiave non sa fare

Qui è scritto senza abbellimenti.

### Non protegge da

- **Qualcuno che ti costringe a premere il pulsante.** Nessun dispositivo può riconoscerlo.
- **Un firmware modificato.** Il chip di sicurezza protegge dagli attacchi al chip, non da un firmware che ti mente. Contro questo aiuta solo l'avvio firmato. Deve esserci **prima** che venga attivata la protezione in lettura.
- **Qualcuno che legge il PIN.** Lo digiti sul computer, quindi un programma dannoso può leggerlo. Usarlo non può, perché ogni autorizzazione richiede il pulsante.
- **La perdita di tutto.** Vedi sotto.

### Impronta digitale

- La prima fase di sviluppo **non ha un sensore di impronte digitali.** Autorizzi con PIN e pulsante.
- Il posto per un sensore è previsto sulla scheda.
- Con FIDO2 un'impronta digitale **sostituisce** il PIN. Non è una protezione in più.
- **Un rilevamento di vitalità non è dimostrato per nessun sensore esaminato.** Chi teme un dito riprodotto non guadagna nulla da nessuno di questi sensori.
- I dati delle impronte non sarebbero mai nella cassaforte e non potrebbero mai andare su una seconda chiave.
- Registrare un dito non lo può fare nessun browser e nessun sito web. Succederebbe sulla chiave stessa.

### Perdita

**Chi perde la frase segreta principale e tutte le chiavi perde tutto.**

Se perdi insieme la chiave, la seconda chiave, il file della copia di sicurezza e le buste, il contenuto è perso. Nessuno può aiutare. Non esiste una porta di servizio che si possa rubare.

### Altro che è bene sapere

- **Nessuna prova dall'esterno.** Nessuno al di fuori del progetto ha esaminato la cifratura.
- **Nessun test di conformità FIDO** è stato eseguito.
- **Il chip arriva con chiavi di fabbrica note pubblicamente.** Alla prima messa in servizio **devono** essere sostituite. Fino ad allora la linea tra i chip non è protetta. È descritto e previsto, ma **mai eseguito su un chip.**
- **Chi può riscrivere direttamente la memoria** non può leggere nulla. Ma può rimettere uno stato più vecchio. Contro questo manca ancora un contatore nel chip.
- Il **contatore di firme** dell'accesso riparte da zero a ogni accensione. Alcuni siti potrebbero scambiarlo per una chiave copiata.
- La chiave invia un'**auto-attestazione**, non un certificato del produttore. Va bene per la privacy, ma alcuni siti aziendali richiedono di più.
- Se il chip fornito permetta lo **scambio di chiavi** (ECDH) dipende dalla sua impostazione di fabbrica. Se non lo permette, **la modalità server con la chiave nel chip viene meno.** La firma continua a funzionare. Il programma lo dice allora onestamente, invece di sembrare un errore di PIN.
- **Mac:** mai provato.
- Lo **spazio nel firmware** è scarso. Se tutte le funzioni ci stiano è aperto.

## Programma e chiave parlano tra loro

- Il programma `osk` parla con la chiave tramite un canale USB dedicato.
- Ogni risposta della chiave viene trattata come ostile: formati rigorosi, limiti severi per dimensione e profondità, e il numero di sequenza deve corrispondere.
- L'interfaccia locale resta in ascolto solo sul tuo computer. Ogni richiesta deve venire dal proprio indirizzo. Una pagina estranea nello stesso browser non può arrivarci.
- Ogni richiesta indica il nome del programma. La chiave lo scrive nel registro. Resta un nome, non una prova.
