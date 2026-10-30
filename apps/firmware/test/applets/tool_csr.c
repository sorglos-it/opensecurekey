/* Werkzeug fuer den Beleg mit einem FREMDEN Programm.
 *
 * WOZU: eine selbstgebaute Zertifikatsanfrage, die nur das eigene
 * Leseprogramm versteht, ist nichts wert. Dieses Werkzeug baut die Anfrage
 * mit DEMSELBEN Firmware-Code wie das Geraet (osk_csr.c) und legt sie als
 * Datei ab, damit openssl sie liest und die Unterschrift bestaetigt.
 * Gesteuert wird es von csr_openssl.py; dort steht der ganze Ablauf.
 *
 * WARUM ZWEI AUFRUFE: unterschrieben wird im Chip, und der Chip ist nicht
 * da. Also wird in zwei Schritten gearbeitet:
 *
 *   1. `info` baut den unterschriebenen Teil (CertificationRequestInfo)
 *      und schreibt ihn heraus. openssl unterschreibt dann GENAU diese
 *      Bytes mit dem privaten Schluessel, den es selbst erzeugt hat.
 *   2. `der` baut die ganze Anfrage, legt dabei die Unterschrift von
 *      openssl in die Attrappe (mocks.h) und schreibt DER und PEM.
 *
 * WAS DAMIT BEWIESEN IST: der Aufbau der Anfrage und dass die Unterschrift
 * ueber den richtigen Teil geht - beides von einem Programm bestaetigt,
 * das nichts von diesem Projekt weiss.
 * WAS DAMIT NICHT BEWIESEN IST: dass der SE051C2 diese Unterschrift
 * liefert. Der Chip ist nicht da. Siehe docs/csr.md.
 *
 * Dieses Werkzeug ist KEINE Pruefung und laeuft nicht bei `make` mit: es
 * schreibt Dateien und braucht openssl daneben. `make openssl` ruft es. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "osk_csr.h"
#include "osk_der.h"
#include "osk_spki.h"
#include "osk_se050.h"
#include "osk_rsa_card.h"
#include "mocks.h"

#define KEY_ID 0x43535201u

static uint8_t g_buf[OSK_CSR_MAX];
static uint8_t g_pub[1024];
static uint8_t g_sig[1024];
static char g_pem[OSK_CSR_PEM_MAX];

static size_t read_file(const char *path, uint8_t *out, size_t cap)
{
	FILE *f = fopen(path, "rb");
	size_t n;

	if (f == NULL) {
		fprintf(stderr, "kann '%s' nicht lesen\n", path);
		exit(2);
	}
	n = fread(out, 1u, cap, f);
	fclose(f);
	return n;
}

static void write_file(const char *path, const void *p, size_t len)
{
	FILE *f = fopen(path, "wb");

	if (f == NULL) {
		fprintf(stderr, "kann '%s' nicht schreiben\n", path);
		exit(2);
	}
	if (fwrite(p, 1u, len, f) != len) {
		fprintf(stderr, "'%s' nicht vollstaendig geschrieben\n", path);
		exit(2);
	}
	fclose(f);
}

/* Die Schluesselart aus dem Namen. Die RSA-Laenge steckt im Namen, weil
 * sie bestimmt, wo im Datentraeger der Modulus endet und der Exponent
 * anfaengt. */
static bool spec_parse(const char *spec, osk_spki_key_t *type_out, size_t *mod_bytes_out,
		       osk_rsa_bits_t *bits_out)
{
	*mod_bytes_out = 0u;
	*bits_out = OSK_RSA_2048;
	if (strcmp(spec, "p256") == 0) {
		*type_out = OSK_SPKI_P256;
		return true;
	}
	if (strcmp(spec, "p384") == 0) {
		*type_out = OSK_SPKI_P384;
		return true;
	}
	*type_out = OSK_SPKI_RSA;
	if (strcmp(spec, "rsa2048") == 0) {
		*mod_bytes_out = 256u;
		*bits_out = OSK_RSA_2048;
		return true;
	}
	if (strcmp(spec, "rsa3072") == 0) {
		*mod_bytes_out = 384u;
		*bits_out = OSK_RSA_3072;
		return true;
	}
	if (strcmp(spec, "rsa4096") == 0) {
		*mod_bytes_out = 512u;
		*bits_out = OSK_RSA_4096;
		return true;
	}
	return false;
}

/* Der Schluessel in der Attrappe, mit dem echten oeffentlichen Teil von
 * openssl. Bei RSA stehen im Datentraeger erst der Modulus (so lang, wie
 * die Laenge im Namen sagt) und dahinter der Exponent. */
