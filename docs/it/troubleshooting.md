# Quando qualcosa non funziona

**Stato:** Niente di tutto questo è stato vissuto su una chiave vera. I messaggi provengono dal programma. I consigli generali sono indicati come tali.

## La chiave è persa

Questa è la sezione che conta. Leggila **adesso**, non solo quando è già successo.

### Prima: Che cosa può fare chi trova la chiave?

- **Niente, senza il PIN.**
- I tentativi sbagliati li conta il chip di sicurezza. Nessuna cancellazione azzera il contatore.
- Anche con il PIN, ogni autorizzazione richiede il pulsante sulla chiave.
- **Non esiste un blocco a distanza.** Nella documentazione tecnica non se ne parla.
- Consiglio generale, non della chiave: se hai dubbi, cambia le password degli account più importanti.

### Poi: Che cosa hai? Cerca il tuo caso.

#### Caso 1: Hai una seconda chiave (strada "riserva possibile")

1. Prendi la seconda chiave.
2. Sa fare le stesse cose di quella persa, **per tutto ciò che era contrassegnato come "può essere copiata".**
3. La tua frase del portafoglio non era tra queste. Non va mai su una seconda chiave.
4. Crea il prima possibile una **nuova** seconda chiave. Altrimenti hai di nuovo una sola chiave.
5. La parte pubblica delle chiavi SSH, dei certificati e PGP per ora **non** è sulla seconda chiave. Salvala di nuovo, altrimenti un programma non può elencare la chiave.

#### Caso 2: Hai una copia di sicurezza e la frase segreta principale

1. Procurati una chiave nuova.
2. Configurala. Vedi [installation.md](installation.md).
3. Carica la copia di sicurezza. Per questo servono il file **e** la frase segreta principale.
4. Se non hai più la frase segreta principale ma hai le buste: **tre su cinque** (o il numero che hai scelto) aprono il file senza la frase segreta principale.
5. Attenzione: il programma avverte che il caricamento sostituisce il contenuto del dispositivo.

**Aperto:** Se una chiave nuova, partendo dalla copia di sicurezza, possa usare di nuovo anche le tue **passkey** non è stabilito nella documentazione tecnica. Per le passkey affidati alla seconda chiave.

#### Caso 3: Non hai niente di tutto questo, e hai scelto "solo questa chiave"

- Per gli accessi creati con essa **non esiste una riserva.**
- Su ogni servizio devi ricominciare da capo, seguendo la strada che il servizio offre per gli accessi persi.
- La chiave stessa non può aiutarti in questo.

#### Caso 4: Tutto perso: entrambe le chiavi e la frase segreta principale

- **Chi perde la frase segreta principale e tutte le chiavi perde tutto.**
- Nessuno può recuperare il contenuto. Nemmeno il progetto. Non esiste una porta di servizio che si possa rubare.

### Come ti prepari

Queste quattro cose, meglio subito durante la configurazione:

1. **Accoppiare la seconda chiave** (solo con la strada A).
2. **Salvare la copia di sicurezza**, conservandola separata dalla frase segreta principale.
3. **Foglio di emergenza:** la frase segreta principale su carta, in un posto sicuro. Annota anche dove si trova la copia di sicurezza.
4. **Buste** per persone di cui ti fidi: una parte per busta, tre su cinque aprono.

Non conservare **mai** tutto nello stesso posto. File e frase segreta principale insieme aprono tutto.

L'ordine della tua sicurezza:

- Seconda chiave + copia di sicurezza + foglio: un guasto non è un dramma.
- Solo la seconda chiave: un incendio o il furto della borsa le porta via entrambe.
- Niente di tutto questo: una chiave persa è una perdita totale.

## La chiave non viene riconosciuta

Nel programma, sotto **Dispositivi**, c'è scritto "Nessun dispositivo trovato."

Consigli generali (non provati sulla chiave):

1. Prova un'altra porta USB.
2. Un altro cavo, oppure direttamente sul computer, senza hub.
3. Scollega la chiave, ricollegala, riavvia il programma.
4. La chiave non ha batteria e prende tutta la corrente dalla porta USB. Se lo schermo resta buio, non riceve corrente: prova un'altra porta (punto 1).

Che cos'altro conta:

- **macOS:** Senza la modalità di compilazione particolare (cgo) il programma parte, ma **non trova nessuna chiave vera** e lo dice.
- Se vuoi usare la chiave di prova, inserisci il suo indirizzo nelle impostazioni. Vedi [settings.md](settings.md).
- L'identificativo della chiave (numero del produttore e del prodotto) è **provvisorio** e non ancora concordato con il firmware.

