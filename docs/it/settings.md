# Impostazioni

**Stato:** Le impostazioni del programma sono verificate con il dispositivo di prova. Le impostazioni nella chiave stessa non esistono ancora su una chiave vera.

Le impostazioni sono poche. È voluto.

## Nel programma `osk`

Nell'interfaccia apri la pagina **Impostazioni.** Il salvataggio avviene nel file `config_osk.json` **accanto al programma**, non nel sistema.

| Impostazione | Significato | Predefinito | Valori ammessi |
| --- | --- | --- | --- |
| Lingua | Lingua dell'interfaccia | `de` | `de`, `en`, `fr`, `it`, `es` |
| Nome di questo programma | Il nome che il programma comunica alla chiave. Finisce nel registro. | `osk` | Testo fino a 64 caratteri |
| Porta | Porta su cui l'interfaccia resta in ascolto | `8101` | Numero da 1024 a 65000 |
| Indirizzo del dispositivo di prova | Solo per provare: indirizzo della chiave di prova | vuoto | per esempio `127.0.0.1:8201`, e solo su questo computer |

Buono a sapersi:

- **Il nome è un'affermazione.** La chiave lo scrive nel registro, ma non dimostra nulla. Solo la chiave stessa può rifiutare qualcosa.
- Se la porta è occupata, il programma cerca una libera a partire dal valore impostato. Se non ne trova, lo dice.
- **Lascia vuoto l'indirizzo del dispositivo di prova** finché usi una chiave vera. Il dispositivo di prova è uno strumento di test. Non metterci mai segreti veri. Il suo PIN è `123456`, e non cifra nulla.
- Se il file delle impostazioni è rotto o troppo grande, il programma parte comunque. Usa i valori predefiniti e dice perché.

Esempio (`config_osk.json`):

```json
{
  "lang": "de",
  "port": 8101,
  "appName": "osk",
  "simAddress": ""
}
```

## Per ogni voce nella chiave

Ogni voce ha tre interruttori. Vedi [usage.md](usage.md), sezione "Tipi di voce".

- può essere copiata
- solo computer accoppiati
- protetta da scrittura

## Nella chiave stessa

Queste cose le imposti **sulla chiave**, non nel programma. **Stato:** non ancora provato da nessuna parte.

| Impostazione | Predefinito | Che cosa cambia |
| --- | --- | --- |
| Profilo e PIN | fino a 4 profili, ciascuno con il proprio PIN | Chi vede che cosa |
| Funzione NFC al tocco (aprire un indirizzo o digitare un codice unico) | si può disattivare | Un telefono, toccando, apre un indirizzo |
| Firmare alla cieca | disattivato | La chiave firma anche cose che non può mostrare in modo leggibile. La domanda sullo schermo allora ha un aspetto visibilmente diverso. |
| Autorizzazione del server | disattivata | Un server può firmare senza pressione, per chiavi indicate, al massimo 24 ore |
| Bluetooth | solo per il programma di gestione | Non per l'accesso nel browser. Accoppiamento solo con confronto dei numeri e pressione sulla chiave. |

Buono a sapersi:

- **Tutto ciò che è rischioso è disattivato** finché non lo attivi di proposito.
- **L'NFC richiede sempre il pulsante.** Anche quando l'NFC dura poco. Questo protegge da un attacco dalla tasca, in cui qualcuno inoltra il collegamento radio.
- L'indirizzo radio del Bluetooth cambia, in modo che non si possa seguirti.
- NFC e Bluetooth funzionano solo finché la chiave è inserita in una porta USB. Non ha batteria.

## Lingua sulla chiave

Lo schermo della chiave conosce le stesse cinque lingue. Se manca un testo, usa l'inglese, mai uno spazio vuoto.

## Cambiare lingua

Cambia la lingua nel programma sotto **Impostazioni.** I file delle lingue stanno nella cartella `lang`, uno per lingua.
