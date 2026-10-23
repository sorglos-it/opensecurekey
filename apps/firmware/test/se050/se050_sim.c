#include <string.h>
#include "se050_sim.h"
#include "se050_t1.h"
#include "se050_t1_wire.h"
#include "se050_crypto.h"
#include "se050_cmd.h"

static const osk_scp03_static_keys_t SIM_KEYS = {
	.enc = { 0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,
		 0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F },
	.mac = { 0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,
		 0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F },
};

const osk_scp03_static_keys_t *sim_keys(void)
{
	return &SIM_KEYS;
}

void sim_set_reply(se050_sim_t *sim, const uint8_t *body, size_t len,
		   uint16_t sw)
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
static size_t wrap_reply(se050_sim_t *sim, const uint8_t *body, size_t body_len,
			 uint16_t sw, uint8_t *out, size_t cap)
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
	uint8_t plain[272];
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
		if (osk_aes_cbc_encrypt(sim->scp.s_enc, icv, plain, out,
					padded) != OSK_OK) {
			return 0;
		}
	}

	uint8_t macin[16 + 272 + 2];
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

/* ------------------------------------------------ Befehl auspacken */

/* Einen gesicherten Befehl auspacken: MAC pruefen, Nutzlast entschluesseln.
 * Das ist der Weg, den der echte Baustein geht - und genau deshalb prueft
 * es den C-MAC des Treibers wirklich nach. */
