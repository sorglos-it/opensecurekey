/* Zugriff auf die Schluessel im Chip. Siehe osk_guard_keys.h. */
#include <string.h>
#include "osk_guard_keys.h"

typedef struct {
	bool used;
	uint8_t len;
	char label[OSK_GUARD_LABEL_MAX];
	osk_guard_node_curve_t curve;
	osk_key_id_t id;
} node_entry_t;

static node_entry_t g_nodes[OSK_GUARD_NODE_MAX];

void osk_guard_keys_reset(void)
{
	memset(g_nodes, 0, sizeof(g_nodes));
}

static bool label_ok(const char *label, size_t len)
{
	return label != NULL && len > 0u && len <= OSK_GUARD_LABEL_MAX &&
	       memchr(label, 0, len) == NULL;
}

static node_entry_t *find(const char *label, size_t len)
{
	for (size_t i = 0; i < OSK_GUARD_NODE_MAX; i++) {
		if (!g_nodes[i].used || g_nodes[i].len != len) {
			continue;
		}
		if (memcmp(g_nodes[i].label, label, len) == 0) {
			return &g_nodes[i];
		}
	}
	return NULL;
}

osk_result_t osk_guard_node_register(const char *label, size_t label_len,
				     osk_guard_node_curve_t curve)
{
	if (!label_ok(label, label_len)) {
		return OSK_ERR_ARG;
	}
	if (curve != OSK_GUARD_CURVE_ED25519 && curve != OSK_GUARD_CURVE_X25519) {
		return OSK_ERR_ARG;
	}

	node_entry_t *found = find(label, label_len);
	if (found != NULL) {
		/* Derselbe Name mit anderer Kurve waere ein zweiter Schluessel
		 * unter einem schon belegten Namen. Dann steht nicht fest,
		 * welcher gemeint ist - also abgelehnt. */
		return found->curve == curve ? OSK_OK : OSK_ERR_EXISTS;
	}

	size_t slot = OSK_GUARD_NODE_MAX;
	for (size_t i = 0; i < OSK_GUARD_NODE_MAX; i++) {
		if (!g_nodes[i].used) {
			slot = i;
			break;
		}
	}
	if (slot == OSK_GUARD_NODE_MAX) {
		return OSK_ERR_FULL;
	}

	const osk_key_id_t id = OSK_GUARD_KEY_NODE_BASE + (osk_key_id_t)slot;
	const osk_key_policy_t policy = {
	    .exportable = false,  /* der private Teil verlaesst den Chip nie */
	    .needs_touch = false, /* ueber die Freigabe entscheidet dieses Applet */
	};
	const osk_curve_t chip_curve =
	    curve == OSK_GUARD_CURVE_ED25519 ? OSK_CURVE_ED25519 : OSK_CURVE_X25519;

	osk_result_t rc = osk_se050_key_create(id, chip_curve, &policy);
	/* Liegt das Objekt schon im Chip, ist das kein Fehler: nach einem
	 * Neustart ist die Tabelle leer, die Objekte sind aber noch da. */
	if (rc != OSK_OK && rc != OSK_ERR_EXISTS) {
		return rc;
	}

	g_nodes[slot].used = true;
	g_nodes[slot].len = (uint8_t)label_len;
	memcpy(g_nodes[slot].label, label, label_len);
	g_nodes[slot].curve = curve;
	g_nodes[slot].id = id;
	return OSK_OK;
}

