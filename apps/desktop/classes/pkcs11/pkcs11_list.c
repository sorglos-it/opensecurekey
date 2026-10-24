/* Die Tabelle der Funktionszeiger.
 *
 * Sie steht in C und nicht in Go, weil ihr Aufbau zur Schnittstelle gehoert:
 * ein fremdes Programm bekommt einen Zeiger auf GENAU diese Struktur und
 * liest die Zeiger an ihrer Stelle. Gefuellt wird sie mit den Funktionen,
 * die die Go-Seite ausfuehrt (//export in pkcs11_osk.go).
 *
 * Was wir nicht anbieten, steht hier als NULL. Das ist der ehrliche Weg:
 * PKCS#11 erlaubt NULL ausdruecklich, und ein Aufrufer sieht dann sofort,
 * dass es diese Funktion hier nicht gibt - anders als bei einer Funktion,
 * die etwas zurueckgibt und dabei nichts tut.
 *
 * Zwei Ausnahmen von "NULL": C_GetFunctionStatus und C_CancelFunction sind
 * laut Spezifikation vorhanden und geben CKR_FUNCTION_NOT_PARALLEL zurueck.
 * Das uebernimmt die Go-Seite.
 */
#include <string.h>
#include "pkcs11_osk.h"

/* Die Funktionen, die Go ausfuehrt. cgo erzeugt sie mit genau diesen Namen. */
extern CK_RV C_Initialize(CK_VOID_PTR pInitArgs);
extern CK_RV C_Finalize(CK_VOID_PTR pReserved);
extern CK_RV C_GetInfo(CK_INFO_PTR pInfo);
extern CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList);
extern CK_RV C_GetSlotList(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount);
extern CK_RV C_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo);
extern CK_RV C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo);
extern CK_RV C_GetMechanismList(CK_SLOT_ID slotID, CK_MECHANISM_TYPE_PTR pList, CK_ULONG_PTR pulCount);
extern CK_RV C_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR pInfo);
extern CK_RV C_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags, CK_VOID_PTR pApplication, CK_NOTIFY Notify, CK_SESSION_HANDLE_PTR phSession);
extern CK_RV C_CloseSession(CK_SESSION_HANDLE hSession);
extern CK_RV C_CloseAllSessions(CK_SLOT_ID slotID);
extern CK_RV C_GetSessionInfo(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR pInfo);
extern CK_RV C_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen);
extern CK_RV C_Logout(CK_SESSION_HANDLE hSession);
extern CK_RV C_GetObjectSize(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize);
extern CK_RV C_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);
extern CK_RV C_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);
extern CK_RV C_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxCount, CK_ULONG_PTR pulCount);
extern CK_RV C_FindObjectsFinal(CK_SESSION_HANDLE hSession);
extern CK_RV C_SignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism, CK_OBJECT_HANDLE hKey);
extern CK_RV C_Sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData, CK_ULONG ulDataLen, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen);
extern CK_RV C_SignUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart, CK_ULONG ulPartLen);
extern CK_RV C_SignFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature, CK_ULONG_PTR pulSignatureLen);
extern CK_RV C_GetFunctionStatus(CK_SESSION_HANDLE hSession);
extern CK_RV C_CancelFunction(CK_SESSION_HANDLE hSession);
extern CK_RV C_WaitForSlotEvent(CK_FLAGS flags, CK_SLOT_ID_PTR pSlot, CK_VOID_PTR pReserved);

/* Die Fassung, die wir nennen: 2.40. Das ist die Fassung der TABELLE, nicht
 * die unseres Codes - der Aufbau von CK_FUNCTION_LIST ist seit 2.40
 * unveraendert, und jedes Programm, das PKCS#11 spricht, kennt ihn. Die
 * Erweiterungen von 3.0 (Nachrichtenbetrieb) bieten wir nicht an, und darum
 * waere "3.0" hier eine Zusage, die wir nicht halten. */