static bool unwrap_command(se050_sim_t *sim, const uint8_t *apdu, size_t len,
			   uint8_t *body, size_t cap, size_t *body_len)
{
	if (len < 5u + SCP03_MAC_LEN) {
		return false;
	}
	/* Hinten steht noch das Le-Byte, das nicht in den MAC eingeht. */
	size_t lcc = apdu[4];
	if (len < 5u + lcc) {
		return false;
	}
	const size_t data_len = lcc - SCP03_MAC_LEN;

	uint8_t macin[16 + 300];
	if (5u + data_len > sizeof(macin) - 16u) {
		return false;
	}
	memcpy(&macin[0], sim->scp.chain, 16);
	memcpy(&macin[16], apdu, 5u + data_len);

	uint8_t mac[16];
	if (osk_aes_cmac(sim->scp.s_mac, macin, 16u + 5u + data_len, mac) != OSK_OK) {
		return false;
	}
	if (osk_memeq_ct(mac, &apdu[5 + data_len], SCP03_MAC_LEN) != 0) {
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
	if (osk_aes_ecb_encrypt_block(sim->scp.s_enc, sim->cmd_counter,
				      icv) != OSK_OK) {
		return false;
	}

	uint8_t plain[300];
	if (data_len > sizeof(plain)) {
		return false;
	}
	if (osk_aes_cbc_decrypt(sim->scp.s_enc, icv, &apdu[5], plain,
				data_len) != OSK_OK) {
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
static size_t handle_apdu(se050_sim_t *sim, const uint8_t *apdu, size_t len,
			  uint8_t *out, size_t cap)
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
		out[3] = 0x0Fu;   /* ECDAA, ECDSA/ECDH, EdDSA, DH_MONT */
		out[4] = 0xFFu;
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

	/* INITIALIZE UPDATE. */
	if (cla == SE050_CLA && ins == 0x50u) {
		if (len < 5u + 8u) {
			return 0;
		}
		const uint8_t *host_challenge = &apdu[5];

		uint8_t card_cg[8];
		uint8_t host_cg[8];
		if (osk_scp03_derive(&sim->scp, &sim->keys, host_challenge,
				     sim->card_challenge, card_cg,
				     host_cg) != OSK_OK) {
			return 0;
		}

		/* Antwort nach Amendment D Tabelle 7-2. */
		if (cap < 10u + 3u + 8u + 8u + 2u) {
			return 0;
		}
		memset(&out[0], 0xAB, 10);     /* Streudaten, ungenutzt */
		out[10] = 0x00u;               /* Schluesselfassung */
		out[11] = 0x03u;               /* SCP03 */
		out[12] = 0x70u;               /* i-Parameter */
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
		memset(&macin[0], 0, 16);      /* Verkettung ist hier Null */
		memcpy(&macin[16], apdu, 13);  /* Kopf plus 8 Byte Kryptogramm */

		uint8_t mac[16];
		if (osk_aes_cmac(sim->scp.s_mac, macin, sizeof(macin),
				 mac) != OSK_OK) {
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

	/* Alles andere ist ein normaler Befehl. */
	sim->commands++;
	sim->last_ins = ins;
	sim->last_p1 = apdu[2];
	sim->last_p2 = apdu[3];

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

static size_t sim_respond(void *ctx, const uint8_t *frame, size_t len,
			  uint8_t *out, size_t cap)
{
	se050_sim_t *sim = (se050_sim_t *)ctx;

	if (osk_se050_t1_check(frame, len) != OSK_OK) {
		return 0;
	}
	const uint8_t pcb = frame[1];
	const uint8_t *inf = &frame[SE050_T1_PROLOGUE_LEN];
	const uint8_t inf_len = frame[2];

	/* S-Block: mit demselben Code und gesetztem Antwortbit quittieren. */
	if (SE050_PCB_IS_S(pcb)) {
		if (pcb & SE050_PCB_S_RESPONSE) {
			/* Bestaetigung des Wirts - darauf kommt nichts. */
			return 0;
		}
		const uint8_t code = pcb & SE050_PCB_S_CODE_MASK;
		const uint8_t rpcb = (uint8_t)(SE050_PCB_TYPE_S |
					       SE050_PCB_S_RESPONSE | code);
		if (code == SE050_S_IF_SOFT_RESET) {
			/* Zuruecksetzen heisst: Sitzung weg, Applet nicht
			 * mehr ausgewaehlt. Die festen Schluessel und die
			 * Zufallszahl bleiben - die gehoeren dem Baustein. */
			osk_scp03_close(&sim->scp);
			sim->applet_selected = false;
			sim->channel = false;
			memset(sim->cmd_counter, 0, sizeof(sim->cmd_counter));

			/* Kennblock - der Treiber wertet ihn nicht aus. */
			static const uint8_t atr[] = { 0x00, 0xA0, 0x00, 0x00 };
			return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST,
						  rpcb, atr, sizeof(atr));
		}
		if (code == SE050_S_RESYNC) {
			osk_scp03_close(&sim->scp);
			sim->channel = false;
		}
		return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST, rpcb,
					  NULL, 0);
	}

	/* R-Block vom Wirt: Quittung, darauf antwortet der Baustein mit dem
	 * naechsten Block - der Nachbau kettet nichts, also nichts. */
	if (SE050_PCB_IS_R(pcb)) {
		return 0;
	}

	/* I-Block: darin steckt die APDU. Kettenbildung auf dem Hinweg
	 * braucht der Nachbau nicht - die Tests hier bleiben unter der
	 * Blockgrenze. */
	if (pcb & SE050_PCB_I_MORE) {
		return 0;
	}

	uint8_t body[320];
	size_t body_len = 0;
	const uint8_t *plain = inf;
	size_t plain_len = inf_len;

	if (sim->channel) {
		uint8_t inner[300];
		size_t inner_len = 0;
		if (!unwrap_command(sim, inf, inf_len, inner, sizeof(inner),
				    &inner_len)) {
			/* Falscher MAC: der echte Baustein reisst dann den
			 * Kanal ab. Hier nur ein Fehlerstatus. */
			static const uint8_t err[] = { 0x69, 0x82 };
			return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST,
						  SE050_PCB_TYPE_I, err,
						  sizeof(err));
		}
		/* Die ausgepackte Nutzlast ist der Klartext-Befehl ohne Kopf;
		 * den Kopf liefert der aeussere Rahmen. */
		uint8_t rebuilt[310];
		rebuilt[0] = SE050_CLA;
		rebuilt[1] = inf[1];
		rebuilt[2] = inf[2];
		rebuilt[3] = inf[3];
		size_t rl = 4u;
		if (inner_len > 0) {
			rebuilt[4] = (uint8_t)inner_len;
			memcpy(&rebuilt[5], inner, inner_len);
			rl = 5u + inner_len;
		}

		uint8_t reply[300];
		const size_t rn = handle_apdu(sim, rebuilt, rl, reply,
					      sizeof(reply));
		if (rn < 2u) {
			return 0;
		}
		const uint16_t sw = (uint16_t)(((uint16_t)reply[rn - 2u] << 8) |
					       reply[rn - 1u]);
		body_len = wrap_reply(sim, reply, rn - 2u, sw, body,
				      sizeof(body));
		if (body_len == 0) {
			return 0;
		}
		return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST,
					  SE050_PCB_TYPE_I, body,
					  (uint8_t)body_len);
	}

	body_len = handle_apdu(sim, plain, plain_len, body, sizeof(body));
	if (body_len == 0) {
		return 0;
	}
	return osk_se050_t1_build(out, cap, SE050_NAD_TO_HOST,
				  SE050_PCB_TYPE_I, body, (uint8_t)body_len);
}

void sim_attach(se050_sim_t *sim, fake_bus_t *fb)
{
	memset(sim, 0, sizeof(*sim));
	sim->keys = SIM_KEYS;
	sim->next_sw = SW_NO_ERROR;
	for (size_t i = 0; i < sizeof(sim->card_challenge); i++) {
		sim->card_challenge[i] = (uint8_t)(0xC0u + i);
	}
	fb_set_responder(fb, sim_respond, sim);
}
