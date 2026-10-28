#include <string.h>
#include "se050_sim.h"
#include "se050_t1.h"
#include "se050_t1_wire.h"
#include "se050_crypto.h"
#include "se050_cmd.h"
#include "se050_tlv.h"

static const osk_scp03_static_keys_t SIM_KEYS = {
    .enc = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
	    0x4E, 0x4F},
    .mac = {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D,
	    0x5E, 0x5F},
    .dek = {0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D,
	    0x6E, 0x6F},
};

/* Weiter unten in dieser Datei. */
static size_t sim_respond(void *ctx, const uint8_t *frame, size_t len, uint8_t *out, size_t cap);

const osk_scp03_static_keys_t *sim_keys(void)
{
	return &SIM_KEYS;
}

void sim_set_auth_keys(se050_sim_t *sim, const osk_scp03_static_keys_t *keys)
{
	sim->next_auth_keys = *keys;
}

/* Den Nachbau wieder an den Bus haengen, OHNE seinen Zustand zu loeschen.
 *
 * Damit laesst sich ein Neustart der Firmware nachstellen: der Treiber
 * wird geschlossen und neu geoeffnet, der Baustein behaelt seine Objekte.
 * Genau das ist bei einem Zaehler der Punkt - ein Test, der nur innerhalb
 * eines Laufs zaehlt, zeigt den Fehler nicht, der gefaehrlich wird. */
void sim_reattach(se050_sim_t *sim, fake_bus_t *fb)
{
	osk_scp03_close(&sim->scp);
	sim->applet_selected = false;
	sim->channel = false;
	sim->session_created = false;
	memset(sim->cmd_counter, 0, sizeof(sim->cmd_counter));
	fb_set_responder(fb, sim_respond, sim);
}

void sim_objects_live(se050_sim_t *sim, bool on)
{
	sim->objects_live = on;
}

void sim_set_reply(se050_sim_t *sim, const uint8_t *body, size_t len, uint16_t sw)
{
	sim->next_body_len = 0;
	if (body != NULL && len > 0 && len <= sizeof(sim->next_body)) {
		memcpy(sim->next_body, body, len);
		sim->next_body_len = len;
	}
	sim->next_sw = sw;
}

/* ------------------------------------------------- Antwort verpacken */

/* Eine Antwort so bauen, wie der Baustein sie im stehenden Kanal baut:
 * Nutzlast auffuellen und mit dem Antwort-Startwert verschluesseln, dann
 * R-MAC darueber (Amendment D 6.2.5 und 6.2.7). */
static size_t wrap_reply(se050_sim_t *sim, const uint8_t *body, size_t body_len, uint16_t sw,
			 uint8_t *out, size_t cap)
{
	if (sw != SW_NO_ERROR) {
		/* Fehlerstatus: nur die zwei Bytes, ohne MAC. */
		if (cap < 2u) {
			return 0;
		}
		out[0] = (uint8_t)(sw >> 8);
		out[1] = (uint8_t)(sw & 0xFFu);
		return 2u;
	}

	size_t padded = 0;
	uint8_t plain[1104];
	if (body_len > 0) {
		padded = ((body_len + 1u + 15u) / 16u) * 16u;
		if (padded > sizeof(plain)) {
			return 0;
		}
		memcpy(plain, body, body_len);
		plain[body_len] = 0x80u;
		memset(&plain[body_len + 1u], 0, padded - body_len - 1u);
	}

	if (cap < padded + SCP03_MAC_LEN + 2u) {
		return 0;
	}

	if (padded > 0) {
		uint8_t block[16];
		memcpy(block, sim->cmd_counter, 16);
		block[0] = 0x80u;

		uint8_t icv[16];
		if (osk_aes_ecb_encrypt_block(sim->scp.s_enc, block, icv) != OSK_OK) {
			return 0;
		}
		if (osk_aes_cbc_encrypt(sim->scp.s_enc, icv, plain, out, padded) != OSK_OK) {
			return 0;
		}
	}

	uint8_t macin[16 + 1104 + 2];
	memcpy(&macin[0], sim->scp.chain, 16);
	if (padded > 0) {
		memcpy(&macin[16], out, padded);
	}
	macin[16 + padded] = (uint8_t)(sw >> 8);
	macin[16 + padded + 1u] = (uint8_t)(sw & 0xFFu);

	uint8_t mac[16];
	if (osk_aes_cmac(sim->scp.s_rmac, macin, 16u + padded + 2u, mac) != OSK_OK) {
		return 0;
	}

	memcpy(&out[padded], mac, SCP03_MAC_LEN);
	out[padded + SCP03_MAC_LEN] = (uint8_t)(sw >> 8);
	out[padded + SCP03_MAC_LEN + 1u] = (uint8_t)(sw & 0xFFu);
	return padded + SCP03_MAC_LEN + 2u;
}

