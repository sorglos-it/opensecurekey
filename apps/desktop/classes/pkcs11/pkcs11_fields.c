/* Zugriff auf die Strukturen von PKCS#11 - in C, nicht in Go.
 *
 * Der Grund ist gemessen, nicht geraten: unter Windows werden diese
 * Strukturen byteweise gepackt (siehe pkcs11_osk.h). Eine gepackte Struktur
 * kann cgo nicht mit Feldern abbilden - es macht daraus ein Feld aus Bytes,
 * und jeder Zugriff per Feldnamen scheitert beim Uebersetzen. Mit der
 * Vorgabe-Ausrichtung geht es, aber dann stimmt unsere Struktur nicht mehr
 * mit der ueberein, die Firefox und OpenSSL erwarten - und das faellt
 * niemandem auf, bis die Werte verrutscht ankommen.
 *
 * Also: die Struktur bleibt gepackt, und jeder Zugriff darauf steht hier.
 * Der C-Uebersetzer kennt die Ausrichtung, wir muessen sie nicht nachbauen.
 *
 * Diese Datei rechnet nichts. Sie liest und schreibt Felder, prueft Puffer
 * und sonst nichts - alles Fachliche steht in classes/pkcs11_token.go.
 */
#include <string.h>
#include "pkcs11_osk.h"

#define CKR_OK_VALUE               0x00000000UL
#define CKR_BUFFER_TOO_SMALL_VALUE 0x00000150UL

/* --- C_Initialize: was der Aufrufer mitgibt --------------------------------- */

/* 0 = in Ordnung, 1 = pReserved belegt, 2 = eigene Sperren verlangt. */
int osk_init_args_check(CK_VOID_PTR args)
{
	CK_C_INITIALIZE_ARGS_PTR init;
	if (args == NULL) {
		return 0;
	}
	init = (CK_C_INITIALIZE_ARGS_PTR)args;
	if (init->pReserved != NULL) {
		return 1;
	}
	if (init->CreateMutex || init->DestroyMutex ||
	    init->LockMutex || init->UnlockMutex) {
		return 2;
	}
	return 0;
}

/* --- CK_INFO ----------------------------------------------------------------- */

void osk_info_fill(CK_INFO_PTR info, const char *manufacturer,
                   const char *description, CK_BYTE major, CK_BYTE minor,
                   CK_BYTE cryptokiMajor, CK_BYTE cryptokiMinor)
{
	info->cryptokiVersion.major = cryptokiMajor;
	info->cryptokiVersion.minor = cryptokiMinor;
	osk_pad(info->manufacturerID, sizeof(info->manufacturerID), manufacturer);
	info->flags = 0;
	osk_pad(info->libraryDescription, sizeof(info->libraryDescription), description);
	info->libraryVersion.major = major;
	info->libraryVersion.minor = minor;
}

/* --- CK_SLOT_INFO ------------------------------------------------------------ */

void osk_slot_info_fill(CK_SLOT_INFO_PTR info, const char *description,
                        const char *manufacturer, CK_FLAGS flags,
                        CK_BYTE fwMajor, CK_BYTE fwMinor)
{
	osk_pad(info->slotDescription, sizeof(info->slotDescription), description);
	osk_pad(info->manufacturerID, sizeof(info->manufacturerID), manufacturer);
	info->flags = flags;
	info->hardwareVersion.major = 0;
	info->hardwareVersion.minor = 0;
	info->firmwareVersion.major = fwMajor;
	info->firmwareVersion.minor = fwMinor;
}

/* --- CK_TOKEN_INFO ----------------------------------------------------------- */

void osk_token_info_fill(CK_TOKEN_INFO_PTR info, const char *label,
                         const char *manufacturer, const char *model,
                         const char *serial, CK_FLAGS flags,
                         CK_ULONG minPin, CK_ULONG maxPin,
                         CK_BYTE fwMajor, CK_BYTE fwMinor)
{
	/* Die Zahl, die PKCS#11 fuer "unbekannt" vorsieht. Sie wird hier
	 * gerechnet und nicht als Zahl geschrieben: CK_ULONG ist unter Windows
	 * vier und unter Linux acht Bytes breit. */
	CK_ULONG unknown = (CK_ULONG)0;
	unknown--;

	osk_pad(info->label, sizeof(info->label), label);
	osk_pad(info->manufacturerID, sizeof(info->manufacturerID), manufacturer);
	osk_pad(info->model, sizeof(info->model), model);
	osk_pad((CK_UTF8CHAR *)info->serialNumber, sizeof(info->serialNumber), serial);
	info->flags = flags;
	info->ulMaxSessionCount = unknown;
	info->ulSessionCount = unknown;
	info->ulMaxRwSessionCount = 0;
	info->ulRwSessionCount = 0;
	info->ulMinPinLen = minPin;
	info->ulMaxPinLen = maxPin;
	/* Ueber den Platz im Chip sagen wir nichts: shared/protocol.md nennt
	 * die Zahl des Chips ausdruecklich als untere Grenze, nicht als
	 * Messung. Erfundene Genauigkeit waere hier schlimmer als "unbekannt". */
	info->ulTotalPublicMemory = unknown;
	info->ulFreePublicMemory = unknown;
	info->ulTotalPrivateMemory = unknown;
	info->ulFreePrivateMemory = unknown;
	info->hardwareVersion.major = 0;
	info->hardwareVersion.minor = 0;
	info->firmwareVersion.major = fwMajor;
	info->firmwareVersion.minor = fwMinor;
	/* Die Uhr gibt das Geraet nicht heraus. Das Feld bleibt leer; erlaubt
	 * ist das, weil CKF_CLOCK_ON_TOKEN nicht gesetzt ist. */
	memset(info->utcTime, ' ', sizeof(info->utcTime));
}