Messaggi relativi:

| Messaggio | Significato | Che cosa fare |
| --- | --- | --- |
| Nessun dispositivo trovato. | Nessuna chiave risponde. | Vedi sopra. |
| I dispositivi di questo computer non possono essere elencati. | Il sistema rifiuta l'elenco. | Riavvia il programma. Se non aiuta, segnalalo con il testo tra parentesi. |
| Il dispositivo non può essere aperto. | Causa possibile: un altro programma sta usando la chiave, oppure il sistema non lo permette. | Chiudi gli altri programmi che usano la chiave. |
| Non si può leggere dal dispositivo. / Non si può scrivere sul dispositivo. | Il collegamento si è interrotto. | Ricollega la chiave. |
| Il dispositivo non ha risposto in tempo. / Il dispositivo ha impiegato troppo tempo. | Forse la chiave aspetta la tua pressione. | Guarda lo schermo della chiave. |
| Il collegamento al dispositivo è disturbato. | Come sopra. | Ricollega la chiave. |
| Questo sistema operativo non è ancora supportato. | Per questo sistema non esiste ancora una via. | Usa Windows o Linux. |
| Il dispositivo usa un'altra versione del protocollo. | Programma e firmware non sono compatibili. | Aggiorna il programma o il firmware. |

## Il PIN è bloccato

- **Tre PIN sbagliati di fila** bloccano la chiave finché non la **scolleghi e la ricolleghi.** Poi puoi riprovare.
- Il contatore è nel chip di sicurezza. Riscrivere o cancellare la memoria **non** lo azzera.
- **Quanti tentativi sbagliati ci siano in tutto** prima che il PIN si blocchi per sempre, e che cosa succeda allora, **non** è nella documentazione tecnica. Perciò non tirare a indovinare. Prima cerca il PIN.
- Per il lato smart card (PIV, OpenPGP) ci sono inoltre un PUK e un PIN amministratore. Stanno nella cassaforte, ciascuno con un proprio contatore.

| Messaggio | Significato | Che cosa fare |
| --- | --- | --- |
| Il dispositivo è bloccato. | Non hai ancora sbloccato. | **Sblocca.** Il consenso lo chiede la chiave sul proprio schermo. |
| Manca il consenso sul dispositivo. | Non hai confermato (o hai confermato con "No"), oppure ci sono voluti più di 30 secondi. | Riprova. Premi a lungo, poi premi a lungo. |

### Reimpostare

In caso di emergenza si può reimpostare la parte di accesso (FIDO).

- **È definitivo.** Il segreto principale diventa nuovo, **ogni vecchio accesso è senza valore.** Devi registrarti di nuovo su ogni servizio.
- Reimpostare **non** funziona tramite NFC.
- Cancellare sulla chiave chiede due volte.
- Se questo risolva anche un PIN bloccato non è descritto nella documentazione tecnica.

## Il browser non accetta la chiave

**Stato:** Mai provato in un browser. Questo elenco conosce solo i **limiti noti.**

- **I vecchi siti che sanno solo U2F** forse non accettano la chiave. Non invia nessun certificato di attestazione.
- **I siti aziendali con certificato obbligatorio** forse la rifiutano. Si attesta da sola.
- La chiave si presenta come **FIDO 2.0 e 2.1.** Comandi per gestire le passkey (elencare, cancellare) e per registrare impronte digitali nel browser **non esistono.**
- La chiave dichiara **nessuna verifica utente integrata** (`uv`), finché non c'è un sensore. È giusto così. Il PIN viene dichiarato a parte.
- Tramite **Bluetooth** **non** c'è accesso nel browser. I browser non supportano FIDO via Bluetooth in modo affidabile. Usa USB o NFC.
- NFC e Bluetooth funzionano solo finché la chiave è inserita in una porta USB. Tenuta libera accanto al telefono, non funziona nulla.
- Il **contatore di firme** riparte da zero a ogni accensione. Un sito che insiste su contatori crescenti potrebbe scambiare la chiave per una copia.
- L'**identificativo del produttore** (AAGUID) è provvisorio.

Messaggi dell'estensione del browser:

| Messaggio | Significato | Che cosa fare |
| --- | --- | --- |
| Questa voce appartiene a un altro indirizzo. | La voce è salvata per un altro sito. | **È la protezione dal phishing.** Controlla l'indirizzo. Se è giusto, cambia l'indirizzo nella voce. |
| Questa pagina non e cifrata. Qui non va nessun segreto. | Il sito non usa https. | Non compilare. |
| Il canale verso il browser si e interrotto. | L'estensione ha perso il collegamento. | Riavvia il browser. |
| Il file di registrazione non si e potuto scrivere. | L'estensione non è riuscita a registrarsi. | Controlla i diritti nella cartella. |
| Questo non e un identificativo di estensione. / Non conosco questo browser. | Dato sbagliato durante la registrazione. | Controlla l'identificativo e il nome del browser. |

## Messaggi su voci e copia di sicurezza

| Messaggio | Significato | Che cosa fare |
| --- | --- | --- |
| Esiste già una voce con questo nome. | Nome doppio. | Scegli un altro nome. |
| Sul dispositivo non c'è più spazio. | La memoria del profilo è piena (768 voci). | Cancella le vecchie voci. |
| Il dispositivo non conosce questa voce. | Voce cancellata o nome sbagliato. | Ricarica l'elenco. |
| Il nome corrisponde a più voci. | Non univoco. | Indica il nome con più precisione. |
| Questo dispositivo non può ancora farlo. | La funzione manca in questo firmware. | Aggiorna il firmware, se ne esiste uno più recente. |
| Il file di backup è vuoto o troppo grande. | File sbagliato. | Scegli un altro file. |
| I dati sul dispositivo non hanno la forma prevista. | I dati sono danneggiati, oppure provengono da un firmware **più recente.** Una versione più recente non viene mai letta in modo sbagliato, ma non viene proprio aperta. | Aggiorna il firmware. Se non aiuta, carica la copia di sicurezza. |
| L'immagine del firmware è vuota. | Nessun file scelto. | Scegli un file. |
| Il file delle impostazioni non può essere letto. / è troppo grande. / contiene un errore. | Il file `config_osk.json` è danneggiato. | Cancellalo. Il programma userà allora i valori predefiniti. |

## Messaggi dell'agente ssh e delle interfacce

| Messaggio | Significato | Che cosa fare |
| --- | --- | --- |
| Questo posto e gia occupato. | Un altro agente o servizio è già in funzione. | Chiudi l'altro. |
| Questa chiave e cifrata con una password. | La chiave SSH nella cassaforte è ancora protetta. | Mettila nella cassaforte decifrata. |
| Questa chiave e troppo corta e non e piu sicura. | Chiave debole. | Crea una chiave nuova. |
| Non sono in grado di gestire questo tipo di chiave. | Non supportato. | Scegli un altro tipo. |
| Questa chiave e stata richiesta, ma non e nella cassaforte. | Il programma chiede una chiave che non hai. | Metti la chiave nella cassaforte. |
| L'accesso non e stato autorizzato. / Non e arrivata risposta alla domanda sull'autorizzazione. | Hai rifiutato o non hai premuto nulla. | Riprova. |

I numeri di uscita per gli script li mostra:

```powershell
osk codes
```

## Messaggi della protezione dongle

| Messaggio | Significato | Che cosa fare |
| --- | --- | --- |
| Nessuna chiave di sicurezza trovata. Inseriscila. | La chiave non è inserita. | Inseriscila. |
| È inserita una chiave diversa da quella configurata. | Il programma appartiene a un'altra chiave. | Inserisci la chiave giusta. |
| Il file non si apre: chiave errata o file modificato. | La chiave non corrisponde al file sigillato, oppure il file è stato modificato. | Usa la chiave che lo ha sigillato. |
| La licenza è scaduta. | La licenza era autentica ed è terminata. | Procurati una nuova licenza. |
| Su questa chiave di sicurezza non c’è una licenza per il prodotto. | Nessuna licenza per il prodotto. | Metti la licenza sulla chiave. |
| Questo chip non può fare lo scambio di chiavi. | Il chip è stato impostato così in fabbrica. La modalità server con la chiave nel chip viene meno. La firma continua a funzionare. | Non si può cambiare. |

## Questo non è ancora possibile

Così non cerchi a lungo:

- **KeePassXC e VeraCrypt** con la chiave
- **Accedere al computer,** sbloccare il disco, la chiave come tastiera
- un foglio di emergenza già stampato dal programma
- **macOS**

## Se non lo trovi

Segnalalo nel progetto su GitHub (sorglos-it/opensecurekey) e indica: sistema operativo, versione di `osk`, il messaggio esatto e che cosa avevi fatto prima. Password e PIN non vanno mai in una segnalazione.