/* -------------------------------------- Objekte, die der Nachbau fuehrt */

/* Zaehler und RSA-Schluessel wirklich halten, statt eine Antwort
 * abzuspulen. Nur eingeschaltet (sim_objects_live), weil die aelteren
 * Tests ihre Antworten selbst setzen.
 *
 * Was hier nachgebaut ist, sind genau die zwei Zusagen aus
 * AN12413 4.7.1.6: ein Zaehler geht nur vorwaerts, und ein Wert (TAG_3)
 * muss dieselbe Laenge haben wie der vorhandene Zaehler. Alles andere
 * waere Beiwerk. */

static sim_counter_t *counter_find(se050_sim_t *sim, uint32_t id)
{
	for (size_t i = 0; i < SIM_MAX_COUNTERS; i++) {
		if (sim->counters[i].used && sim->counters[i].id == id) {
			return &sim->counters[i];
		}
	}
	return NULL;
}

static sim_rsa_t *rsa_find(se050_sim_t *sim, uint32_t id)
{
	for (size_t i = 0; i < SIM_MAX_RSA; i++) {
		if (sim->rsa[i].used && sim->rsa[i].id == id) {
			return &sim->rsa[i];
		}
	}
	return NULL;
}

/* Statuswort allein als Antwort. */
static size_t only_sw(uint8_t *out, size_t cap, uint16_t sw)
{
	if (cap < 2u) {
		return 0;
	}
	out[0] = (uint8_t)(sw >> 8);
	out[1] = (uint8_t)(sw & 0xFFu);
	return 2u;
}

/* Ein TLV[TAG_1] mit Wert, dann 0x9000. */
static size_t tag1_sw(uint8_t *out, size_t cap, const uint8_t *val, size_t val_len)
{
	osk_tlv_writer_t w;
	osk_tlv_init(&w, out, (cap > 2u) ? (cap - 2u) : 0u);
	osk_tlv_put(&w, TAG_1, val, val_len);

	size_t n = 0;
	if (osk_tlv_done(&w, &n) != OSK_OK) {
		return 0;
	}
	out[n] = 0x90u;
	out[n + 1u] = 0x00u;
	return n + 2u;
}

static bool tlv_u32(const osk_tlv_reader_t *rd, uint8_t tag, uint32_t *out)
{
	const uint8_t *v = NULL;
	size_t n = 0;
	if (osk_tlv_get(rd, tag, &v, &n) != OSK_OK || n != 4u) {
		return false;
	}
	*out = ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) | ((uint32_t)v[2] << 8) |
	       (uint32_t)v[3];
	return true;
}

