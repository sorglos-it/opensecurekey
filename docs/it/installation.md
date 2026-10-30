# Configurazione

**Stato:** La chiave non esiste ancora. Questi passi descrivono la procedura prevista. Il programma `osk` finora funziona solo con un dispositivo di prova. Una procedura guidata che accompagni i passi da 3 a 5 non esiste ancora: i testi ci sono, il programma no.

## Che cosa ti serve

- la chiave
- un computer con Windows o Linux (macOS: mai provato)
- un browser
- tempo per leggere con calma la domanda del passo 3

Per il programma `osk` **non serve nessuna installazione** e **nessun diritto di amministratore.**

## Passo 1: Procurarsi il programma

- `osk` è **un solo file** per ogni sistema operativo.
- Salva le sue impostazioni **accanto a sé**, non nel sistema.
- Un programma pronto da scaricare non esiste ancora. Non è stata costruita nessuna versione ufficiale. Chi vuole costruirlo da sé legge [development.md](development.md).

**È previsto** che la chiave porti con sé i programmi: all’inserimento mostra una piccola unità di sola lettura.

**Allora vale questo: «inserire e avviare» vuol dire aspettare più di un minuto.** La chiave dà i suoi dati piano. È normale e non è un guasto. Quanto esattamente e perché sta in [how-it-works.md](how-it-works.md), sezione «L’unità con i programmi si prende il suo tempo».

## Passo 2: Inserire la chiave, avviare il programma

1. Inserisci la chiave in una porta USB.
2. Avvia `osk`.
3. Il programma scrive un indirizzo, per esempio `http://127.0.0.1:8101/`.
4. Apri questo indirizzo nel browser.
5. Sotto **Dispositivi** deve comparire la tua chiave. Se lì c'è scritto "Nessun dispositivo trovato.", ti aiuta [troubleshooting.md](troubleshooting.md).

L'indirizzo porta solo al tuo computer. Dall'esterno nessuno raggiunge la pagina.

## Passo 3: La domanda

L'interfaccia ti pone prima questa domanda:

> **Deve restare possibile una chiave di riserva più avanti?**

**Questa risposta non si può più cambiare.** Non è un avviso di prudenza, è tecnica: un segreto nato nel chip di sicurezza non ne esce mai più. Quindi non si può copiare più tardi su una seconda chiave.

Ci sono due strade. Entrambe hanno un prezzo.

### Strada A: "Sì, riserva possibile"

- **Che cosa ottieni:** Potrai creare più avanti una seconda chiave che fa la stessa cosa. Se ne perdi una, usi l'altra e rientri ovunque.
- **Che cosa paghi:** Il tuo segreto principale viene creato durante la configurazione **una volta fuori dal chip di sicurezza.** In quel solo momento un programma dannoso sul tuo computer potrebbe leggerlo. Mai più in seguito.

### Strada B: "No, solo questa chiave"

- **Che cosa ottieni:** Tutto resta per sempre nel chip di sicurezza. Nessun segreto lo lascia, in nessun momento.
- **Che cosa paghi:** **Non esiste una riserva.** Se la chiave si perde o si rompe, non puoi più accedere agli account creati con essa. Su ogni servizio dovresti ricominciare da capo.

### Che cosa consiglia il programma

- **Per l'uso quotidiano:** riserva possibile. Chi perde la sua unica chiave, altrimenti, perde tutti gli accessi.
- **Per un portafoglio di criptovalute:** per questo potrai creare più avanti un **secondo profilo senza riserva.**

Nessuna delle due strade è in assoluto la più sicura. Dipende da che cosa temi di più: quel singolo momento sul computer, o la perdita dell'unica chiave.

### Cambiare idea

- Si può fare solo così: **creare un nuovo profilo** e **registrarti di nuovo su ogni servizio.**
- Una chiave contiene fino a **quattro profili.** Ognuno ha un proprio PIN.
- **Deve** essere così: l’accesso con le passkey non conosce profili. Anche al semplice passaggio da un profilo all’altro ti registri di nuovo presso i servizi. Vedi [usage.md](usage.md), sezione «Più persone su una chiave».

Prima di proseguire devi spuntare: "Ho capito che questo non potrà più essere cambiato".

## Passo 4: PIN

- Senza PIN non funziona niente. Chi trova la chiave non arriva a nulla senza il PIN.
- Il PIN ha almeno **4 caratteri.** Le lettere con dieresi contano ognuna come un carattere.
- Quanti tentativi sbagliati ci sono in tutto prima che il PIN si blocchi per sempre, **non è ancora stabilito.** Tre errori di fila bloccano per ora solo fino a quando scolleghi la chiave. Altro in [troubleshooting.md](troubleshooting.md).
- I tentativi sbagliati li conta il **chip di sicurezza.** Nessuno può azzerare il contatore cancellando la memoria.
- **Aperto:** Dove esattamente imposti il PIN la prima volta non è ancora descritto.

## Passo 5: Mettere al sicuro, prima di usare la chiave

Questo è il passo più importante. Fallo **subito**, non "più tardi".

