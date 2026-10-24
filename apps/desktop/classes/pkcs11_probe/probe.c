/* Die PKCS#11-Bibliothek von aussen durchfahren - so, wie Firefox es tut.
 *
 * Dieses Programm laedt die Bibliothek zur Laufzeit, holt sich die Tabelle
 * ueber C_GetFunctionList und geht den ganzen Weg: anmelden, Objekte suchen,
 * Merkmale lesen, unterschreiben. Die Unterschrift und die unterschriebenen
 * Daten legt es als Dateien ab, damit ein FREMDES Programm sie nachprueft -
 * openssl. Eine Unterschrift, die nur unser eigener Code fuer gueltig
 * erklaert, beweist nichts.
 *
 * Eigener Code, keine Fremdbibliothek: geladen wird mit dlopen bzw.
 * LoadLibrary, die Typen kommen aus ../pkcs11/pkcs11_osk.h.
 *
 * Bauen und laufen lassen: siehe docs/anbindungen.md.
 *
 * Rueckgabe: 0 wenn alles gelaufen ist, sonst die Nummer des Schritts, der
 * gescheitert ist. Eine 0 ohne die Zeile "probe: ok" am Ende gilt nicht.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../pkcs11/pkcs11_osk.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define LIB_HANDLE HMODULE
#define LIB_OPEN(path) LoadLibraryA(path)
#define LIB_SYM(h, name) ((void *)GetProcAddress((h), (name)))
#define LIB_ERROR() "LoadLibrary/GetProcAddress fehlgeschlagen"
#else
#include <dlfcn.h>
#define LIB_HANDLE void *
#define LIB_OPEN(path) dlopen((path), RTLD_NOW)
#define LIB_SYM(h, name) dlsym((h), (name))
#define LIB_ERROR() dlerror()
#endif

/* Die Werte, die dieses Programm braucht - aus der Spezifikation. */
#define CKA_CLASS_V        0x00000000UL
#define CKA_LABEL_V        0x00000003UL
#define CKA_VALUE_V        0x00000011UL
#define CKA_KEY_TYPE_V     0x00000100UL
#define CKA_ID_V           0x00000102UL
#define CKA_SENSITIVE_V    0x00000103UL
#define CKA_SIGN_V         0x00000108UL
#define CKA_EXTRACTABLE_V  0x00000162UL
#define CKA_MODULUS_V      0x00000120UL
#define CKA_PUBLIC_EXP_V   0x00000122UL

#define CKO_PRIVATE_KEY_V  0x00000003UL
#define CKO_PUBLIC_KEY_V   0x00000002UL
#define CKK_RSA_V          0x00000000UL

#define CKU_USER_V         1UL
#define CKF_SERIAL_V       0x00000004UL
#define CKM_SHA256_RSA_V   0x00000040UL

#define CKR_OK_V                 0x00000000UL
#define CKR_ATTR_SENSITIVE_V     0x00000011UL
#define CKR_ATTR_TYPE_INVALID_V  0x00000012UL

typedef CK_RV (*get_list_fn)(CK_FUNCTION_LIST_PTR_PTR);

static CK_FUNCTION_LIST_PTR fns;

/* step haelt fest, wo wir sind - der Rueckgabewert bei einem Fehler. */
static int step;

static int fail(const char *what, CK_RV rv)
{
	fprintf(stderr, "probe: Schritt %d (%s) gab 0x%lx\n", step, what,
	        (unsigned long)rv);
	return step;
}

/* text gibt ein Feld fester Laenge ohne die Leerzeichen am Ende aus. */
static void text(const char *label, const CK_UTF8CHAR *field, size_t size)
{
	size_t end = size;
	while (end > 0 && (field[end - 1] == ' ' || field[end - 1] == 0)) {
		end--;
	}
	printf("  %-14s %.*s\n", label, (int)end, (const char *)field);
}

/* write_file legt eine Datei ab - fuer openssl zum Nachpruefen. */
static int write_file(const char *path, const void *data, size_t length)
{
	FILE *handle = fopen(path, "wb");
	if (!handle) {
		fprintf(stderr, "probe: %s laesst sich nicht schreiben\n", path);
		return 0;
	}
	if (length && fwrite(data, 1, length, handle) != length) {
		fclose(handle);
		fprintf(stderr, "probe: %s unvollstaendig geschrieben\n", path);
		return 0;
	}
	fclose(handle);
	return 1;
}