static size_t handle_object(se050_sim_t *sim, uint8_t ins, uint8_t p1, uint8_t p2, uint8_t *out,
			    size_t cap)
{
	osk_tlv_reader_t rd;
	osk_tlv_reader_init(&rd, sim->last_payload, sim->last_payload_len);

	uint32_t id = 0;
	if (!tlv_u32(&rd, TAG_1, &id)) {
		return 0;
	}

	/* ---- WriteCounter (4.7.1.6) */
	if (ins == INS_WRITE && p1 == P1_COUNTER && p2 == P2_DEFAULT) {
		uint16_t size_bytes = 0;
		const bool have_size = (osk_tlv_get_u16(&rd, TAG_2, &size_bytes) == OSK_OK);

		const uint8_t *val = NULL;
		size_t val_len = 0;
		const bool have_val = (osk_tlv_get(&rd, TAG_3, &val, &val_len) == OSK_OK);

		sim_counter_t *c = counter_find(sim, id);
		if (c == NULL) {
			if (!have_size && !have_val) {
				return only_sw(out, cap, SW_CONDITIONS_NOT_SATISFIED);
			}
			const size_t want = have_size ? (size_t)size_bytes : val_len;
			if (want == 0u || want > 8u) {
				return only_sw(out, cap, SW_WRONG_DATA);
			}
			for (size_t i = 0; i < SIM_MAX_COUNTERS; i++) {
				if (!sim->counters[i].used) {
					c = &sim->counters[i];
					break;
				}
			}
			if (c == NULL) {
				return only_sw(out, cap, SW_CONDITIONS_NOT_SATISFIED);
			}
			c->used = true;
			c->id = id;
			c->len = (uint8_t)want;
			c->value = 0;
			if (!have_val) {
				return only_sw(out, cap, SW_NO_ERROR);
			}
		}

		if (!have_val) {
			/* Kein Wert dabei: um eins hoch. */
			c->value++;
			return only_sw(out, cap, SW_NO_ERROR);
		}
		if (val_len != c->len) {
			/* "must always have the same length as the existing
			 * counter; otherwise the command will return an
			 * error" */
			return only_sw(out, cap, SW_WRONG_DATA);
		}
		uint64_t v = 0;
		for (size_t i = 0; i < val_len; i++) {
			v = (v << 8) | (uint64_t)val[i];
		}
		if (v < c->value) {
			/* "Counters can only be incremented, not
			 * decremented." */
			return only_sw(out, cap, SW_CONDITIONS_NOT_SATISFIED);
		}
		c->value = v;
		return only_sw(out, cap, SW_NO_ERROR);
	}

	/* ---- WriteRSAKey (4.7.1.2), Paar im Baustein erzeugen */
	if (ins == INS_WRITE && p1 == (uint8_t)(P1_KEY_PAIR | P1_RSA)) {
		uint16_t bits = 0;
		if (osk_tlv_get_u16(&rd, TAG_2, &bits) != OSK_OK) {
			return only_sw(out, cap, SW_WRONG_DATA);
		}
		if (bits != RSA_2048 && bits != RSA_3072 && bits != RSA_4096) {
			return only_sw(out, cap, SW_WRONG_DATA);
		}
		sim_rsa_t *k = rsa_find(sim, id);
		if (k == NULL) {
			for (size_t i = 0; i < SIM_MAX_RSA; i++) {
				if (!sim->rsa[i].used) {
					k = &sim->rsa[i];
					break;
				}
			}
		}
		if (k == NULL) {
			return only_sw(out, cap, SW_CONDITIONS_NOT_SATISFIED);
		}
		k->used = true;
		k->id = id;
		k->bits = bits;
		k->crt = (p2 == P2_DEFAULT);
		return only_sw(out, cap, SW_NO_ERROR);
	}

	/* ---- ReadObject (4.7.3.1) */
	if (ins == INS_READ && p1 == P1_DEFAULT && p2 == P2_DEFAULT) {
		const sim_counter_t *c = counter_find(sim, id);
		if (c != NULL) {
			uint8_t val[8];
			for (size_t i = 0; i < c->len; i++) {
				val[c->len - 1u - i] = (uint8_t)((c->value >> (i * 8u)) & 0xFFu);
			}
			return tag1_sw(out, cap, val, c->len);
		}

		const sim_rsa_t *k = rsa_find(sim, id);
		if (k != NULL) {
			uint8_t comp = 0;
			if (osk_tlv_get_u8(&rd, TAG_4, &comp) != OSK_OK) {
				/* Ohne TAG_4 gaebe der Baustein den ganzen
				 * Schluessel heraus - das braucht der
				 * Treiber nicht und der Nachbau kann es
				 * nicht. */
				return only_sw(out, cap, SW_CONDITIONS_NOT_SATISFIED);
			}
			if (comp == RSA_COMP_MOD) {
				uint8_t mod[512];
				const size_t n = k->bits / 8u;
				for (size_t i = 0; i < n; i++) {
					mod[i] = (uint8_t)(0x40u + (i & 0x3Fu));
				}
				/* Hoechstes Bit gesetzt, wie bei einem
				 * echten Modulus dieser Laenge. */
				mod[0] |= 0x80u;
				return tag1_sw(out, cap, mod, n);
			}
			if (comp == RSA_COMP_PUB_EXP) {
				/* Drei Byte, nicht vier: genau der Fall, den
				 * der Vertrag meint, wenn er sagt, dass die
				 * Laenge gemeldet und nicht angenommen
				 * wird. */
				static const uint8_t e[3] = {0x01u, 0x00u, 0x01u};
				return tag1_sw(out, cap, e, sizeof(e));
			}
			return only_sw(out, cap, SW_WRONG_DATA);
		}

		/* Unbekannte Kennung: der Baustein hat das Objekt nicht. */
		return only_sw(out, cap, SW_CONDITIONS_NOT_SATISFIED);
	}

	/* ---- RSASign (4.11.1.1) */
	if (ins == INS_CRYPTO && p1 == P1_SIGNATURE && p2 == P2_SIGN) {
		const sim_rsa_t *k = rsa_find(sim, id);
		if (k == NULL) {
			return 0; /* kein RSA-Schluessel - nicht unsere Sache */
		}
		if (osk_tlv_get_u8(&rd, TAG_2, &sim->last_rsa_algo) != OSK_OK) {
			return only_sw(out, cap, SW_WRONG_DATA);
		}
		uint8_t sig[512];
		const size_t n = k->bits / 8u;
		for (size_t i = 0; i < n; i++) {
			sig[i] = (uint8_t)(0x11u + (i & 0x1Fu));
		}
		return tag1_sw(out, cap, sig, n);
	}

	/* ---- RSADecrypt (4.11.3.2) */
	if (ins == INS_CRYPTO && p1 == P1_RSA && p2 == P2_DECRYPT_ONESHOT) {
		const sim_rsa_t *k = rsa_find(sim, id);
		if (k == NULL) {
			return only_sw(out, cap, SW_CONDITIONS_NOT_SATISFIED);
		}
		if (osk_tlv_get_u8(&rd, TAG_2, &sim->last_rsa_algo) != OSK_OK) {
			return only_sw(out, cap, SW_WRONG_DATA);
		}
		const uint8_t *ct = NULL;
		size_t ct_len = 0;
		if (osk_tlv_get(&rd, TAG_3, &ct, &ct_len) != OSK_OK) {
			return only_sw(out, cap, SW_WRONG_DATA);
		}
		/* Ein echtes Chiffrat ist so lang wie der Modulus. Kuerzer
		 * waere ein Fehler, und den soll der Nachbau zeigen. */
		if (ct_len != (size_t)(k->bits / 8u)) {
			return only_sw(out, cap, SW_WRONG_DATA);
		}
		static const uint8_t msg[15] = "Geheime Post!!";
		return tag1_sw(out, cap, msg, sizeof(msg));
	}

	return 0;
}

