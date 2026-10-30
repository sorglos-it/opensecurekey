/* Die Zusammenfuehrung beider Geheimnisse. Siehe osk_pqc.h.
 *
 * Wie gemischt wird, und warum so:
 *
 *   ss = SHA3-256( Kennung || x_ss || m_ss || x_ct || m_ct || x_pub || m_ek )
 *
 * Beide Anteile gehen durch EINEN Hash, zusammen mit dem ganzen
 * Gespraech. Drei Eigenschaften, auf die es ankommt:
 *
 * 1. Wer einen Anteil bricht, gewinnt nichts. SHA3-256 ist ueber die
 *    Verkettung nicht umkehrbar und nicht teilweise umkehrbar: wer x_ss
 *    kennt, m_ss aber nicht (oder umgekehrt), hat 256 unbekannte Bit
 *    im Vorbild stehen. Es gibt keinen Rechenweg, der das Ergebnis aus
 *    einem Anteil allein naehert - anders als bei einem XOR oder einer
 *    Addition der beiden fertigen Schluessel, wo der bekannte Anteil
 *    sich herausrechnen laesst. Das Ergebnis bleibt also sicher,
 *    solange EINER von beiden hielt.
 *
 * 2. Beide Schluesseltexte und beide oeffentlichen Schluessel gehen mit
 *    hinein. Ein Angreifer, der unterwegs einen der beiden Anteile
 *    austauscht - etwa den ML-KEM-Schluesseltext gegen einen eigenen
 *    oder den oeffentlichen Schluessel gegen seinen -, bekommt dadurch
 *    auf den zwei Seiten verschiedene Ergebnisse. Ohne diese Bindung
 *    sind Angriffe bekannt, bei denen ein gueltiges Geheimnis in einen
 *    anderen Zusammenhang getragen wird, obwohl beide Verfahren halten.
 *
 * 3. Die Kennung vorn trennt den Bereich. Dieselben zwei Anteile fuer
 *    eine andere Aufgabe ergeben mit einer anderen Kennung ein anderes
 *    Geheimnis.
 *
 * Was hier NICHT gebaut ist, und zwar absichtlich: ein Weg, der ohne
 * x_ss auskommt. Fehlt der X25519-Anteil oder ist er lauter Nullen,
 * bricht die Funktion ab. Es gibt keinen Schalter dafuer.
 */
#define OSK_PQC_INTERNAL 1
#include "osk_pqc.h"
#include "pqc_mlkem.h"
#include "pqc_keccak.h"
#include "osk_mem.h"

#define POLYVEC_BYTES (OSK_MLKEM_K * OSK_MLKEM_POLY_BYTES)

/* Lauter Nullen? Immer alle Byte angesehen, ohne Verzweigung. Bei X25519
 * heisst ein Ergebnis aus lauter Nullen: die Gegenseite hat einen Punkt
 * kleiner Ordnung geschickt. Dann traegt nur noch ML-KEM - und das ist
 * genau der Zustand, den dieses Modul nicht zulassen darf. */
static int all_zero(const uint8_t *p, size_t len)
{
	uint8_t any = 0u;

	for (size_t i = 0; i < len; i++) {
		any |= p[i];
	}
	return any == 0u;
}

/* Die eine Mischung. Beide Anteile und das ganze Gespraech, in dieser
 * Reihenfolge - sie steht hier einmal und wird von beiden Seiten
 * benutzt, damit sie nicht an zwei Stellen auseinanderlaufen kann. */
static void combine(uint8_t ss[OSK_PQC_SS_BYTES], const uint8_t x_ss[OSK_PQC_X_BYTES],
		    const uint8_t m_ss[OSK_MLKEM_SS_BYTES], const uint8_t x_ct[OSK_PQC_X_BYTES],
		    const uint8_t m_ct[OSK_MLKEM_CT_BYTES], const uint8_t x_pub[OSK_PQC_X_BYTES],
		    const uint8_t m_ek[OSK_MLKEM_EK_BYTES])
{
	struct osk_keccak st;
	static const char label[] = OSK_PQC_LABEL;

	osk_keccak_init(&st, OSK_SHA3_256_RATE, OSK_KECCAK_PAD_SHA3);
	/* ohne die abschliessende Null - sie traegt nichts und waere eine
	 * Kleinigkeit, in der sich zwei Umsetzungen unterscheiden koennen */
	osk_keccak_absorb(&st, (const uint8_t *)label, sizeof(label) - 1u);
	osk_keccak_absorb(&st, x_ss, OSK_PQC_X_BYTES);
	osk_keccak_absorb(&st, m_ss, OSK_MLKEM_SS_BYTES);
	osk_keccak_absorb(&st, x_ct, OSK_PQC_X_BYTES);
	osk_keccak_absorb(&st, m_ct, OSK_MLKEM_CT_BYTES);
	osk_keccak_absorb(&st, x_pub, OSK_PQC_X_BYTES);
	osk_keccak_absorb(&st, m_ek, OSK_MLKEM_EK_BYTES);
	osk_keccak_squeeze(&st, ss, OSK_PQC_SS_BYTES);
	osk_keccak_wipe(&st);
}

