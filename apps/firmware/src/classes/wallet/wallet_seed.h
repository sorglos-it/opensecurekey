/* Der Seed im Tresor - und der Weg von dort zu einem abgeleiteten
 * Schluessel.
 *
 * Der Seed liegt als Eintrag vom Typ OSK_ITEM_SEED im Tresor
 * (osk_store.h), nicht im SE050: der SE05x kann Schluessel nicht intern
 * ableiten, das ist in docs/se050.md belegt. Der Tresorschluessel selbst
 * kommt dabei weiter aus dem Chip, der Seed ist also verschluesselt
 * abgelegt und nur bei offenem Tresor erreichbar.
 *
 * Jeder Weg hier gibt dem Aufrufer einen Puffer und erwartet, dass er ihn
 * loescht. Innen wird jeder Zwischenstand geloescht, auch auf dem
 * Fehlerweg. */
#ifndef WALLET_SEED_H
#define WALLET_SEED_H

#include <stdint.h>
#include <stddef.h>
#include "osk_result.h"
#include "osk_store.h"
#include "wallet_bip32.h"
#include "wallet_bip39.h"

/* Seed aus einer Mnemonik erzeugen und in den Tresor legen. Prueft die
 * Pruefsumme der Woerter - eine Mnemonik mit falscher Pruefsumme ist meist
 * ein Abtippfehler, und sie wuerde auf ein leeres Konto zeigen. */
osk_result_t osk_wallet_seed_store(uint8_t profile, const char *name, const char *mnemonic,
				   const char *passphrase, osk_item_id_t *id_out);

/* Alle Seed-Eintraege eines Profils. Oeffnet keinen davon und verlangt
 * keine Freigabe - es werden nur Namen gelesen. */
osk_result_t osk_wallet_seed_list(uint8_t profile, osk_item_id_t *out, size_t cap, size_t *len_out);

/* Einen Schluessel fuer einen Pfad ableiten. Holt den Seed aus dem Tresor
 * (das verlangt eine Freigabe, siehe osk_store_reveal), leitet ab und
 * loescht den Seed sofort wieder aus dem Arbeitsspeicher.
 * `path` ist "m/84'/0'/0'/0/0" und darf hoechstens
 * OSK_BIP32_MAX_DEPTH Schritte haben. */
osk_result_t osk_wallet_seed_derive(osk_item_id_t id, const char *asked_by, osk_bip32_curve_t curve,
				    const char *path, osk_bip32_priv_t *out);

#endif
