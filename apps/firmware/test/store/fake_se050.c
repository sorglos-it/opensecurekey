/* SE050-Attrappe - siehe fake_se050.h. KEINE Sicherheit. */

#include <string.h>
#include "fake_se050.h"
#include "osk_se050.h"
#include "osk_crypto.h"

#define TRIES_MAX 8u
#define PRIV_LEN 32u
#define PUB_LEN 32u

#define DEVICES 4

/* Je Geraet ein eigener Platz. Sonst koennten nicht zwei Geraete
 * gleichzeitig bestehen - und genau das braucht Stick zu Stick. */
struct dev {
	uint32_t id;
	bool used;
	uint8_t priv[PRIV_LEN];
	bool have_key;
	osk_key_id_t key_id;
};

#define COUNTERS 4

/* Monotone Zaehler. Sie gehoeren zum GERAET und nicht zum Flash - das ist
 * der ganze Sinn: ein zurueckgespielter Flash nimmt sie nicht mit. Darum
 * ueberlebt dieser Platz auch fake_flash_wipe(). */
struct ctr {
	uint32_t dev;
	uint32_t id;
	bool used;
	uint64_t value;
};

static struct ctr g_ctr[COUNTERS];
static uint64_t g_ctr_start;

static uint32_t g_device = 1u;
static char g_pin[65] = "123456";
static uint8_t g_tries = TRIES_MAX;
static struct dev g_dev[DEVICES];

static struct dev *cur(void)
{
	for (int i = 0; i < DEVICES; i++) {
		if (g_dev[i].used && g_dev[i].id == g_device) {
			return &g_dev[i];
		}
	}
	for (int i = 0; i < DEVICES; i++) {
		if (!g_dev[i].used) {
			g_dev[i].used = true;
			g_dev[i].id = g_device;
			return &g_dev[i];
		}
	}
	/* Mehr Geraete braucht kein Pruefsatz. */
	g_dev[0].id = g_device;
	g_dev[0].have_key = false;
	return &g_dev[0];
}

void fake_se050_device(uint32_t id)
{
	g_device = id;
}

void fake_se050_set_pin(const char *pin)
{
	memset(g_pin, 0, sizeof(g_pin));
	if (pin != NULL) {
		size_t n = strlen(pin);

		if (n > sizeof(g_pin) - 1u) {
			n = sizeof(g_pin) - 1u;
		}
		memcpy(g_pin, pin, n);
	}
}

void fake_se050_reset(void)
{
	g_tries = TRIES_MAX;
	memset(g_dev, 0, sizeof(g_dev));
	memset(g_ctr, 0, sizeof(g_ctr));
	g_ctr_start = 0u;
}

/* ---- monotone Zaehler ------------------------------------------------- */

static struct ctr *ctr_find(uint32_t id)
{
	for (int i = 0; i < COUNTERS; i++) {
		if (g_ctr[i].used && g_ctr[i].dev == g_device && g_ctr[i].id == id) {
			return &g_ctr[i];
		}
	}
	return NULL;
}

void fake_se050_counter_start_at(uint64_t value)
{
	g_ctr_start = value;
}

uint64_t fake_se050_counter_peek(uint32_t id)
{
	struct ctr *c = ctr_find(id);

	return (c != NULL) ? c->value : (uint64_t)-1;
}

osk_result_t osk_se050_counter_create(osk_key_id_t id)
{
	/* Die Falle aus AN12413: ein WriteCounter OHNE Wert gegen einen
	 * BESTEHENDEN Zaehler heisst "um eins erhoehen". Eine
	 * Einrichtungsroutine, die zweimal laeuft, haette den Rueckdrehschutz
	 * still verschoben. Darum wird zuerst nachgesehen. */
	if (ctr_find(id) != NULL) {
		return OSK_ERR_EXISTS;
	}
	for (int i = 0; i < COUNTERS; i++) {
		if (!g_ctr[i].used) {
			g_ctr[i].used = true;
			g_ctr[i].dev = g_device;
			g_ctr[i].id = id;
			g_ctr[i].value = g_ctr_start;
			return OSK_OK;
		}
	}
	return OSK_ERR_FULL;
}

