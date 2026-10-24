# Costruire da sé e sviluppare

Questo capitolo è per chi costruisce il progetto da sé. La tecnica dettagliata è in inglese nella cartella [`docs/`](../) (per esempio `architecture.md`, `threat-model.md`, `vault.md`, `build.md`).

**Stato:** Una chiave ancora non esiste. Si costruisce e si prova sul computer, con dispositivi di prova per chip, memoria e schermo.

## Cartelle

| Cartella | Contenuto | Linguaggio |
| --- | --- | --- |
| `apps/firmware` | Software nella chiave (nRF52840 + SE051) | C / Zephyr |
| `apps/desktop` | Programma di gestione `osk` con interfaccia locale | Go |
| `apps/web` | Estensione per il browser | JavaScript |
| `shared/` | Protocollo, formato della cassaforte, libreria `osk-guard` | |
| `hardware/` | Schema elettrico, assegnazione dei pin, note sul layout | |
| `cad/` | Custodia | |
| `tools/` | Strumenti di costruzione e di verifica | Python |

## Che cosa ti serve

| Parte | Strumento | Senza questo strumento |
| --- | --- | --- |
| tutte | Python 3 | non funziona niente |
| `apps/desktop` | Go 1.24 o più recente | viene saltata |
| `apps/web` | Node.js 20 o più recente (solo per verificare) | La costruzione funziona, la verifica viene saltata |
| `apps/firmware` | nRF Connect SDK (Zephyr, `west`) | viene saltata |

"Saltata" significa sempre: **non costruita, non verificata.**

## Verificare tutto

```powershell
python tools/check.py
```

- Verifica ciò che si può fare senza hardware.
- Alla fine c'è scritto quanti passi sono stati eseguiti, che cosa è stato saltato e quanti test sono stati eseguiti.
- Il codice di uscita è 0 solo se nulla è fallito **e** almeno un test è stato eseguito.

## Costruire

```powershell
python tools/build.py desktop
```

```powershell
python tools/build.py web
```

```powershell
python tools/build.py firmware
```

I risultati sono in `dist/`.

Il programma `osk` si può costruire anche direttamente. Non richiede nessun pacchetto esterno e nessun cgo (tranne che per macOS).

```powershell
go build -C apps\desktop -trimpath -ldflags "-s -w" -o ..\..\dist\osk.exe .
```

## Provare senza la chiave

La chiave di prova (il dispositivo di prova) gira solo in memoria. Non cifra nulla e finge soltanto che tu abbia premuto il pulsante. **Non metterci mai segreti veri.**

```powershell
go run -C apps\desktop .\tests\simulator -port 8201
```

Poi inserisci `127.0.0.1:8201` sotto **Impostazioni → Indirizzo del dispositivo di prova.** Il PIN del dispositivo di prova è `123456`.

I test del programma:

```powershell
go test -C apps\desktop ./...
```

## Quattro livelli di "verificato"

Ogni affermazione su "verificato" indica uno di quattro livelli:

1. scritto
2. compilato
3. nell'immagine
4. girato sulla chiave

Questo progetto è al **livello 2, più prove sul computer con dispositivi di prova.** I livelli 3 e 4 mancano ovunque.

## Decisioni e il loro motivo

| Decisione | Motivo |
| --- | --- |
| Due chip: il chip radio parla, il chip di sicurezza custodisce i segreti | La protezione in lettura del chip radio è già stata aggirata. |
| Due strade di configurazione, scelte una volta sola | Il chip di sicurezza non ricava nessuna chiave al proprio interno. Un segreto nato nel chip non si può mai copiare. |
| Schermo e pulsante a ogni autorizzazione | Il computer non deve decidere che cosa autorizzi. |
| Nessuna autorizzazione senza uno schermo funzionante | Non potresti vedere che cosa autorizzi. |
| Nomi e indirizzi non vengono mai tagliati | Un destinatario tagliato è peggio di nessuno schermo. |
| Livello 1 senza sensore di impronte digitali | Con FIDO2 sostituisce il PIN, non lo integra. Nessun sensore soddisfaceva tutte e quattro le condizioni. |
| Spazio per l'aggiornamento nella memoria esterna | La memoria interna del chip radio è scarsa. |
| Carattere proprio e verificatori propri per il protocollo | Nessuna libreria grafica o di analisi esterna, limiti rigidi, nessuna memoria dinamica. |
| Nessun codice di terzi | Tutto è scritto secondo la specifica. La cifratura viene sempre da librerie verificate. |

## Cinque lingue

- Nessun testo è scritto fisso nel codice.
- Ogni lingua è un file nella cartella `lang/`: `de.json`, `en.json`, `fr.json`, `it.json`, `es.json`.
- Un test segnala come errore le chiavi mancanti.
- Se manca un testo, vale quello inglese.

## Montaggio

**Nuovo:** questa sezione è nuova. Qui arriveranno altre regole di montaggio, appena l'hardware sarà sul tavolo.

- La pellicola di ferrite sotto la batteria è larga **12,0 mm**, **al massimo 12,5 mm.** Non più larga, nemmeno un poco.
- Finisce **a filo del bordo esterno della cella.** Non deve essere nemmeno più stretta: deve coprire la cella.
- Altrimenti la cella si inclina.
- Le misure sono scritte solo in `hardware/layout-notes.md`. Se lì c'è scritto altro, vale quel file.

## Partecipare

Non cambiare mai solo la tecnica. Quando cambiano funzione, uso, impostazioni o struttura, la documentazione ne fa parte: `docs/` e questo manuale.