osk_result_t osk_pqc_keypair(uint8_t ek[OSK_PQC_EK_BYTES], uint8_t dk[OSK_PQC_DK_BYTES],
			     const uint8_t x_pub[OSK_PQC_X_BYTES], const uint8_t d[32],
			     const uint8_t z[32])
{
	if (ek == NULL || dk == NULL || x_pub == NULL || d == NULL || z == NULL) {
		return OSK_ERR_ARG;
	}
	if (all_zero(x_pub, OSK_PQC_X_BYTES)) {
		return OSK_ERR_ARG;
	}

	for (size_t i = 0; i < OSK_PQC_X_BYTES; i++) {
		ek[i] = x_pub[i];
		dk[i] = x_pub[i];
	}
	osk_mlkem_keygen(ek + OSK_PQC_X_BYTES, dk + OSK_PQC_X_BYTES, d, z);
	return OSK_OK;
}

osk_result_t osk_pqc_encaps(uint8_t ct[OSK_PQC_CT_BYTES], uint8_t ss[OSK_PQC_SS_BYTES],
			    const uint8_t ek[OSK_PQC_EK_BYTES], const uint8_t x_ct[OSK_PQC_X_BYTES],
			    const uint8_t x_ss[OSK_PQC_X_BYTES], const uint8_t m[32])
{
	uint8_t m_ss[OSK_MLKEM_SS_BYTES];
	osk_result_t res;

	if (ct == NULL || ss == NULL || ek == NULL || x_ct == NULL || x_ss == NULL || m == NULL) {
		return OSK_ERR_ARG;
	}
	/* Kein Weg ohne den Anteil aus dem Chip. */
	if (all_zero(x_ss, OSK_PQC_X_BYTES)) {
		return OSK_ERR_ARG;
	}

	for (size_t i = 0; i < OSK_PQC_X_BYTES; i++) {
		ct[i] = x_ct[i];
	}
	res = osk_mlkem_encaps(ct + OSK_PQC_X_BYTES, m_ss, ek + OSK_PQC_X_BYTES, m);
	if (res != OSK_OK) {
		osk_wipe(m_ss, sizeof(m_ss));
		return res;
	}
	combine(ss, x_ss, m_ss, x_ct, ct + OSK_PQC_X_BYTES, ek, ek + OSK_PQC_X_BYTES);
	osk_wipe(m_ss, sizeof(m_ss));
	return OSK_OK;
}

osk_result_t osk_pqc_decaps(uint8_t ss[OSK_PQC_SS_BYTES], const uint8_t dk[OSK_PQC_DK_BYTES],
			    const uint8_t ct[OSK_PQC_CT_BYTES], const uint8_t x_ss[OSK_PQC_X_BYTES])
{
	uint8_t m_ss[OSK_MLKEM_SS_BYTES];
	const uint8_t *m_dk = dk + OSK_PQC_X_BYTES;
	/* Der oeffentliche ML-KEM-Schluessel steht im geheimen mit drin
	 * (FIPS 203, Alg. 18) - von dort, nicht aus einer zweiten Kopie. */
	const uint8_t *m_ek = m_dk + POLYVEC_BYTES;
	osk_result_t res;

	if (ss == NULL || dk == NULL || ct == NULL || x_ss == NULL) {
		return OSK_ERR_ARG;
	}
	if (all_zero(x_ss, OSK_PQC_X_BYTES)) {
		return OSK_ERR_ARG;
	}

	res = osk_mlkem_decaps(m_ss, m_dk, ct + OSK_PQC_X_BYTES);
	if (res != OSK_OK) {
		osk_wipe(m_ss, sizeof(m_ss));
		return res;
	}
	/* Passte der ML-KEM-Schluesseltext nicht, steht in m_ss das
	 * Ersatzgeheimnis aus z - kein Fehler, und das Ergebnis haengt
	 * weiter am Anteil aus dem Chip. */
	combine(ss, x_ss, m_ss, ct, ct + OSK_PQC_X_BYTES, dk, m_ek);
	osk_wipe(m_ss, sizeof(m_ss));
	return OSK_OK;
}