static CK_FUNCTION_LIST osk_list = {
	{ 2, 40 },
	C_Initialize,
	C_Finalize,
	C_GetInfo,
	C_GetFunctionList,
	C_GetSlotList,
	C_GetSlotInfo,
	C_GetTokenInfo,
	C_GetMechanismList,
	C_GetMechanismInfo,
	NULL,                     /* C_InitToken - das Geraet richtet man am Geraet ein */
	NULL,                     /* C_InitPIN */
	NULL,                     /* C_SetPIN */
	C_OpenSession,
	C_CloseSession,
	C_CloseAllSessions,
	C_GetSessionInfo,
	NULL,                     /* C_GetOperationState */
	NULL,                     /* C_SetOperationState */
	C_Login,
	C_Logout,
	NULL,                     /* C_CreateObject - fremde Software schreibt hier nichts */
	NULL,                     /* C_CopyObject */
	NULL,                     /* C_DestroyObject */
	C_GetObjectSize,
	C_GetAttributeValue,
	NULL,                     /* C_SetAttributeValue */
	C_FindObjectsInit,
	C_FindObjects,
	C_FindObjectsFinal,
	NULL,                     /* C_EncryptInit  - nur Unterschriften */
	NULL,                     /* C_Encrypt */
	NULL,                     /* C_EncryptUpdate */
	NULL,                     /* C_EncryptFinal */
	NULL,                     /* C_DecryptInit  - entschluesseln nie */
	NULL,                     /* C_Decrypt */
	NULL,                     /* C_DecryptUpdate */
	NULL,                     /* C_DecryptFinal */
	NULL,                     /* C_DigestInit   - streuen kann der Aufrufer selbst */
	NULL,                     /* C_Digest */
	NULL,                     /* C_DigestUpdate */
	NULL,                     /* C_DigestKey */
	NULL,                     /* C_DigestFinal */
	C_SignInit,
	C_Sign,
	C_SignUpdate,
	C_SignFinal,
	NULL,                     /* C_SignRecoverInit */
	NULL,                     /* C_SignRecover */
	NULL,                     /* C_VerifyInit - pruefen braucht kein Geheimnis */
	NULL,                     /* C_Verify */
	NULL,                     /* C_VerifyUpdate */
	NULL,                     /* C_VerifyFinal */
	NULL,                     /* C_VerifyRecoverInit */
	NULL,                     /* C_VerifyRecover */
	NULL,                     /* C_DigestEncryptUpdate */
	NULL,                     /* C_DecryptDigestUpdate */
	NULL,                     /* C_SignEncryptUpdate */
	NULL,                     /* C_DecryptVerifyUpdate */
	NULL,                     /* C_GenerateKey - Schluessel entstehen im Geraet */
	NULL,                     /* C_GenerateKeyPair */
	NULL,                     /* C_WrapKey */
	NULL,                     /* C_UnwrapKey */
	NULL,                     /* C_DeriveKey */
	NULL,                     /* C_SeedRandom */
	NULL,                     /* C_GenerateRandom */
	C_GetFunctionStatus,
	C_CancelFunction,
	C_WaitForSlotEvent
};

/* Der Name der Schnittstelle ist in v3.0 festgelegt: "PKCS 11", mit
 * Leerzeichen und ohne Raute. */
static CK_CHAR osk_interface_name[] = "PKCS 11";

static CK_INTERFACE osk_iface = {
	osk_interface_name,
	&osk_list,
	0
};

CK_FUNCTION_LIST_PTR osk_function_list(void) { return &osk_list; }
CK_INTERFACE_PTR     osk_interface(void)     { return &osk_iface; }

/* PKCS#11 fuellt Felder fester Laenge mit LEERZEICHEN. Mit Nullbytes
 * gefuellt zeigen manche Programme den Namen samt Muell dahinter an. */
void osk_pad(CK_UTF8CHAR *field, size_t size, const char *text)
{
	size_t length = text ? strlen(text) : 0;
	if (length > size) {
		length = size;
	}
	memset(field, ' ', size);
	if (length) {
		memcpy(field, text, length);
	}
}

size_t osk_sizeof_ulong(void) { return sizeof(CK_ULONG); }
size_t osk_sizeof_bbool(void) { return sizeof(CK_BBOOL); }
