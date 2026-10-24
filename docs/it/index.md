# OpenSecureKey – Manuale

**Questo dispositivo non si può ancora comprare e finora nessuno lo ha usato.**

Tutto in questo manuale descrive ciò che si sta costruendo. Niente di tutto questo è mai stato eseguito su una chiave vera. Nessuna chiave è ancora stata costruita. Dove qualcosa è stato verificato solo sul computer, c'è scritto.

## Che cos'è?

Una chiave di sicurezza da portachiavi. Custodisce i tuoi accessi. Non esce nulla, se non premi tu stesso il pulsante sulla chiave.

In concreto:

- Ti fa entrare nei siti web senza digitare una password (passkey).
- Conserva password, codici, chiavi e certificati.
- Può comportarsi come una smart card.
- Può firmare pagamenti per Bitcoin ed Ethereum.
- Può far funzionare un programma solo finché è inserita (dongle).

La chiave ha un piccolo schermo tutto suo. Lì c'è scritto **che cosa** stai autorizzando. Il computer non può ingannarti su questo.

## Come è fatto questo manuale

| File | A che cosa serve |
| --- | --- |
| [installation.md](installation.md) | Prima configurazione, passo dopo passo. Qui c'è la domanda che non si può più cambiare. |
| [usage.md](usage.md) | Uso di ogni giorno: accedere, prendere una password, un codice, firmare. |
| [settings.md](settings.md) | Che cosa si può impostare. |
| [how-it-works.md](how-it-works.md) | Come lavora la chiave all'interno. E che cosa **non** sa fare. |
| [troubleshooting.md](troubleshooting.md) | Quando qualcosa non funziona. E: **quando la chiave è persa.** |
| [development.md](development.md) | Per chi costruisce il progetto da sé. |

## L'essenziale in anticipo

- **Una domanda durante la configurazione non si può più cambiare:** in futuro deve poter esistere una chiave di riserva? Leggila con calma. Vedi [installation.md](installation.md).
- **Senza una seconda chiave e senza una copia di sicurezza, una chiave persa è una perdita totale.**
- **Chi perde la frase segreta principale e tutte le chiavi perde tutto.** Non esiste una porta di servizio. Vedi [troubleshooting.md](troubleshooting.md).
- Non c'è **nessuna verifica dall'esterno** e **nessuna prova su hardware vero.** Per ora non fidarti di niente che non puoi sostituire.

## Che cosa la chiave non sa fare

Spiegato in dettaglio in [how-it-works.md](how-it-works.md). In breve:

- Nella prima fase di sviluppo **non ha un sensore di impronte digitali.** E per nessuno dei sensori esaminati è dimostrato che riconosca una riproduzione di impronta.
- **Non** ti protegge se qualcuno ti costringe a premere il pulsante.
- **Non** protegge da un firmware modificato.
- **Non** ti salva se la frase segreta principale e tutte le chiavi sono perse.

## Stato delle parti

| Parte | Stato |
| --- | --- |
| Dispositivo (scheda, custodia) | non ancora costruito |
| Firmware | scritto, provato sul computer con dispositivi di prova, mai sulla chiave |
| Programma `osk` sul computer | verificato con un dispositivo di prova, mai con una chiave vera |
| Windows e Linux | compilato e provato con il dispositivo di prova |
| macOS | mai provato |
| Browser e siti web | mai provati |

Tecnica più approfondita (in inglese, per sviluppatori): [`docs/`](../) nella cartella del progetto.