osk_result_t osk_guard_node_lookup(const char *label, size_t label_len,
				   osk_guard_node_curve_t curve, osk_key_id_t *id_out)
{
	if (!label_ok(label, label_len) || id_out == NULL) {
		return OSK_ERR_ARG;
	}
	node_entry_t *found = find(label, label_len);
	if (found == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	if (found->curve != curve) {
		/* Mit einem Unterschreibschluessel laesst sich kein Schluessel
		 * austauschen und umgekehrt. Das ist keine fehlende Freigabe,
		 * sondern der falsche Schluessel. */
		return OSK_ERR_NOT_FOUND;
	}
	*id_out = found->id;
	return OSK_OK;
}

/* Eine Unterschrift hat beim Ed25519 immer 64 Byte. Meldet der Chip eine
 * andere Laenge, ist etwas anderes passiert als eine Unterschrift - dann
 * geben wir keine halbe heraus. */
static osk_result_t sign_with(osk_key_id_t id, const uint8_t *msg, size_t msg_len,
			      uint8_t sig_out[OSK_GUARD_SIG_LEN])
{
	if (msg == NULL || msg_len == 0u || msg_len > OSK_GUARD_MSG_MAX || sig_out == NULL) {
		return OSK_ERR_ARG;
	}
	size_t len = 0;
	/* Ed25519 hasht selbst: hier geht die NACHRICHT hinein, nicht ihr
	 * Hash (osk_se050.h, AN12413 4.10.1.2). */
	osk_result_t rc = osk_se050_sign(id, msg, msg_len, sig_out, OSK_GUARD_SIG_LEN, &len);
	if (rc != OSK_OK) {
		return rc;
	}
	if (len != OSK_GUARD_SIG_LEN) {
		memset(sig_out, 0, OSK_GUARD_SIG_LEN);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

static osk_result_t public_of(osk_key_id_t id, uint8_t pub_out[OSK_GUARD_PUB_LEN])
{
	size_t len = 0;
	osk_result_t rc = osk_se050_key_public(id, pub_out, OSK_GUARD_PUB_LEN, &len);
	if (rc != OSK_OK) {
		return rc;
	}
	if (len != OSK_GUARD_PUB_LEN) {
		memset(pub_out, 0, OSK_GUARD_PUB_LEN);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

osk_result_t osk_guard_keys_attest(const uint8_t *msg, size_t msg_len,
				   uint8_t sig_out[OSK_GUARD_SIG_LEN],
				   uint8_t pub_out[OSK_GUARD_PUB_LEN])
{
	if (pub_out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_result_t rc = sign_with(OSK_GUARD_KEY_IDENTITY, msg, msg_len, sig_out);
	if (rc != OSK_OK) {
		return rc;
	}
	rc = public_of(OSK_GUARD_KEY_IDENTITY, pub_out);
	if (rc != OSK_OK) {
		memset(sig_out, 0, OSK_GUARD_SIG_LEN);
	}
	return rc;
}

osk_result_t osk_guard_keys_node_sign(const char *label, size_t label_len, const uint8_t *msg,
				      size_t msg_len, uint8_t sig_out[OSK_GUARD_SIG_LEN])
{
	osk_key_id_t id = 0;
	osk_result_t rc = osk_guard_node_lookup(label, label_len, OSK_GUARD_CURVE_ED25519, &id);
	if (rc != OSK_OK) {
		return rc;
	}
	return sign_with(id, msg, msg_len, sig_out);
}

osk_result_t osk_guard_keys_node_public(const char *label, size_t label_len,
					osk_guard_node_curve_t curve,
					uint8_t pub_out[OSK_GUARD_PUB_LEN])
{
	if (pub_out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_key_id_t id = 0;
	osk_result_t rc = osk_guard_node_lookup(label, label_len, curve, &id);
	if (rc != OSK_OK) {
		return rc;
	}
	return public_of(id, pub_out);
}

osk_result_t osk_guard_keys_agree(const char *label, size_t label_len,
				  const uint8_t peer_pub[OSK_GUARD_PUB_LEN],
				  uint8_t secret_out[OSK_GUARD_PUB_LEN])
{
	if (peer_pub == NULL || secret_out == NULL) {
		return OSK_ERR_ARG;
	}
	osk_key_id_t id = 0;
	osk_result_t rc = osk_guard_node_lookup(label, label_len, OSK_GUARD_CURVE_X25519, &id);
	if (rc != OSK_OK) {
		return rc;
	}
	size_t len = 0;
	rc = osk_se050_ecdh(id, peer_pub, OSK_GUARD_PUB_LEN, secret_out, OSK_GUARD_PUB_LEN, &len);
	if (rc != OSK_OK) {
		/* OSK_ERR_UNSUPPORTED geht unveraendert nach oben durch: ein im
		 * Werk auf FIPS gestelltes Stueck macht kein ECDH, und dann
		 * faellt der Anwendungsfall weg. Hilfsweise in der Firmware zu
		 * rechnen waere nicht "etwas weniger sicher", sondern die
		 * Ruecknahme der einzigen Zusage des Serverbetriebs
		 * (docs/threat-model.md). */
		return rc;
	}
	if (len != OSK_GUARD_PUB_LEN) {
		memset(secret_out, 0, OSK_GUARD_PUB_LEN);
		return OSK_ERR_INTERNAL;
	}
	return OSK_OK;
}

osk_result_t osk_guard_keys_derive(const uint8_t *info, size_t info_len, uint8_t *out,
				   size_t out_len)
{
	if (info == NULL || out == NULL) {
		return OSK_ERR_ARG;
	}
	if (info_len == 0u || info_len > OSK_GUARD_INFO_MAX) {
		return OSK_ERR_ARG;
	}
	if (out_len == 0u || out_len > OSK_GUARD_DERIVE_MAX) {
		return OSK_ERR_ARG;
	}
	/* Nur ableiten, nie lesen: osk_se050_hkdf() rechnet im Chip aus dem
	 * Grundgeheimnis. Es gibt in diesem Modul keinen Weg, das
	 * Grundgeheimnis selbst herauszugeben - der Chip hat dafuer auch
	 * keinen Befehl (ReadObject lehnt symmetrische Schluessel ab,
	 * AN12413 3.7.1). */
	return osk_se050_hkdf(OSK_GUARD_KEY_ROOT, info, info_len, out, out_len);
}