/* --- CK_SESSION_INFO --------------------------------------------------------- */

void osk_session_info_fill(CK_SESSION_INFO_PTR info, CK_SLOT_ID slot,
                           CK_STATE state, CK_FLAGS flags)
{
	info->slotID = slot;
	info->state = state;
	info->flags = flags;
	info->ulDeviceError = 0;
}

/* --- CK_MECHANISM_INFO ------------------------------------------------------- */

void osk_mech_info_fill(CK_MECHANISM_INFO_PTR info, CK_ULONG min,
                        CK_ULONG max, CK_FLAGS flags)
{
	info->ulMinKeySize = min;
	info->ulMaxKeySize = max;
	info->flags = flags;
}

/* --- CK_MECHANISM ------------------------------------------------------------ */

CK_ULONG    osk_mech_type(CK_MECHANISM_PTR m)      { return m->mechanism; }
CK_ULONG    osk_mech_param_len(CK_MECHANISM_PTR m) { return m->ulParameterLen; }
CK_VOID_PTR osk_mech_param(CK_MECHANISM_PTR m)     { return m->pParameter; }

/* --- CK_ATTRIBUTE ------------------------------------------------------------ */

CK_ULONG    osk_attr_type(CK_ATTRIBUTE_PTR list, CK_ULONG index) { return list[index].type; }
CK_VOID_PTR osk_attr_value(CK_ATTRIBUTE_PTR list, CK_ULONG index) { return list[index].pValue; }
CK_ULONG    osk_attr_len(CK_ATTRIBUTE_PTR list, CK_ULONG index) { return list[index].ulValueLen; }

/* osk_attr_unknown setzt die Laenge auf -1. So verlangt PKCS#11, dass ein
 * Merkmal gemeldet wird, das es an diesem Objekt nicht gibt: der Aufrufer
 * sieht dann, WELCHES gefehlt hat. */
void osk_attr_unknown(CK_ATTRIBUTE_PTR list, CK_ULONG index)
{
	CK_ULONG unknown = (CK_ULONG)0;
	unknown--;
	list[index].ulValueLen = unknown;
}

/* osk_attr_write schreibt einen Wert. Ist pValue leer, wird nur die Laenge
 * genannt - so fragt jeder Aufrufer zuerst nach der Groesse. */
CK_RV osk_attr_write(CK_ATTRIBUTE_PTR list, CK_ULONG index,
                     const void *data, CK_ULONG length)
{
	if (list[index].pValue == NULL) {
		list[index].ulValueLen = length;
		return CKR_OK_VALUE;
	}
	if (list[index].ulValueLen < length) {
		list[index].ulValueLen = length;
		return CKR_BUFFER_TOO_SMALL_VALUE;
	}
	if (length > 0) {
		memcpy(list[index].pValue, data, (size_t)length);
	}
	list[index].ulValueLen = length;
	return CKR_OK_VALUE;
}

/* --- CK_INTERFACE ------------------------------------------------------------ */

void osk_interface_copy(CK_INTERFACE_PTR target)
{
	*target = *osk_interface();
}

/* --- Felder einfacher Zahlen ------------------------------------------------- */

void osk_ulong_set(CK_ULONG_PTR target, CK_ULONG value) { *target = value; }
CK_ULONG osk_ulong_get(CK_ULONG_PTR source) { return *source; }

void osk_slot_set(CK_SLOT_ID_PTR list, CK_ULONG index, CK_SLOT_ID value)
{
	list[index] = value;
}

void osk_object_set(CK_OBJECT_HANDLE_PTR list, CK_ULONG index, CK_OBJECT_HANDLE value)
{
	list[index] = value;
}

void osk_mech_list_set(CK_MECHANISM_TYPE_PTR list, CK_ULONG index, CK_MECHANISM_TYPE value)
{
	list[index] = value;
}

void osk_bytes_copy(CK_BYTE_PTR target, const void *source, CK_ULONG length)
{
	if (length > 0) {
		memcpy(target, source, (size_t)length);
	}
}

/* osk_version_major und osk_version_minor lesen eine Fassung. Nur fuer die
 * Pruefung der Pipe-Seite - die Go-Seite baut CK_VERSION nie selbst. */
CK_BYTE osk_version_major(CK_VERSION_PTR v) { return v ? v->major : 0; }
CK_BYTE osk_version_minor(CK_VERSION_PTR v) { return v ? v->minor : 0; }