/* ------------------------------------------------ Befehl auspacken */

/* Einen gesicherten Befehl auspacken: MAC pruefen, Nutzlast entschluesseln.
 * Das ist der Weg, den der echte Baustein geht - und genau deshalb prueft
 * es den C-MAC des Treibers wirklich nach. */
static bool unwrap_command(se050_sim_t *sim, const uint8_t *apdu, size_t len, uint8_t *body,
			   size_t cap, size_t *body_len)
{
	if (len < 5u + SCP03_MAC_LEN) {
		return false;
	}
	/* Zwei Formen von Lc - ein Byte, oder ein Nullbyte und zwei Byte
	 * (erweiterte Laenge). Der Nachbau muss beide annehmen, weil der
	 * Treiber die erweiterte Form fuer ein RSA-Chiffrat braucht.
	 * Hinten steht noch das Le-Feld, das nicht in den MAC eingeht. */
	size_t head = 5u;
	size_t lcc = apdu[4];
	if (apdu[4] == 0x00u && len > 7u + SCP03_MAC_LEN) {
		head = 7u;
		lcc = ((size_t)apdu[5] << 8) | apdu[6];
	}
	if (lcc < SCP03_MAC_LEN || len < head + lcc) {
		return false;
	}
	const size_t data_len = lcc - SCP03_MAC_LEN;

	uint8_t macin[16 + 1104];
	if (head + data_len > sizeof(macin) - 16u) {
		return false;
	}
	memcpy(&macin[0], sim->scp.chain, 16);
	memcpy(&macin[16], apdu, head + data_len);

	uint8_t mac[16];
	if (osk_aes_cmac(sim->scp.s_mac, macin, 16u + head + data_len, mac) != OSK_OK) {
		return false;
	}
	if (osk_memeq_ct(mac, &apdu[head + data_len], SCP03_MAC_LEN) != 0) {
		return false;
	}
	/* Verkettung weiterdrehen - beide Seiten tun das gleich. */
	memcpy(sim->scp.chain, mac, 16);

	/* Den Zaehler des Befehls festhalten, der Antwort-Startwert haengt
	 * daran. Danach weiterzaehlen. */
	memcpy(sim->cmd_counter, sim->scp.counter, 16);
	for (size_t i = 16; i-- > 0;) {
		if (++sim->scp.counter[i] != 0u) {
			break;
		}
	}

	*body_len = 0;
	if (data_len == 0) {
		return true;
	}
	if ((data_len % 16u) != 0u) {
		return false;
	}

	uint8_t icv[16];
	if (osk_aes_ecb_encrypt_block(sim->scp.s_enc, sim->cmd_counter, icv) != OSK_OK) {
		return false;
	}

	uint8_t plain[1104];
	if (data_len > sizeof(plain)) {
		return false;
	}
	if (osk_aes_cbc_decrypt(sim->scp.s_enc, icv, &apdu[head], plain, data_len) != OSK_OK) {
		return false;
	}

	size_t n = data_len;
	while (n > 0 && plain[n - 1u] == 0x00u) {
		n--;
	}
	if (n == 0 || plain[n - 1u] != 0x80u) {
		return false;
	}
	n--;
	if (n > cap) {
		return false;
	}
	memcpy(body, plain, n);
	*body_len = n;
	return true;
}