int main(int argc, char **argv)
{
	LIB_HANDLE library;
	get_list_fn get_list;
	CK_RV rv;
	CK_INFO info;
	CK_SLOT_ID slots[4];
	CK_ULONG slot_count = 4;
	CK_TOKEN_INFO token;
	CK_MECHANISM_TYPE mechanisms[32];
	CK_ULONG mechanism_count = 32;
	CK_SESSION_HANDLE session = 0;
	CK_OBJECT_HANDLE privates[8], publics[8];
	CK_ULONG private_count = 0, public_count = 0;
	CK_ATTRIBUTE search[2];
	CK_OBJECT_CLASS want_class;
	CK_ATTRIBUTE query[4];
	CK_BYTE label[64], id[16], sensitive, extractable;
	CK_ULONG key_type = 0;
	CK_BYTE modulus[1024], exponent[16];
	CK_ATTRIBUTE public_query[2];
	CK_MECHANISM mechanism;
	CK_BYTE signature[1024];
	CK_ULONG signature_len = 0;
	const char *path, *out_dir;
	char out_path[1024];
	static const char payload[] =
	    "OpenSecureKey: dieser Text wurde ueber PKCS#11 unterschrieben.\n";
	CK_ULONG index;

	if (argc < 3) {
		fprintf(stderr,
		        "Aufruf: probe <bibliothek> <ausgabeordner> [pin]\n");
		return 1;
	}
	path = argv[1];
	out_dir = argv[2];
	const char *pin = (argc > 3) ? argv[3] : "123456";

	step = 1;
	library = LIB_OPEN(path);
	if (!library) {
		fprintf(stderr, "probe: %s laesst sich nicht laden: %s\n", path,
		        LIB_ERROR());
		return step;
	}

	step = 2;
	get_list = (get_list_fn)LIB_SYM(library, "C_GetFunctionList");
	if (!get_list) {
		fprintf(stderr, "probe: C_GetFunctionList fehlt: %s\n", LIB_ERROR());
		return step;
	}
	rv = get_list(&fns);
	if (rv != CKR_OK_V || !fns) {
		return fail("C_GetFunctionList", rv);
	}
	printf("Tabelle: Fassung %u.%u\n", fns->version.major, fns->version.minor);

	step = 3;
	rv = fns->C_Initialize(NULL);
	if (rv != CKR_OK_V) {
		return fail("C_Initialize", rv);
	}

	step = 4;
	rv = fns->C_GetInfo(&info);
	if (rv != CKR_OK_V) {
		return fail("C_GetInfo", rv);
	}
	printf("Bibliothek:\n");
	text("Hersteller", info.manufacturerID, sizeof(info.manufacturerID));
	text("Beschreibung", info.libraryDescription, sizeof(info.libraryDescription));
	printf("  %-14s %u.%u\n", "Cryptoki", info.cryptokiVersion.major,
	       info.cryptokiVersion.minor);

	step = 5;
	rv = fns->C_GetSlotList(CK_TRUE, slots, &slot_count);
	if (rv != CKR_OK_V) {
		return fail("C_GetSlotList", rv);
	}
	if (slot_count != 1) {
		fprintf(stderr, "probe: %lu Steckplaetze, erwartet 1\n",
		        (unsigned long)slot_count);
		return step;
	}
	printf("Steckplatz: %lu\n", (unsigned long)slots[0]);

	step = 6;
	rv = fns->C_GetTokenInfo(slots[0], &token);
	if (rv != CKR_OK_V) {
		return fail("C_GetTokenInfo", rv);
	}
	printf("Token:\n");
	text("Name", token.label, sizeof(token.label));
	text("Modell", token.model, sizeof(token.model));
	text("Seriennummer", (const CK_UTF8CHAR *)token.serialNumber,
	     sizeof(token.serialNumber));
	printf("  %-14s 0x%lx\n", "Kennzeichen", (unsigned long)token.flags);

	step = 7;
	rv = fns->C_GetMechanismList(slots[0], mechanisms, &mechanism_count);
	if (rv != CKR_OK_V) {
		return fail("C_GetMechanismList", rv);
	}
	printf("Verfahren: %lu\n   ", (unsigned long)mechanism_count);
	for (index = 0; index < mechanism_count; index++) {
		printf(" 0x%lx", (unsigned long)mechanisms[index]);
	}
	printf("\n");

	step = 8;
	rv = fns->C_OpenSession(slots[0], CKF_SERIAL_V, NULL, NULL, &session);
	if (rv != CKR_OK_V) {
		return fail("C_OpenSession", rv);
	}

	/* Vor dem Anmelden darf nichts zu finden sein. Das ist die
	 * Zwischenpruefung: ohne sie wuesste niemand, ob die Liste spaeter
	 * wirklich an der Anmeldung haengt. */
	step = 9;
	rv = fns->C_FindObjectsInit(session, NULL, 0);
	if (rv == CKR_OK_V) {
		CK_ULONG early = 0;
		fns->C_FindObjects(session, privates, 8, &early);
		fns->C_FindObjectsFinal(session);
		if (early != 0) {
			fprintf(stderr,
			        "probe: vor dem Anmelden wurden %lu Objekte genannt\n",
			        (unsigned long)early);
			return step;
		}
	}
	printf("Vor dem Anmelden: kein Objekt sichtbar - richtig.\n");

	step = 10;
	rv = fns->C_Login(session, CKU_USER_V, (CK_UTF8CHAR_PTR)pin,
	                  (CK_ULONG)strlen(pin));
	if (rv != CKR_OK_V) {
		return fail("C_Login", rv);
	}
	printf("Angemeldet.\n");

	/* Private Schluessel suchen. */
	step = 11;
	want_class = CKO_PRIVATE_KEY_V;
	search[0].type = CKA_CLASS_V;
	search[0].pValue = &want_class;
	search[0].ulValueLen = sizeof(want_class);
	rv = fns->C_FindObjectsInit(session, search, 1);
	if (rv != CKR_OK_V) {
		return fail("C_FindObjectsInit", rv);
	}
	rv = fns->C_FindObjects(session, privates, 8, &private_count);
	if (rv != CKR_OK_V) {
		return fail("C_FindObjects", rv);
	}
	fns->C_FindObjectsFinal(session);
	printf("Private Schluessel: %lu\n", (unsigned long)private_count);
	if (private_count == 0) {
		fprintf(stderr, "probe: kein privater Schluessel im Tresor\n");
		return step;
	}

	/* Merkmale lesen - und die Gegenprobe, dass der private Teil NICHT
	 * herauskommt. Das ist die eigentliche Zusicherung dieser Bibliothek. */
	step = 12;
	{
		CK_OBJECT_HANDLE rsa_private = 0;
		CK_OBJECT_HANDLE rsa_public = 0;

		for (index = 0; index < private_count; index++) {
			memset(label, 0, sizeof(label));
			memset(id, 0, sizeof(id));
			key_type = 0;
			sensitive = 0;
			extractable = 1;
			query[0].type = CKA_LABEL_V;
			query[0].pValue = label;
			query[0].ulValueLen = sizeof(label) - 1;
			query[1].type = CKA_KEY_TYPE_V;
			query[1].pValue = &key_type;
			query[1].ulValueLen = sizeof(key_type);
			query[2].type = CKA_SENSITIVE_V;
			query[2].pValue = &sensitive;
			query[2].ulValueLen = sizeof(sensitive);
			query[3].type = CKA_EXTRACTABLE_V;
			query[3].pValue = &extractable;
			query[3].ulValueLen = sizeof(extractable);
			rv = fns->C_GetAttributeValue(session, privates[index], query, 4);
			if (rv != CKR_OK_V) {
				return fail("C_GetAttributeValue", rv);
			}
			printf("  Schluessel %lu: %s, Art 0x%lx, empfindlich %u, "
			       "ausleitbar %u\n",
			       (unsigned long)index, (const char *)label,
			       (unsigned long)key_type, sensitive, extractable);
			if (!sensitive || extractable) {
				fprintf(stderr, "probe: ein privater Schluessel gibt sich "
				                "als ausleitbar aus\n");
				return step;
			}
			/* Der private Teil darf NICHT kommen. */
			query[0].type = CKA_VALUE_V;
			query[0].pValue = modulus;
			query[0].ulValueLen = sizeof(modulus);
			rv = fns->C_GetAttributeValue(session, privates[index], query, 1);
			if (rv != CKR_ATTR_SENSITIVE_V) {
				fprintf(stderr, "probe: CKA_VALUE gab 0x%lx, erwartet "
				                "CKR_ATTRIBUTE_SENSITIVE\n",
				        (unsigned long)rv);
				return step;
			}
			if (key_type == CKK_RSA_V && rsa_private == 0) {
				rsa_private = privates[index];
				memcpy(id, label, 1); /* nur, damit id benutzt ist */
			}
		}
		printf("CKA_VALUE wird als empfindlich abgewiesen - richtig.\n");

		if (rsa_private == 0) {
			fprintf(stderr, "probe: kein RSA-Schluessel im Tresor - fuer den "
			                "Nachweis mit openssl wird einer gebraucht\n");
			return step;
		}

		/* Den oeffentlichen Teil holen: er ist kein Geheimnis und muss
		 * herauskommen. */
		step = 13;
		want_class = CKO_PUBLIC_KEY_V;
		search[0].pValue = &want_class;
		rv = fns->C_FindObjectsInit(session, search, 1);
		if (rv != CKR_OK_V) {
			return fail("C_FindObjectsInit public", rv);
		}
		rv = fns->C_FindObjects(session, publics, 8, &public_count);
		if (rv != CKR_OK_V) {
			return fail("C_FindObjects public", rv);
		}
		fns->C_FindObjectsFinal(session);
		printf("Oeffentliche Schluessel: %lu\n", (unsigned long)public_count);

		for (index = 0; index < public_count; index++) {
			CK_ULONG this_type = 0;
			query[0].type = CKA_KEY_TYPE_V;
			query[0].pValue = &this_type;
			query[0].ulValueLen = sizeof(this_type);
			if (fns->C_GetAttributeValue(session, publics[index], query, 1)
			    == CKR_OK_V && this_type == CKK_RSA_V) {
				rsa_public = publics[index];
				break;
			}
		}
		if (rsa_public == 0) {
			fprintf(stderr, "probe: kein oeffentlicher RSA-Teil gefunden\n");
			return step;
		}

		step = 14;
		public_query[0].type = CKA_MODULUS_V;
		public_query[0].pValue = modulus;
		public_query[0].ulValueLen = sizeof(modulus);
		public_query[1].type = CKA_PUBLIC_EXP_V;
		public_query[1].pValue = exponent;
		public_query[1].ulValueLen = sizeof(exponent);
		rv = fns->C_GetAttributeValue(session, rsa_public, public_query, 2);
		if (rv != CKR_OK_V) {
			return fail("C_GetAttributeValue modulus", rv);
		}
		printf("Modulus: %lu Bytes, Exponent: %lu Bytes\n",
		       (unsigned long)public_query[0].ulValueLen,
		       (unsigned long)public_query[1].ulValueLen);

		/* Jetzt unterschreiben. */
		step = 15;
		mechanism.mechanism = CKM_SHA256_RSA_V;
		mechanism.pParameter = NULL;
		mechanism.ulParameterLen = 0;
		rv = fns->C_SignInit(session, &mechanism, rsa_private);
		if (rv != CKR_OK_V) {
			return fail("C_SignInit", rv);
		}

		/* Zuerst nur nach der Laenge fragen - so macht es jeder Aufrufer. */
		step = 16;
		signature_len = 0;
		rv = fns->C_Sign(session, (CK_BYTE_PTR)payload,
		                 (CK_ULONG)(sizeof(payload) - 1), NULL, &signature_len);
		if (rv != CKR_OK_V) {
			return fail("C_Sign (Laenge)", rv);
		}
		printf("Angekuendigte Laenge: %lu Bytes\n",
		       (unsigned long)signature_len);
		if (signature_len == 0 || signature_len > sizeof(signature)) {
			fprintf(stderr, "probe: angekuendigte Laenge unbrauchbar\n");
			return step;
		}

		step = 17;
		signature_len = sizeof(signature);
		rv = fns->C_Sign(session, (CK_BYTE_PTR)payload,
		                 (CK_ULONG)(sizeof(payload) - 1), signature,
		                 &signature_len);
		if (rv != CKR_OK_V) {
			return fail("C_Sign", rv);
		}
		printf("Unterschrift: %lu Bytes\n", (unsigned long)signature_len);

		step = 18;
		snprintf(out_path, sizeof(out_path), "%s/daten.bin", out_dir);
		if (!write_file(out_path, payload, sizeof(payload) - 1)) {
			return step;
		}
		snprintf(out_path, sizeof(out_path), "%s/unterschrift.bin", out_dir);
		if (!write_file(out_path, signature, (size_t)signature_len)) {
			return step;
		}
		printf("Daten und Unterschrift liegen in %s\n", out_dir);
	}

	step = 19;
	rv = fns->C_Logout(session);
	if (rv != CKR_OK_V) {
		return fail("C_Logout", rv);
	}
	rv = fns->C_CloseSession(session);
	if (rv != CKR_OK_V) {
		return fail("C_CloseSession", rv);
	}
	rv = fns->C_Finalize(NULL);
	if (rv != CKR_OK_V) {
		return fail("C_Finalize", rv);
	}

	/* Diese Zeile ist der Beleg, nicht die 0. */
	printf("probe: ok\n");
	return 0;
}