osk_result_t osk_se050_counter_read(osk_key_id_t id, uint64_t *value_out)
{
	struct ctr *c = ctr_find(id);

	if (value_out == NULL) {
		return OSK_ERR_ARG;
	}
	if (c == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	*value_out = c->value;
	return OSK_OK;
}

osk_result_t osk_se050_counter_raise(osk_key_id_t id, uint64_t at_least)
{
	struct ctr *c = ctr_find(id);

	if (c == NULL) {
		return OSK_ERR_NOT_FOUND;
	}
	/* Wiederholbar: steht er schon dort oder hoeher, aendert sich nichts
	 * und das Ergebnis ist trotzdem OSK_OK. Genau das wird nach einem
	 * Stromausfall mitten im Schreiben gebraucht. Kleiner wird er nie -
	 * es gibt keinen Weg zurueck, auch nicht in der Attrappe. */
	if (at_least > c->value) {
		c->value = at_least;
	}
	return OSK_OK;
}

uint8_t fake_se050_tries_left(void)
{
	return g_tries;
}

osk_result_t osk_se050_open(void)
{
	return OSK_OK;
}

void osk_se050_close(void)
{
}

osk_result_t osk_se050_curves(uint32_t *mask_out)
{
	if (mask_out == NULL) {
		return OSK_ERR_ARG;
	}
	*mask_out = 1u << OSK_CURVE_P256;
	return OSK_OK;
}

osk_result_t osk_se050_random(uint8_t *out, size_t len)
{
	return osk_crypto_random(out, len);
}

/* Das Grundgeheimnis verlaesst den Chip nie - herausgegeben wird nur die
 * Ableitung. Genau das stellt diese Funktion nach. */
osk_result_t osk_se050_hkdf(osk_key_id_t id, const uint8_t *info, size_t info_len, uint8_t *out,
			    size_t out_len)
{
	uint8_t ikm[8];

	if (out == NULL) {
		return OSK_ERR_ARG;
	}
	for (int i = 0; i < 4; i++) {
		ikm[i] = (uint8_t)(g_device >> (8 * i));
		ikm[4 + i] = (uint8_t)(id >> (8 * i));
	}
	return osk_crypto_hkdf(ikm, sizeof(ikm), NULL, 0, info, info_len, out, out_len);
}

osk_result_t osk_se050_hmac(osk_key_id_t id, const uint8_t *msg, size_t msg_len, uint8_t *mac_out,
			    size_t cap, size_t *len_out)
{
	if (mac_out == NULL || len_out == NULL || cap < OSK_HASH_LEN) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = osk_se050_hkdf(id, msg, msg_len, mac_out, OSK_HASH_LEN);

	if (r == OSK_OK) {
		*len_out = OSK_HASH_LEN;
	}
	return r;
}

osk_result_t osk_se050_pin_verify(const uint8_t *pin, size_t len)
{
	if (pin == NULL) {
		return OSK_ERR_ARG;
	}
	if (g_tries == 0u) {
		return OSK_ERR_LOCKED;
	}
	if (len == strlen(g_pin) && memcmp(pin, g_pin, len) == 0) {
		g_tries = TRIES_MAX;
		return OSK_OK;
	}
	g_tries--;
	return OSK_ERR_DENIED;
}

osk_result_t osk_se050_pin_set(const uint8_t *old_pin, size_t old_len, const uint8_t *new_pin,
			       size_t new_len)
{
	osk_result_t r = osk_se050_pin_verify(old_pin, old_len);

	if (r != OSK_OK) {
		return r;
	}
	if (new_pin == NULL || new_len == 0u || new_len > sizeof(g_pin) - 1u) {
		return OSK_ERR_ARG;
	}
	memset(g_pin, 0, sizeof(g_pin));
	memcpy(g_pin, new_pin, new_len);
	return OSK_OK;
}

/* ---- Spielzeug-Austausch --------------------------------------------- */

osk_result_t osk_se050_key_create(osk_key_id_t id, osk_curve_t curve,
				  const osk_key_policy_t *policy)
{
	(void)policy;
	if (curve != OSK_CURVE_P256) {
		return OSK_ERR_UNSUPPORTED;
	}
	struct dev *d = cur();
	osk_result_t r = osk_crypto_random(d->priv, sizeof(d->priv));

	if (r != OSK_OK) {
		return r;
	}
	d->key_id = id;
	d->have_key = true;
	return OSK_OK;
}

osk_result_t osk_se050_key_delete(osk_key_id_t id)
{
	struct dev *d = cur();

	if (!d->have_key || d->key_id != id) {
		return OSK_ERR_NOT_FOUND;
	}
	d->have_key = false;
	memset(d->priv, 0, sizeof(d->priv));
	return OSK_OK;
}

static const char INFO_PUB[] = "fake/pub";

osk_result_t osk_se050_key_public(osk_key_id_t id, uint8_t *out, size_t cap, size_t *len_out)
{
	struct dev *d = cur();

	if (!d->have_key || d->key_id != id) {
		return OSK_ERR_NOT_FOUND;
	}
	if (out == NULL || len_out == NULL || cap < PUB_LEN) {
		return OSK_ERR_ARG;
	}
	osk_result_t r =
	    osk_crypto_hkdf(d->priv, sizeof(d->priv), NULL, 0, (const uint8_t *)INFO_PUB,
			    sizeof(INFO_PUB) - 1, out, PUB_LEN);
	if (r == OSK_OK) {
		*len_out = PUB_LEN;
	}
	return r;
}

osk_result_t osk_se050_ecdh(osk_key_id_t id, const uint8_t *peer_pub, size_t peer_len,
			    uint8_t *secret_out, size_t cap, size_t *len_out)
{
	uint8_t own[PUB_LEN];
	uint8_t both[2u * PUB_LEN];
	size_t own_len = 0;

	if (peer_pub == NULL || secret_out == NULL || len_out == NULL || cap < OSK_HASH_LEN ||
	    peer_len != PUB_LEN) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = osk_se050_key_public(id, own, sizeof(own), &own_len);

	if (r != OSK_OK) {
		return r;
	}
	/* Sortiert, damit beide Seiten denselben Wert erhalten. Im Chip tut
	 * das die Kurvenrechnung von selbst. */
	int cmp = memcmp(own, peer_pub, PUB_LEN);

	if (cmp < 0) {
		memcpy(both, own, PUB_LEN);
		memcpy(both + PUB_LEN, peer_pub, PUB_LEN);
	} else {
		memcpy(both, peer_pub, PUB_LEN);
		memcpy(both + PUB_LEN, own, PUB_LEN);
	}
	r = osk_crypto_sha256(both, sizeof(both), secret_out);
	if (r == OSK_OK) {
		*len_out = OSK_HASH_LEN;
	}
	return r;
}

osk_result_t osk_se050_sign(osk_key_id_t id, const uint8_t *hash, size_t hash_len, uint8_t *sig_out,
			    size_t cap, size_t *len_out)
{
	if (sig_out == NULL || len_out == NULL || cap < 64u) {
		return OSK_ERR_ARG;
	}
	osk_result_t r = osk_se050_hkdf(id, hash, hash_len, sig_out, 64u);

	if (r == OSK_OK) {
		*len_out = 64u;
	}
	return r;
}