static void set_up_key(osk_spki_key_t type, size_t mod_bytes, osk_rsa_bits_t bits,
		       const char *pub_path)
{
	osk_key_policy_t pol;
	const size_t n = read_file(pub_path, g_pub, sizeof(g_pub));

	memset(&pol, 0, sizeof(pol));
	mock_reset_all();

	if (type == OSK_SPKI_RSA) {
		/* Ohne RSA gebaut (oder ein Chip ohne RSA): das ist KEIN
		 * Fehler, sondern der andere Zweig der Kconfig-Option. Der
		 * Ausstiegscode 3 sagt csr_openssl.py, dass dieser Fall
		 * uebersprungen wird - und das steht dann auch im Bericht.
		 * Ein uebersprungener Beleg darf nicht aussehen wie ein
		 * erbrachter. */
		if (!osk_rsa_available()) {
			fprintf(stderr, "RSA ist in dieser Fassung nicht vorhanden\n");
			exit(3);
		}
		if (n <= mod_bytes) {
			fprintf(stderr, "'%s' ist zu kurz fuer Modulus und Exponent\n", pub_path);
			exit(2);
		}
		if (osk_se050_rsa_key_create(KEY_ID, bits, &pol) != OSK_OK) {
			fprintf(stderr, "RSA-Schluessel laesst sich nicht anlegen\n");
			exit(2);
		}
		mock_se050_set_rsa_public(g_pub, mod_bytes, g_pub + mod_bytes, n - mod_bytes);
		return;
	}
	if (osk_se050_key_create(KEY_ID, (type == OSK_SPKI_P384) ? OSK_CURVE_P384 : OSK_CURVE_P256,
				 &pol) != OSK_OK) {
		fprintf(stderr, "Schluessel laesst sich nicht anlegen\n");
		exit(2);
	}
	mock_se050_set_public(g_pub, n);
}

/* Der Betreff und die Erweiterungen. Absichtlich reichlich: so steht in
 * der Ausgabe von `openssl req -text` jedes Stueck, das gebaut wird, und
 * jedes davon kann am Ergebnis nachgelesen werden. */
static void fill_req(osk_csr_req_t *req, osk_spki_key_t type)
{
	memset(req, 0, sizeof(*req));
	req->key = KEY_ID;
	req->key_type = type;
	req->cn = "key.example.org";
	req->country = "DE";
	req->org = "Sorglos";
	req->org_unit = "openSecureKey PIV 9A";
	req->serial_number = "0123456789ABCDEF";
	req->san[0].kind = OSK_CSR_SAN_DNS;
	req->san[0].name = "key.example.org";
	req->san[1].kind = OSK_CSR_SAN_DNS;
	req->san[1].name = "zweiter.example.org";
	req->san[2].kind = OSK_CSR_SAN_EMAIL;
	req->san[2].name = "thomas@example.org";
	req->san[3].kind = OSK_CSR_SAN_URI;
	req->san[3].name = "https://example.org/key";
	req->san_count = 4u;
	req->key_usage = OSK_KU_DIGITAL_SIGNATURE | OSK_KU_KEY_ENCIPHERMENT;
	req->eku[0] = OSK_CSR_EKU_CLIENT_AUTH;
	req->eku[1] = OSK_CSR_EKU_EMAIL_PROTECTION;
	req->eku_count = 2u;
}

int main(int argc, char **argv)
{
	osk_csr_req_t req;
	osk_spki_key_t type;
	osk_rsa_bits_t bits;
	size_t mod_bytes = 0;
	size_t len = 0;
	osk_result_t rc;

	if (argc < 5) {
		fprintf(stderr,
			"Aufruf:\n"
			"  tool_csr info <art> <pubdatei> <ausgabe.tbs>\n"
			"  tool_csr der  <art> <pubdatei> <sigdatei> <ausgabe.der> <ausgabe.pem>\n"
			"<art>: p256 p384 rsa2048 rsa3072 rsa4096\n");
		return 2;
	}
	if (!spec_parse(argv[2], &type, &mod_bytes, &bits)) {
		fprintf(stderr, "unbekannte Art '%s'\n", argv[2]);
		return 2;
	}
	set_up_key(type, mod_bytes, bits, argv[3]);
	fill_req(&req, type);

	if (strcmp(argv[1], "info") == 0) {
		rc = osk_csr_info(&req, g_buf, sizeof(g_buf), &len);
		if (rc != OSK_OK) {
			fprintf(stderr, "osk_csr_info: %s\n", osk_result_text(rc));
			return 1;
		}
		write_file(argv[4], g_buf, len);
		printf("tbs %zu Byte\n", len);
		return 0;
	}

	if (strcmp(argv[1], "der") == 0) {
		size_t pem_len = 0;
		size_t sig_len;

		if (argc < 7) {
			fprintf(stderr, "der: sigdatei, der und pem fehlen\n");
			return 2;
		}
		sig_len = read_file(argv[4], g_sig, sizeof(g_sig));
		mock_se050_set_signature(g_sig, sig_len);
		rc = osk_csr_der(&req, g_buf, sizeof(g_buf), &len);
		if (rc != OSK_OK) {
			fprintf(stderr, "osk_csr_der: %s\n", osk_result_text(rc));
			return 1;
		}
		write_file(argv[5], g_buf, len);
		rc = osk_csr_pem(g_buf, len, g_pem, sizeof(g_pem), &pem_len);
		if (rc != OSK_OK) {
			fprintf(stderr, "osk_csr_pem: %s\n", osk_result_text(rc));
			return 1;
		}
		write_file(argv[6], g_pem, pem_len);
		printf("der %zu Byte, pem %zu Byte\n", len, pem_len);
		return 0;
	}

	fprintf(stderr, "unbekannter Befehl '%s'\n", argv[1]);
	return 2;
}