/* ------------------------------------------------------- Befehle */

/* Einen Klartext-C-APDU beantworten. Legt die ANTWORT (Nutzlast und
 * Statuswort) in out ab. */
static size_t handle_apdu(se050_sim_t *sim, const uint8_t *apdu, size_t len, uint8_t *out,
			  size_t cap)
{
	static const uint8_t aid[SE050_APPLET_AID_LEN] = SE050_APPLET_AID;

	if (len < 4u) {
		return 0;
	}
	const uint8_t cla = apdu[0];
	const uint8_t ins = apdu[1];

	/* Applet auswaehlen: CLA 0x00, das geht an den Kartenverwalter. */
	if (cla == 0x00u && ins == 0xA4u) {
		if (len < 5u + SE050_APPLET_AID_LEN ||
		    memcmp(&apdu[5], aid, SE050_APPLET_AID_LEN) != 0) {
			out[0] = 0x6Au;
			out[1] = 0x82u;
			return 2u;
		}
		sim->applet_selected = true;

		/* 7 Byte VersionInfo (Abschnitt 4.3.33): Fassung 3.6.0,
		 * dann die Merkmale, dann SecureBox. Merkmale so, dass alle
		 * Kurven gemeldet werden, die wir brauchen. */
		if (cap < 9u) {
			return 0;
		}
		out[0] = 0x03u;
		out[1] = 0x06u;
		out[2] = 0x00u;
		out[3] = (uint8_t)(sim->applet_config >> 8);
		out[4] = (uint8_t)(sim->applet_config & 0xFFu);
		out[5] = 0x00u;
		out[6] = 0x01u;
		out[7] = 0x90u;
		out[8] = 0x00u;
		return 9u;
	}

	if (!sim->applet_selected) {
		out[0] = 0x69u;
		out[1] = 0x85u;
		return 2u;
	}

	/* CreateSession (AN12413 4.5.1.1): Kennung des Anmeldeobjekts herein,
	 * 8 Byte Sitzungskennung hinaus. */
	if (cla == SE050_CLA && ins == INS_MGMT && apdu[3] == P2_SESSION_CREATE) {
		if (len < 5u + 6u) {
			return 0;
		}
		/* TLV[TAG_1] mit 4 Byte Objektkennung. */
		const uint8_t *v = &apdu[7];
		sim->session_obj =
		    ((uint32_t)v[0] << 24) | ((uint32_t)v[1] << 16) | ((uint32_t)v[2] << 8) | v[3];

		for (size_t i = 0; i < sizeof(sim->session_id); i++) {
			sim->session_id[i] = (uint8_t)(0x50u + i);
		}
		sim->session_created = true;
		sim->channel = false;
		osk_scp03_close(&sim->scp);

		if (cap < 12u) {
			return 0;
		}
		out[0] = TAG_1;
		out[1] = (uint8_t)sizeof(sim->session_id);
		memcpy(&out[2], sim->session_id, sizeof(sim->session_id));
		out[10] = 0x90u;
		out[11] = 0x00u;
		return 12u;
	}

	/* CloseSession. */
	if (cla == SE050_CLA && ins == INS_MGMT && apdu[3] == P2_SESSION_CLOSE) {
		sim->session_created = false;
		sim->channel = false;
		osk_scp03_close(&sim->scp);
		out[0] = 0x90u;
		out[1] = 0x00u;
		return 2u;
	}

	/* INITIALIZE UPDATE. */
	if (cla == SE050_CLA && ins == 0x50u) {
		if (!sim->session_created) {
			/* Ohne CreateSession gibt es keine AESKey-Sitzung
			 * (AN12413 3.6.3.2). Genau das soll auffallen. */
			out[0] = 0x69u;
			out[1] = 0x85u;
			return 2u;
		}
		if (len < 5u + 8u) {
			return 0;
		}
		const uint8_t *host_challenge = &apdu[5];

		uint8_t card_cg[8];
		uint8_t host_cg[8];
		if (osk_scp03_derive(&sim->scp, &sim->next_auth_keys, host_challenge,
				     sim->card_challenge, card_cg, host_cg) != OSK_OK) {
			return 0;
		}

		/* Antwort nach Amendment D Tabelle 7-2. */
		if (cap < 10u + 3u + 8u + 8u + 2u) {
			return 0;
		}
		memset(&out[0], 0xAB, 10); /* Streudaten, ungenutzt */
		out[10] = 0x00u;	   /* Schluesselfassung */
		out[11] = 0x03u;	   /* SCP03 */
		out[12] = 0x70u;	   /* i-Parameter */
		memcpy(&out[13], sim->card_challenge, 8);
		memcpy(&out[21], card_cg, 8);
		out[29] = 0x90u;
		out[30] = 0x00u;
		return 31u;
	}

	/* EXTERNAL AUTHENTICATE: MAC mit dem Verkettungswert Null pruefen. */
	if (cla == SE050_CLA_SECURE && ins == 0x82u) {
		if (len != 5u + 16u) {
			out[0] = 0x67u;
			out[1] = 0x00u;
			return 2u;
		}
		uint8_t macin[16 + 13];
		memset(&macin[0], 0, 16);     /* Verkettung ist hier Null */
		memcpy(&macin[16], apdu, 13); /* Kopf plus 8 Byte Kryptogramm */

		uint8_t mac[16];
		if (osk_aes_cmac(sim->scp.s_mac, macin, sizeof(macin), mac) != OSK_OK) {
			return 0;
		}
		if (osk_memeq_ct(mac, &apdu[13], 8) != 0) {
			out[0] = 0x63u;
			out[1] = 0x00u;
			return 2u;
		}

		memcpy(sim->scp.chain, mac, 16);
		memset(sim->scp.counter, 0, 16);
		sim->scp.counter[15] = 1u;
		sim->scp.established = true;
		sim->channel = true;

		out[0] = 0x90u;
		out[1] = 0x00u;
		return 2u;
	}

	/* ProcessSessionCmd (AN12413 4.5.1.3): Sitzungskennung pruefen, dann
	 * den inneren Befehl behandeln. Die Antwort ist die innere Antwort,
	 * ohne eigenen Umschlag (Tabelle 61). */
	if (cla == SE050_CLA && ins == INS_PROCESS) {
		if (!sim->session_created || len < 5u) {
			out[0] = 0x69u;
			out[1] = 0x85u;
			return 2u;
		}
		/* TLV[TAG_SESSION_ID] (8 Byte), dann TLV[TAG_1].
		 *
		 * Der Umschlag kann selbst erweiterte Laenge haben: ein
		 * verpacktes RSA-Chiffrat ist ueber 255 Byte lang. Dann
		 * beginnt die Nutzlast bei 7 und nicht bei 5. Diese eine
		 * Zahl war der Grund, warum die Entschluesselung erst mit
		 * "Sicherheitsstatus" scheiterte - der Nachbau hat die
		 * falschen Bytes in den MAC gerechnet. */
		const uint8_t *p = (apdu[4] == 0x00u && len > 7u) ? &apdu[7] : &apdu[5];
		if (p[0] != TAG_SESSION_ID || p[1] != 8u ||
		    memcmp(&p[2], sim->session_id, 8) != 0) {
			out[0] = 0x69u;
			out[1] = 0x85u;
			return 2u;
		}
		p += 10;
		if (p[0] != TAG_1) {
			out[0] = 0x6Au;
			out[1] = 0x80u;
			return 2u;
		}
		size_t il;
		if (p[1] < 0x80u) {
			il = p[1];
			p += 2;
		} else if (p[1] == 0x82u) {
			il = ((size_t)p[2] << 8) | p[3];
			p += 4;
		} else {
			out[0] = 0x6Au;
			out[1] = 0x80u;
			return 2u;
		}

		if (sim->channel) {
			/* Im Kanal: innen auspacken, behandeln, wieder
			 * einpacken. Das prueft den C-MAC des Treibers
			 * wirklich nach. */
			uint8_t inner[1104];
			size_t inner_len = 0;
			if (!unwrap_command(sim, p, il, inner, sizeof(inner), &inner_len)) {
				out[0] = 0x69u;
				out[1] = 0x82u;
				return 2u;
			}
			uint8_t rebuilt[1112];
			rebuilt[0] = SE050_CLA;
			rebuilt[1] = p[1];
			rebuilt[2] = p[2];
			rebuilt[3] = p[3];
			size_t rl = 4u;
			if (inner_len > 0xFFu) {
				rebuilt[4] = 0x00u;
				rebuilt[5] = (uint8_t)(inner_len >> 8);
				rebuilt[6] = (uint8_t)(inner_len & 0xFFu);
				memcpy(&rebuilt[7], inner, inner_len);
				rl = 7u + inner_len;
			} else if (inner_len > 0) {
				rebuilt[4] = (uint8_t)inner_len;
				memcpy(&rebuilt[5], inner, inner_len);
				rl = 5u + inner_len;
			}

			uint8_t reply[1112];
			const size_t rn = handle_apdu(sim, rebuilt, rl, reply, sizeof(reply));
			if (rn < 2u) {
				return 0;
			}
			const uint16_t isw =
			    (uint16_t)(((uint16_t)reply[rn - 2u] << 8) | reply[rn - 1u]);
			return wrap_reply(sim, reply, rn - 2u, isw, out, cap);
		}

		/* Noch kein Kanal: der innere Befehl geht offen durch - das
		 * sind genau INITIALIZE UPDATE und EXTERNAL AUTHENTICATE. */
		return handle_apdu(sim, p, il, out, cap);
	}

	/* Alles andere ist ein normaler Befehl. */
	sim->commands++;
	sim->last_ins = ins;
	sim->last_p1 = apdu[2];
	sim->last_p2 = apdu[3];

	/* Die Klartext-Nutzlast festhalten: nur so kann ein Test pruefen,
	 * was wirklich im Befehl stand - bei PUT KEY ist genau das der
	 * Punkt. */
	sim->last_payload_len = 0;
	if (len > 5u) {
		size_t phead = 5u;
		size_t pl = apdu[4];
		if (apdu[4] == 0x00u && len > 7u) {
			phead = 7u;
			pl = ((size_t)apdu[5] << 8) | apdu[6];
		}
		if (pl > len - phead) {
			pl = len - phead;
		}
		if (pl > sizeof(sim->last_payload)) {
			pl = sizeof(sim->last_payload);
		}
		memcpy(sim->last_payload, &apdu[phead], pl);
		sim->last_payload_len = pl;
	}

	if (sim->objects_live) {
		const size_t on = handle_object(sim, apdu[1], apdu[2], apdu[3], out, cap);
		if (on > 0) {
			return on;
		}
	}

	const size_t n = sim->next_body_len;
	const uint16_t sw = sim->next_sw;
	if (cap < n + 2u) {
		return 0;
	}
	if (n > 0) {
		memcpy(out, sim->next_body, n);
	}
	out[n] = (uint8_t)(sw >> 8);
	out[n + 1u] = (uint8_t)(sw & 0xFFu);
	return n + 2u;
}

