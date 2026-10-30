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
- Una batteria avrebbe dato solo NFC e Bluetooth senza computer. Contro questo c'erano più lunghezza (quanta, è scritto in `hardware/layout-notes.md`) e un componente soggetto a usura con pochi anni di vita, in una chiave che deve durare decenni.

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
- **La chiave privata non esiste come file.** I programmi che pretendono un file di chiave non possono lavorare con la chiave. Vedi [usage.md](usage.md), sezione «Software che pretende un file di chiave».
- **Mac:** mai provato.
- Lo **spazio nel firmware** è scarso. Se tutte le funzioni ci stiano è aperto.

## L’unità con i programmi si prende il suo tempo

**Stato:** calcolato, non misurato sull’apparecchio. L’unità è prevista e non è costruita.

Quando la si inserisce, la chiave mostra una piccola unità di sola lettura con i programmi di amministrazione. Chi la apre aspetta.

**Le tre cifre su questo stanno qui. Tutti gli altri punti del manuale rimandano qui.**

| Che cosa | Cifra |
| --- | ---: |
| Quanto il chip radio fa passare sull’USB | circa **400 kB al secondo** |
| Quanto sono grandi i programmi | **29,10 MB** (misurato il 23.10.2026 sull’immagine finita) |
| Quanto dura dunque la lettura | circa **73 secondi** |

«Inserire e avviare» vuol dire in realtà: inserire, **aspettare più di un minuto,** avviare. Chi non lo sa crede che la chiave sia rotta e la stacca.

Perché così lento? Il chip radio sta su una porta USB del vecchio standard 1.1. Sulla carta ne vengono 1,5 MB al secondo, ma la chiave deve prima tirare ogni pezzo dalla sua memoria e rispondere. Quello che resta sono i 400 kB. I 400 kB sono **misurati** su una scheda di sviluppo; le altre due cifre sono calcolate da quelli.

I programmi crescono. Se cresce il file, cresce l’attesa nella stessa proporzione.

## Il supporto dati cifrato

**Stato: carta.** Non c’è nessun chip, nessuna scheda e nemmeno una riga di programma per questo. Questa sezione sta qui perché nessuno creda poi qualcosa di falso, non perché esista.

È prevista una scheda di memoria sulla chiave, da 8 a 16 GB, per i tuoi file.

**La scheda ha sempre bisogno di un openSecureKey.** In un normale lettore di schede non ci si fa nulla. Porta un formato suo, non un formato di Windows. **Non** è un supporto da mettere in mano a qualcuno che non ha una chiave.

### Tre modi di funzionamento, e uno di essi non è segreto

Questo è il punto in cui è più facile farsi male da sé. Ecco quindi i tre uno accanto all’altro:

| Modo | Nome | Chi può leggere | Contenuto segreto? |
| --- | --- | --- | --- |
| 1 | **solo questa chiave** | solo la chiave che ha scritto | **sì** |
| 2 | **qualunque delle nostre chiavi, con la parola della scheda** | qualunque openSecureKey che abbia la parola della scheda | **sì** |
| 3 | **qualunque delle nostre chiavi, solo sigillato** | **qualunque** openSecureKey, senza password | **no** |

**Il modo 3 non cifra nulla.** Sigilla soltanto: si riconosce se qualcuno ha cambiato il contenuto. Leggerlo può chiunque abbia un openSecureKey qualsiasi.

**Chi crede di aver cifrato e ha solo sigillato regala i suoi segreti.** È il peggiore errore che questo apparecchio possa fare, e non nasce da un attacco ma da una confusione.

Per questo è previsto:

- Il display dice il modo in parole chiare **prima** di scrivere, e la tua pressione conferma esattamente quel testo. Nel modo 3 vi è scritto «**APERTO — solo sigillato**».
- Il modo 3 si presenta in modo chiaramente diverso sul display, non la stessa riga con un’altra parola dentro.
- Finché la scheda è inserita, il modo resta sul display.
- **Cambiare modo vuol dire riscrivere la scheda.** Non esiste un «cifrare dopo».

### L’autenticità non è la paternità

La firma sulla scheda dimostra: **il contenuto è inalterato** da quando è stato firmato.

**Non** dimostra da chi viene.

Che una scheda venga da una persona precisa segue dalla firma **soltanto** se conosci **in anticipo** la chiave pubblica di quella persona: da un incontro, da un biglietto da visita, da un’altra via. Altrimenti sai solo questo: qui ha firmato un openSecureKey qualsiasi.

| Affermazione | coperta dalla firma? |
| --- | --- |
| «Questo contenuto è inalterato.» | **sì** |
| «L’ha firmato un openSecureKey.» | sì |
| «L’ha firmato *questo* apparecchio.» | sì |
| «Questo apparecchio appartiene a una persona precisa.» | **no** |
| «Il contenuto viene da quella persona.» | **no** |

È previsto quindi: all’inserimento il display dice la chiave del mittente come una breve sequenza di caratteri e aggiunge «**mittente sconosciuto**» o il nome che una volta hai dato a quel mittente.

### Si controlla alla lettura, non all’inserimento

Controllare **del tutto** una scheda da 16 GB vuol dire leggerla per intero. Questo dura **circa un’ora e mezza** (calcolato con la velocità della scheda stessa, non con quella della porta USB della sezione sopra: sono due strade diverse). Nessuno aspetta tanto all’inserimento, e perciò la chiave non lo fa.

Al suo posto controlla **ogni blocco nel momento in cui lo legge.**

Il lato buono: nessun file che apri ti arriva non controllato.

Il lato cattivo: **un blocco cambiato in un file che nessuno apre non si nota.**

Per questo la chiave non dice mai «scheda controllata». Sarebbe una bugia. Dice «questo blocco controllato», e questa è la verità.

## Programma e chiave parlano tra loro

- Il programma `osk` parla con la chiave tramite un canale USB dedicato.
- Ogni risposta della chiave viene trattata come ostile: formati rigorosi, limiti severi per dimensione e profondità, e il numero di sequenza deve corrispondere.
- L'interfaccia locale resta in ascolto solo sul tuo computer. Ogni richiesta deve venire dal proprio indirizzo. Una pagina estranea nello stesso browser non può arrivarci.
- Ogni richiesta indica il nome del programma. La chiave lo scrive nel registro. Resta un nome, non una prova.
