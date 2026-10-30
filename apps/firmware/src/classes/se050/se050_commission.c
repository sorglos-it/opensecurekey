/* Der Erstlauf: Werksschluessel ersetzen, genau einmal.
 *
 * Vertrag und Begruendung: include/osk_commission.h und docs/erstlauf.md.
 *
 * Diese Datei kennt weder Bus noch Flash noch Krypto. Sie stellt Fragen an
 * den Chip und schreibt einen Zettel - beides ueber eingehaengte Haken.
 * Dadurch laeuft der ganze Ablauf im Prueflauf am PC, einschliesslich der
 * Abbruchfaelle, die am Geraet nur mit gezogenem Stecker zu messen waeren.
 *
 * DIE EINE REGEL, die hier alles traegt: der Zettel im Flash wird dem Chip
 * NIE geglaubt, sondern immer gegen ihn geprueft. Wer den Flash zuruecksetzt,
 * bekommt deshalb keinen zweiten ersten Start. */

#include <string.h>

#include "osk_commission.h"
#include "osk_mem.h"

/* Reihenfolge der Arbeit: Satz 12 vor Satz 11. Zwei Gruende, und beide
 * zaehlen:
 *
 * 1. Satz 12 ist der Rettungssatz, mit dem Satz 11 neu gesetzt werden kann
 *    (AN12973 2.3). Solange er NXP gehoert, ist ein getauschter Satz 11
 *    nichts wert - also muss er als erstes weg.
 * 2. Der Geraetekanal laeuft auf einem der beiden Saetze. Welcher es ist,
 *    ist am echten Chip nicht bestaetigt (docs/se050.md). Waere es Satz 11
 *    und wir taeuschten ihn zuerst, kaeme der zweite Befehl ueber einen
 *    Kanal, dessen Schluessel gerade gewechselt wurde - und Satz 12 bliebe
 *    fuer immer der von NXP. Satz 12 zuerst kann diesen Fall nicht haben.
 *
 * Die Reihenfolge rettet KEINEN der zwei Zwischenstaende: zwischen den
 * beiden Befehlen ist immer noch ein Werkssatz im Chip. Sicher ist erst das
 * fertige Paar; darum kommt require_scp() danach und nicht dazwischen. */
static const uint8_t k_order[OSK_SCP_SET_COUNT] = {OSK_SCP_SET_RESCUE, OSK_SCP_SET_MAIN};

/* Platz des Satzes im Zettel. Die Zuordnung steht nur hier. */
static osk_result_t slot_of(uint8_t key_version, size_t *idx_out, uint8_t *bit_out)
{
	switch (key_version) {
	case OSK_SCP_SET_MAIN:
		*idx_out = 0u;
		*bit_out = OSK_COMMISSION_DONE_MAIN;
		return OSK_OK;
	case OSK_SCP_SET_RESCUE:
		*idx_out = 1u;
		*bit_out = OSK_COMMISSION_DONE_RESCUE;
		return OSK_OK;
	default:
		return OSK_ERR_ARG;
	}
}

static bool backend_complete(const osk_commission_backend_t *be)
{
	return be != NULL && be->load != NULL && be->store != NULL && be->random_mcu != NULL &&
	       be->random_chip != NULL && be->factory_keys != NULL && be->probe != NULL &&
	       be->rotate != NULL && be->require_scp != NULL;
}

/* Ist der Puffer ganz Null? Eine ausgefallene Zufallsquelle liefert gern
 * Nullen und meldet dabei Erfolg. Ein Schluessel aus Nullen waere schlimmer
 * als der Werksschluessel, denn er sieht nach Sicherheit aus. */
static bool all_zero(const uint8_t *p, size_t n)
{
	uint8_t acc = 0;

	for (size_t i = 0; i < n; i++) {
		acc |= p[i];
	}
	return acc == 0u;
}