1. **Creare la seconda chiave** (solo con la strada A). Vedi sotto.
2. **Salvare la copia di sicurezza.** Vedi sotto.
3. **Scrivere il foglio di emergenza.** Vedi sotto.

**Perché questo non può aspettare «dopo»:** chi dimentica la frase segreta principale e perde entrambe le chiavi **ha perso tutto.** Non «quasi tutto» e non «recuperabile con fatica». Non esiste una porta di servizio, non esiste una procedura di riserva e non c’è nessuno che possa aiutare. Per questo il foglio di emergenza nella procedura guidata **non si può saltare:** nessun «salta», nessun «ricordamelo dopo». O una seconda chiave è accoppiata, o il foglio è scritto.

**Stato:** questo obbligo è previsto e **non è costruito.** Finché la procedura guidata non esiste, nessuno ti obbliga. Fai entrambe le cose comunque, e fallo adesso.

### Accoppiare la seconda chiave

1. Collega entrambe le chiavi.
2. Nell'interfaccia scegli **Accoppia un dispositivo di riserva.**
3. **Entrambe le chiavi mostrano lo stesso numero di sei cifre.** Confrontalo.
4. Se i numeri sono **uguali:** premi su entrambe le chiavi.
5. Se sono **diversi:** non premere. Allora c'è qualcuno in mezzo. In quel caso non viene trasferito nulla.

Che cosa viene trasferito:

- Solo le voci con il segno **"può essere copiata".**

Che cosa **non** viene trasferito:

- tutto il resto
- una frase del portafoglio di criptovalute: viene creata in modo da non essere copiata su una seconda chiave

L'accoppiamento avviene solo via USB al computer. Entrambe le chiavi sono inserite nello stesso momento. Non esiste un trasferimento via radio da chiave a chiave, perché la chiave non ha batteria.

Il computer a cui sono inserite entrambe le chiavi vede solo dati cifrati.

**Lacuna nota:** La parte pubblica delle chiavi SSH, dei certificati e delle chiavi PGP per ora **non** passa alla seconda chiave e non è nella copia di sicurezza. La chiave funziona lì, ma un programma può elencarla solo dopo che la parte pubblica è stata salvata di nuovo.

### Salvare la copia di sicurezza

1. Nell'interfaccia apri **Backup** e scegli **Salva un backup.**
2. Scegli una **frase segreta principale.** Ha almeno **dodici caratteri.** Non cifre, ma caratteri.
3. Accetta la proposta della chiave: **otto parole.** È più difficile da indovinare di quasi tutto ciò che si inventano le persone, ed è più facile da scrivere su carta.
4. Il file è cifrato. Il computer non può leggerlo.
5. Metti il file su un disco o su un NAS.

**Importante:**

- Conserva **file e frase segreta principale separati.** Insieme aprono tutto.
- Con il file e la frase segreta principale si può configurare una **chiave completamente nuova.** Il file non dipende dalla chiave che l'ha scritto.
- Il programma avverte al momento del caricamento: "Il caricamento sostituisce il contenuto del dispositivo." Quindi non caricare per sbaglio.

### Foglio di emergenza

Un foglio di emergenza è un foglio di carta che conservi **in un posto sicuro.** Contiene la tua **frase segreta principale.**

Se vuoi che anche altri possano accedere in caso di emergenza, si può fare con le **buste:**

- La chiave divide la chiave della copia di sicurezza in più parti, per esempio cinque.
- Ogni parte è **una riga di testo.** Stampale, una busta chiusa per ogni persona.
- **Tre su cinque** insieme aprono la copia di sicurezza, senza la frase segreta principale.
- **Una singola busta non rivela nulla.** Nemmeno una lettera.
- Una parte copiata male si nota. Non esce un risultato sbagliato.

Il numero delle parti e quante ne servono lo decidi tu.

**Aperto:** Un "foglio di emergenza" già stampato e generato dal programma non è descritto come funzione. Scrivi tu stesso la tua frase segreta principale e il luogo in cui si trova la copia di sicurezza.

## Aggiornamento

### Firmware (il software nella chiave)

1. Nell'interfaccia apri **Firmware** e scegli **Scrivi il firmware.**
2. **Non scollegare la chiave durante la scrittura.**
3. La chiave accetta **solo immagini firmate.**

La tua cassaforte resta intatta. Una versione più vecchia della cassaforte viene letta e riscritta quando la usi. Una cassaforte di un firmware **più recente** non viene nemmeno aperta, invece di essere letta in modo sbagliato.

**Stato:** La scrittura su una chiave non è mai stata provata. Lo strumento per farlo (`tools/flash.py`) non è verificato.

### Il programma `osk`

Non esiste ancora una funzione di aggiornamento. Sostituisci il file con uno più recente.

## Rimozione

- **Programma:** cancella il file `osk`. Le impostazioni sono accanto, non nel sistema.
- **Svuotare la chiave:** Vedi [troubleshooting.md](troubleshooting.md), sezione "Reimpostare". **È definitivo.**