/* ------------------------------------------------- Rahmen beantworten */

static size_t sim_respond(void *ctx, const uint8_t *frame, size_t len, uint8_t *out, size_t cap)
{
	se050_sim_t *sim = (se050_sim_t *)ctx;

	if (osk_se050_t1_check(frame, len) != OSK_OK) {
		return 0;
	}
	const uint8_t pcb = frame[1];
	const uint8_t *inf = &frame[SE050_T1_PROLOGUE_LEN];
	const uint16_t inf_len = (uint16_t)(((uint16_t)frame[2] << 8) | frame[3]);

	/* S-Block: mit demselben Code und gesetztem Antwortbit quittieren. */
	if (SE050_PCB_IS_S(pcb)) {
		if (pcb & SE050_PCB_S_RESPONSE) {
			/* Bestaetigung des Wirts - darauf kommt nichts. */
			return 0;
		}
		const uint8_t code = pcb & SE050_PCB_S_CODE_MASK;
		const uint8_t rpcb = (uint8_t)(SE050_PCB_TYPE_S | SE050_PCB_S_RESPONSE | code);
		if (code == SE050_S_SWR) {
			/* Zuruecksetzen heisst: Sitzung weg, Applet nicht
			 * mehr ausgewaehlt. Die festen Schluessel und die
			 * Zufallszahl bleiben - die gehoeren dem Baustein.
			 * S(SWR response) traegt keine Daten. */
			osk_scp03_close(&sim->scp);
			sim->applet_selected = false;
			sim->channel = false;
			sim->session_created = false;
			memset(sim->cmd_counter, 0, sizeof(sim->cmd_counter));
			return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST, rpcb, NULL, 0);
		}
		if (code == SE050_S_CIP) {
			/* Schnittstellenwerte nach GPC_SPE_172 Abschnitt 4.3:
			 * PVER | LenIIN | IIN | PLID | LenPLP | PLP |
			 * LenDLLP | DLLP | LenHB | HB.
			 * DLLP traegt BWT (500 ms) und IFSC (254 Byte). */
			static const uint8_t cip[] = {
			    0x01,	/* PVER            */
			    0x00,	/* LenIIN = 0      */
			    0x02,	/* PLID = I2C      */
			    0x01, 0xAA, /* LenPLP = 1, PLP */
			    0x04,	/* LenDLLP = 4     */
			    0x01, 0xF4, /* BWT  = 500 ms   */
			    0x00, 0xFE, /* IFSC = 254      */
			    0x00,	/* LenHB = 0       */
			};
			return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST, rpcb, cip,
						  (uint16_t)sizeof(cip));
		}
		if (code == SE050_S_IFS) {
			/* "Same INF field as preceding S(IFS request)"
			 * (Tabelle 4-5) - also unveraendert zurueck. */
			return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST, rpcb, inf, inf_len);
		}
		if (code == SE050_S_RESYNC) {
			osk_scp03_close(&sim->scp);
			sim->channel = false;
		}
		return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST, rpcb, NULL, 0);
	}

	/* R-Block vom Wirt: Quittung, darauf antwortet der Baustein mit dem
	 * naechsten Block - der Nachbau kettet nichts, also nichts. */
	if (SE050_PCB_IS_R(pcb)) {
		return 0;
	}

	/* I-Block: darin steckt die APDU. Laengere Befehle kommen in einer
	 * Kette - der Treiber zerlegt nach dem IFSC, das der Nachbau im
	 * CIP genannt hat. Jeder Zwischenblock wird mit einem R-Block ohne
	 * Fehlergrund quittiert, erst der letzte wird behandelt.
	 *
	 * Ohne das waere ein RSA-Chiffrat nicht zustellbar: 512 Byte gehen
	 * bei IFSC = 254 in drei Bloecke. */
	if (pcb & SE050_PCB_I_MORE) {
		if (sim->asm_len + inf_len > sizeof(sim->asm_buf)) {
			sim->asm_len = 0;
			return 0;
		}
		memcpy(&sim->asm_buf[sim->asm_len], inf, inf_len);
		sim->asm_len += inf_len;
		return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST,
					  SE050_PCB_TYPE_R | SE050_PCB_R_ERR_NONE, NULL, 0);
	}

	/* Der I-Block traegt eine Klartext-APDU. Die SCP03-Sicherung liegt
	 * eine Ebene weiter innen, im TAG_1 eines ProcessSessionCmd - das
	 * packt handle_apdu() selbst aus. */
	/* Den letzten Block an das Gesammelte haengen - ohne Kette ist das
	 * Gesammelte leer und es bleibt beim Block selbst. */
	uint8_t whole[1104];
	const uint8_t *apdu = inf;
	size_t apdu_len = inf_len;
	if (sim->asm_len > 0) {
		if (sim->asm_len + inf_len > sizeof(whole)) {
			sim->asm_len = 0;
			return 0;
		}
		memcpy(whole, sim->asm_buf, sim->asm_len);
		memcpy(&whole[sim->asm_len], inf, inf_len);
		apdu_len = sim->asm_len + inf_len;
		apdu = whole;
		sim->asm_len = 0;
	}

	uint8_t body[1120];
	const size_t body_len = handle_apdu(sim, apdu, apdu_len, body, sizeof(body));
	if (body_len == 0 || body_len > SE050_T1_MAX_INF) {
		return 0;
	}
	/* uint16_t und nicht uint8_t: ein RSA-4096-Modulus ist 512 Byte
	 * lang, und ein Schnitt auf 255 waere ein Fehler des Nachbaus, der
	 * wie ein Fehler des Treibers aussieht. Der Treiber kuendigt
	 * SE050_IFSD = 1024 an, so viel darf in einem Block kommen. */
	return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST, SE050_PCB_TYPE_I, body,
				  (uint16_t)body_len);
}

void sim_attach(se050_sim_t *sim, fake_bus_t *fb)
{
	memset(sim, 0, sizeof(*sim));
	sim->keys = SIM_KEYS;
	sim->next_auth_keys = SIM_KEYS;
	/* Vorgabe: alles an, was der Treiber abfragt, und FIPS-Modus AUS
	 * (Bit 0x1000 gesetzt heisst "FIPS deaktiviert"). */
	sim->applet_config = 0x100Fu;
	sim->next_sw = SW_NO_ERROR;
	for (size_t i = 0; i < sizeof(sim->card_challenge); i++) {
		sim->card_challenge[i] = (uint8_t)(0xC0u + i);
	}
	fb_set_responder(fb, sim_respond, sim);
}