/* Einen Schluesselsatz erzeugen: zwei Quellen, mit XOR verknuepft.
 *
 * Warum zwei: keine der beiden soll den Schluessel allein bestimmen. Der
 * Zufall aus dem Chip muss ueber die I2C-Leitung, die auf einem Geraet im
 * Werkszustand mitlesbar ist; der aus dem Rechenkern verlaesst ihn nie.
 *
 * Was das NICHT leistet, damit niemand mehr hineinliest: der Lauscher, der
 * den Erstlauf mitschneidet, kennt die neuen Schluessel ohnehin - der
 * PUT KEY-Befehl ist mit dem WERKS-Key-DEK verpackt, und der steht in
 * AN12973. Gegen ihn hilft nur, dass der erste Start in sicherer Hand
 * geschieht. Die zwei Quellen sind gegen eine kaputte Quelle, nicht gegen
 * den Lauscher. Steht in docs/erstlauf.md, Frage 2. */
static osk_result_t make_keyset(const osk_commission_backend_t *be, osk_scp_keyset_t *out)
{
	osk_scp_keyset_t a;
	osk_scp_keyset_t b;

	osk_result_t r = be->random_mcu((uint8_t *)&a, sizeof(a));
	if (r != OSK_OK) {
		osk_wipe(&a, sizeof(a));
		return r;
	}
	r = be->random_chip((uint8_t *)&b, sizeof(b));
	if (r != OSK_OK) {
		osk_wipe(&a, sizeof(a));
		osk_wipe(&b, sizeof(b));
		return r;
	}

	/* Eine Quelle, die nur Nullen liefert, ist ausgefallen - auch wenn
	 * sie OSK_OK gemeldet hat. Dann wird nicht weitergerechnet: mit
	 * einer halben Quelle waere der Satz schwaecher, als er aussieht. */
	if (all_zero((const uint8_t *)&a, sizeof(a)) || all_zero((const uint8_t *)&b, sizeof(b))) {
		osk_wipe(&a, sizeof(a));
		osk_wipe(&b, sizeof(b));
		return OSK_ERR_INTERNAL;
	}

	uint8_t *dst = (uint8_t *)out;
	const uint8_t *pa = (const uint8_t *)&a;
	const uint8_t *pb = (const uint8_t *)&b;
	for (size_t i = 0; i < sizeof(*out); i++) {
		dst[i] = (uint8_t)(pa[i] ^ pb[i]);
	}
	osk_wipe(&a, sizeof(a));
	osk_wipe(&b, sizeof(b));

	if (all_zero(dst, sizeof(*out))) {
		/* Beide Quellen haben dasselbe geliefert. Das ist entweder
		 * ein Zufall jenseits jeder Wahrscheinlichkeit oder eine
		 * Quelle, die die andere abschreibt. */
		osk_wipe(out, sizeof(*out));
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

/* Die neuen Saetze erzeugen und aufschreiben, BEVOR ein Befehl den Chip
 * erreicht. Das ist der Angelpunkt des Stromausfalls: ab hier stehen die
 * Schluessel fest, und jeder weitere Lauf setzt DIESEN Tausch fort statt
 * einen neuen zu beginnen. Ein Angreifer, der den Strom im richtigen
 * Augenblick zieht, bekommt also keinen Satz seiner Wahl - er bekommt
 * denselben Satz noch einmal. */
static osk_result_t pin_down_keys(const osk_commission_backend_t *be,
				  osk_commission_record_t *rec)
{
	memset(rec, 0, sizeof(*rec));
	rec->magic = OSK_COMMISSION_MAGIC;
	rec->stage = (uint8_t)OSK_COMMISSION_PENDING;
	rec->done = 0u;

	for (size_t i = 0; i < OSK_SCP_SET_COUNT; i++) {
		osk_result_t r = make_keyset(be, &rec->keys[i]);
		if (r != OSK_OK) {
			osk_wipe(rec, sizeof(*rec));
			return r;
		}
	}

	/* Die neuen Saetze duerfen nicht die Werkssaetze sein. Das waere
	 * nur bei einer kaputten Zufallsquelle moeglich - und genau dann
	 * waere der Lauf gruen und das Geraet offen. Der Treiber lehnt
	 * current == replacement ebenfalls ab; hier wird es gefragt, bevor
	 * irgendetwas aufgeschrieben ist. */
	for (size_t i = 0; i < OSK_SCP_SET_COUNT; i++) {
		size_t idx = 0;
		uint8_t bit = 0;
		osk_scp_keyset_t fac;

		if (slot_of(k_order[i], &idx, &bit) != OSK_OK) {
			osk_wipe(rec, sizeof(*rec));
			return OSK_ERR_INTERNAL;
		}
		osk_result_t r = be->factory_keys(k_order[i], &fac);
		if (r != OSK_OK) {
			osk_wipe(&fac, sizeof(fac));
			osk_wipe(rec, sizeof(*rec));
			return r;
		}
		const bool same = memcmp(&fac, &rec->keys[idx], sizeof(fac)) == 0;
		osk_wipe(&fac, sizeof(fac));
		if (same) {
			osk_wipe(rec, sizeof(*rec));
			return OSK_ERR_INTERNAL;
		}
	}

	return be->store(rec);
}

/* Den Zettel an den Chip angleichen, bevor weitergearbeitet wird.
 *
 * Der Fall: PUT KEY ist durchgegangen, der Strom fiel vor dem Schreiben des
 * Zettels aus. Dann steht das Bit nicht, obwohl der Chip den Satz schon
 * hat - und ein zweiter PUT KEY mit dem WERKS-Schluessel als `current`
 * koennte nicht mehr gelingen, denn der gilt nicht mehr. Ohne diesen
 * Abgleich waere das Geraet an dieser Stelle fuer immer stecken.
 *
 * Geprueft wird es mit derselben Frage wie ueberall: oeffnet dieser Satz
 * den Kanal. Antwortet der Chip nicht eindeutig (OSK_ERR_UNSUPPORTED - die
 * Platine kann mit diesem Satz keinen Kanal aufbauen), bleibt das Bit
 * stehen, wie es war. Dann wird der Tausch versucht und darf scheitern;
 * geraten wird nicht. OFFEN AM GERAET: ob sich mit Satz 12 ueberhaupt ein
 * Kanal aufbauen laesst - steht in docs/erstlauf.md unter den Annahmen. */
static osk_result_t sync_record_with_chip(const osk_commission_backend_t *be,
					  osk_commission_record_t *rec)
{
	bool changed = false;

	for (size_t i = 0; i < OSK_SCP_SET_COUNT; i++) {
		size_t idx = 0;
		uint8_t bit = 0;

		if (slot_of(k_order[i], &idx, &bit) != OSK_OK) {
			return OSK_ERR_INTERNAL;
		}
		if ((rec->done & bit) != 0u) {
			continue;
		}
		if (be->probe(&rec->keys[idx]) == OSK_OK) {
			rec->done = (uint8_t)(rec->done | bit);
			changed = true;
		}
	}

	if (!changed) {
		return OSK_OK;
	}
	return be->store(rec);
}

/* Die zwei Befehle, in der Reihenfolge aus k_order. Jeder Satz wird nur
 * angefasst, wenn sein Bit fehlt - damit ist der Ablauf wiederholbar, und
 * ein Satz wird nie zweimal getauscht. */
static osk_result_t rotate_missing(const osk_commission_backend_t *be,
				   osk_commission_record_t *rec)
{
	for (size_t i = 0; i < OSK_SCP_SET_COUNT; i++) {
		size_t idx = 0;
		uint8_t bit = 0;
		osk_scp_keyset_t fac;

		osk_result_t r = slot_of(k_order[i], &idx, &bit);
		if (r != OSK_OK) {
			return OSK_ERR_INTERNAL;
		}
		if ((rec->done & bit) != 0u) {
			continue;
		}

		r = be->factory_keys(k_order[i], &fac);
		if (r != OSK_OK) {
			osk_wipe(&fac, sizeof(fac));
			return r;
		}
		r = be->rotate(k_order[i], &fac, &rec->keys[idx]);
		osk_wipe(&fac, sizeof(fac));
		if (r != OSK_OK) {
			/* Der Zettel bleibt, wie er ist: PENDING, mit den
			 * Bits der schon erledigten Saetze. Der naechste
			 * Start setzt hier fort. */
			return r;
		}

		/* Erst der Chip, dann der Zettel. Umgekehrt stuende im
		 * Zettel ein Satz als getauscht, den der Chip nie bekam -
		 * und das Geraet suchte seinen Kanal mit Schluesseln, die
		 * nirgends gelten. */
		rec->done = (uint8_t)(rec->done | bit);
		r = be->store(rec);
		if (r != OSK_OK) {
			return r;
		}
	}
	return OSK_OK;
}

/* Nachmessen, dass die Werksschluessel wirklich weg sind.
 *
 * Nicht ein zweites "sieht gut aus", sondern die einzige Pruefung, die der
 * Zweck des ganzen Ablaufs verlangt: wer mit AN12973 in der Hand kommt,
 * darf keinen Kanal mehr bekommen. Haelt der Chip einen Werkssatz noch an,
 * wird der Kanal NICHT verpflichtend gemacht und der Lauf meldet Fehler -
 * ein Geraet mit SCP_REQUIRED und bekannten Schluesseln waere das
 * schlechteste von beidem. */
static osk_result_t verify_factory_dead(const osk_commission_backend_t *be)
{
	for (size_t i = 0; i < OSK_SCP_SET_COUNT; i++) {
		osk_scp_keyset_t fac;

		osk_result_t r = be->factory_keys(k_order[i], &fac);
		if (r != OSK_OK) {
			osk_wipe(&fac, sizeof(fac));
			return r;
		}
		const osk_result_t p = be->probe(&fac);
		osk_wipe(&fac, sizeof(fac));
		if (p == OSK_OK) {
			return OSK_ERR_INTERNAL;
		}
	}
	return OSK_OK;
}

osk_result_t osk_commission_run(const osk_commission_backend_t *be)
{
	if (!backend_complete(be)) {
		return OSK_ERR_ARG;
	}

	osk_commission_record_t rec;
	osk_result_t r = be->load(&rec);

	if (r == OSK_ERR_NOT_FOUND) {
		/* Kein Zettel. Das ist ein frisches Geraet - oder eines,
		 * dessen Flash geloescht wurde. Welches von beidem, sagt
		 * der Chip: nur im Werkszustand geht es weiter. */
		memset(&rec, 0, sizeof(rec));
		rec.magic = OSK_COMMISSION_MAGIC;
		rec.stage = (uint8_t)OSK_COMMISSION_FACTORY;
	} else if (r != OSK_OK) {
		return r;
	} else if (rec.magic != OSK_COMMISSION_MAGIC ||
		   rec.stage > (uint8_t)OSK_COMMISSION_DONE) {
		osk_wipe(&rec, sizeof(rec));
		return OSK_ERR_FORMAT;
	}

	if (rec.stage == (uint8_t)OSK_COMMISSION_DONE) {
		/* Fertig - aber geglaubt wird das nicht. Gilt der eigene
		 * Satz noch, ist alles gut und es geht kein Befehl auf den
		 * Bus. Gilt stattdessen wieder ein Werkssatz, dann wurde
		 * der Chip zurueckgesetzt oder getauscht, und ein Erstlauf
		 * waere genau das, was ein Angreifer will: ein Tausch gegen
		 * Schluessel, die er mitgehoert hat. Also nichts tun und
		 * den Widerspruch melden. */
		const osk_result_t own = be->probe(&rec.keys[0]);
		if (own == OSK_OK) {
			osk_wipe(&rec, sizeof(rec));
			return OSK_OK;
		}
		osk_scp_keyset_t fac;
		osk_result_t fr = be->factory_keys(OSK_SCP_SET_MAIN, &fac);
		if (fr == OSK_OK) {
			fr = be->probe(&fac);
		}
		osk_wipe(&fac, sizeof(fac));
		osk_wipe(&rec, sizeof(rec));
		if (fr == OSK_OK) {
			return OSK_ERR_ROLLBACK;
		}
		/* Weder der eigene noch der Werkssatz oeffnet. Der Chip ist
		 * nicht ansprechbar - kaputt, abgezogen oder ein fremder.
		 * Das ist ein Fehler der Leitung, kein Erstlauf. */
		return own;
	}

	if (rec.stage == (uint8_t)OSK_COMMISSION_FACTORY) {
		/* Jetzt die Frage, an der der ganze Erstlauf haengt: ist der
		 * Chip wirklich im Werkszustand? */
		osk_scp_keyset_t fac;
		osk_result_t fr = be->factory_keys(OSK_SCP_SET_MAIN, &fac);
		if (fr != OSK_OK) {
			osk_wipe(&fac, sizeof(fac));
			osk_wipe(&rec, sizeof(rec));
			return fr;
		}
		fr = be->probe(&fac);
		osk_wipe(&fac, sizeof(fac));
		if (fr != OSK_OK) {
			/* Der Zettel sagt Werkszustand, der Chip nicht. Also
			 * ist der Zettel zurueckgedreht worden - und die
			 * Schluessel, die jetzt gelten, kennen wir nicht.
			 * Hier wird NICHT getauscht; ein Tausch braucht den
			 * gueltigen Satz als `current`, und ohne ihn gaebe
			 * es nur Fehler. Das Geraet ist nicht mehr zu
			 * oeffnen, und das ist das ehrliche Ergebnis. */
			osk_wipe(&rec, sizeof(rec));
			return OSK_ERR_ROLLBACK;
		}

		r = pin_down_keys(be, &rec);
		if (r != OSK_OK) {
			osk_wipe(&rec, sizeof(rec));
			return r;
		}
	} else {
		/* PENDING: derselbe Tausch wird fortgesetzt, mit den
		 * Schluesseln, die im Zettel stehen. Vorher der Abgleich
		 * mit dem Chip. */
		r = sync_record_with_chip(be, &rec);
		if (r != OSK_OK) {
			osk_wipe(&rec, sizeof(rec));
			return r;
		}
	}

	r = rotate_missing(be, &rec);
	if (r != OSK_OK) {
		osk_wipe(&rec, sizeof(rec));
		return r;
	}

	r = verify_factory_dead(be);
	if (r != OSK_OK) {
		osk_wipe(&rec, sizeof(rec));
		return r;
	}

	/* Der Kanal wird verpflichtend. Erst hier, mit beiden Saetzen in
	 * eigener Hand: dauerhaft, nicht zuruecknehmbar (AN12413 4.6.2). */
	r = be->require_scp();
	if (r != OSK_OK) {
		osk_wipe(&rec, sizeof(rec));
		return r;
	}

	rec.stage = (uint8_t)OSK_COMMISSION_DONE;
	r = be->store(&rec);
	osk_wipe(&rec, sizeof(rec));
	return r;
}

osk_result_t osk_commission_active_keys(const osk_commission_backend_t *be, uint8_t key_version,
					osk_scp_keyset_t *out)
{
	if (be == NULL || be->load == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}

	size_t idx = 0;
	uint8_t bit = 0;
	osk_result_t r = slot_of(key_version, &idx, &bit);
	if (r != OSK_OK) {
		return r;
	}

	osk_commission_record_t rec;
	r = be->load(&rec);
	if (r == OSK_ERR_NOT_FOUND) {
		return OSK_ERR_NOT_FOUND;
	}
	if (r != OSK_OK) {
		return r;
	}
	if (rec.magic != OSK_COMMISSION_MAGIC || rec.stage > (uint8_t)OSK_COMMISSION_DONE) {
		osk_wipe(&rec, sizeof(rec));
		return OSK_ERR_FORMAT;
	}

	/* Das Bit entscheidet, nicht die Stufe: waehrend eines
	 * abgebrochenen Tauschs gilt fuer einen Satz schon der eigene
	 * Schluessel und fuer den anderen noch der Werkssatz. Wer hier die
	 * Stufe fragte, griffe in genau diesem Zustand zum falschen. */
	if ((rec.done & bit) == 0u) {
		osk_wipe(&rec, sizeof(rec));
		return OSK_ERR_NOT_FOUND;
	}

	memcpy(out, &rec.keys[idx], sizeof(*out));
	osk_wipe(&rec, sizeof(rec));
	return OSK_OK;
}
