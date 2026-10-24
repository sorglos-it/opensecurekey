/* Die Schnittstelle PKCS#11 v3.0, so weit wir sie anbieten.
 *
 * Diese Datei ist Abschrift aus der Spezifikation (OASIS PKCS #11 Base
 * Specification v3.0): Typen, Strukturen und die Reihenfolge der Zeiger in
 * CK_FUNCTION_LIST stehen dort so. An einer Schnittstelle ist nichts frei -
 * wer hier etwas aendert, wird von Firefox, OpenSSL und Java nicht mehr
 * verstanden. Die Umsetzung dahinter ist unsere eigene (pkcs11_osk.go).
 *
 * Zwei Dinge, an denen eine eigene Umsetzung ueblicherweise scheitert, und
 * die darum hier ausdruecklich stehen:
 *
 *   1. Unter Windows werden die Strukturen BYTEWEISE gepackt. Das steht so
 *      in der Spezifikation (Anhang zu den Plattformen) und ist nicht
 *      Geschmack: wer es weglaesst, liefert Strukturen, die der Aufrufer
 *      falsch liest - und zwar leise.
 *   2. CK_ULONG ist "unsigned long". Das sind unter Windows VIER Bytes und
 *      unter Linux ACHT. Jede Zahl, die ueber diese Grenze geht, muss darum
 *      umgerechnet werden, nie einfach durchgereicht.
 */
#ifndef OSK_PKCS11_H
#define OSK_PKCS11_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char     CK_BYTE;
typedef CK_BYTE           CK_CHAR;
typedef CK_BYTE           CK_UTF8CHAR;
typedef CK_BYTE           CK_BBOOL;
typedef unsigned long int CK_ULONG;
typedef long int          CK_LONG;
typedef CK_ULONG          CK_FLAGS;

typedef CK_BYTE     *CK_BYTE_PTR;
typedef CK_CHAR     *CK_CHAR_PTR;
typedef CK_UTF8CHAR *CK_UTF8CHAR_PTR;
typedef CK_ULONG    *CK_ULONG_PTR;
typedef void        *CK_VOID_PTR;
typedef CK_VOID_PTR *CK_VOID_PTR_PTR;

#define CK_TRUE  1
#define CK_FALSE 0

typedef CK_ULONG CK_RV;
typedef CK_ULONG CK_SLOT_ID;
typedef CK_SLOT_ID *CK_SLOT_ID_PTR;
typedef CK_ULONG CK_SESSION_HANDLE;
typedef CK_SESSION_HANDLE *CK_SESSION_HANDLE_PTR;
typedef CK_ULONG CK_OBJECT_HANDLE;
typedef CK_OBJECT_HANDLE *CK_OBJECT_HANDLE_PTR;
typedef CK_ULONG CK_OBJECT_CLASS;
typedef CK_ULONG CK_ATTRIBUTE_TYPE;
typedef CK_ULONG CK_MECHANISM_TYPE;
typedef CK_MECHANISM_TYPE *CK_MECHANISM_TYPE_PTR;
typedef CK_ULONG CK_USER_TYPE;
typedef CK_ULONG CK_STATE;
typedef CK_ULONG CK_NOTIFICATION;

/* Unter Windows byteweise packen - siehe Kopf dieser Datei. */
#if defined(_WIN32) || defined(_WIN64)
#pragma pack(push, 1)
#endif

typedef struct CK_VERSION {
	CK_BYTE major;
	CK_BYTE minor;
} CK_VERSION;
typedef CK_VERSION *CK_VERSION_PTR;

typedef struct CK_INFO {
	CK_VERSION  cryptokiVersion;
	CK_UTF8CHAR manufacturerID[32];
	CK_FLAGS    flags;
	CK_UTF8CHAR libraryDescription[32];
	CK_VERSION  libraryVersion;
} CK_INFO;
typedef CK_INFO *CK_INFO_PTR;

typedef struct CK_SLOT_INFO {
	CK_UTF8CHAR slotDescription[64];
	CK_UTF8CHAR manufacturerID[32];
	CK_FLAGS    flags;
	CK_VERSION  hardwareVersion;
	CK_VERSION  firmwareVersion;
} CK_SLOT_INFO;
typedef CK_SLOT_INFO *CK_SLOT_INFO_PTR;

typedef struct CK_TOKEN_INFO {
	CK_UTF8CHAR label[32];
	CK_UTF8CHAR manufacturerID[32];
	CK_UTF8CHAR model[16];
	CK_CHAR     serialNumber[16];
	CK_FLAGS    flags;
	CK_ULONG    ulMaxSessionCount;
	CK_ULONG    ulSessionCount;
	CK_ULONG    ulMaxRwSessionCount;
	CK_ULONG    ulRwSessionCount;
	CK_ULONG    ulMaxPinLen;
	CK_ULONG    ulMinPinLen;
	CK_ULONG    ulTotalPublicMemory;
	CK_ULONG    ulFreePublicMemory;
	CK_ULONG    ulTotalPrivateMemory;
	CK_ULONG    ulFreePrivateMemory;
	CK_VERSION  hardwareVersion;
	CK_VERSION  firmwareVersion;
	CK_CHAR     utcTime[16];
} CK_TOKEN_INFO;
typedef CK_TOKEN_INFO *CK_TOKEN_INFO_PTR;

typedef struct CK_SESSION_INFO {
	CK_SLOT_ID slotID;
	CK_STATE   state;
	CK_FLAGS   flags;
	CK_ULONG   ulDeviceError;
} CK_SESSION_INFO;
typedef CK_SESSION_INFO *CK_SESSION_INFO_PTR;

typedef struct CK_ATTRIBUTE {
	CK_ATTRIBUTE_TYPE type;
	CK_VOID_PTR       pValue;
	CK_ULONG          ulValueLen;
} CK_ATTRIBUTE;
typedef CK_ATTRIBUTE *CK_ATTRIBUTE_PTR;

typedef struct CK_MECHANISM {
	CK_MECHANISM_TYPE mechanism;
	CK_VOID_PTR       pParameter;
	CK_ULONG          ulParameterLen;
} CK_MECHANISM;
typedef CK_MECHANISM *CK_MECHANISM_PTR;

typedef struct CK_MECHANISM_INFO {
	CK_ULONG ulMinKeySize;
	CK_ULONG ulMaxKeySize;
	CK_FLAGS flags;
} CK_MECHANISM_INFO;
typedef CK_MECHANISM_INFO *CK_MECHANISM_INFO_PTR;

typedef CK_RV (*CK_CREATEMUTEX)(CK_VOID_PTR_PTR ppMutex);
typedef CK_RV (*CK_DESTROYMUTEX)(CK_VOID_PTR pMutex);
typedef CK_RV (*CK_LOCKMUTEX)(CK_VOID_PTR pMutex);
typedef CK_RV (*CK_UNLOCKMUTEX)(CK_VOID_PTR pMutex);

typedef struct CK_C_INITIALIZE_ARGS {
	CK_CREATEMUTEX  CreateMutex;
	CK_DESTROYMUTEX DestroyMutex;
	CK_LOCKMUTEX    LockMutex;
	CK_UNLOCKMUTEX  UnlockMutex;
	CK_FLAGS        flags;
	CK_VOID_PTR     pReserved;
} CK_C_INITIALIZE_ARGS;
typedef CK_C_INITIALIZE_ARGS *CK_C_INITIALIZE_ARGS_PTR;

#define CKF_LIBRARY_CANT_CREATE_OS_THREADS 0x00000001UL
#define CKF_OS_LOCKING_OK                  0x00000002UL

typedef CK_RV (*CK_NOTIFY)(CK_SESSION_HANDLE hSession, CK_NOTIFICATION event,
                           CK_VOID_PTR pApplication);

/* CK_INTERFACE kommt aus v3.0: damit kann ein Aufrufer nach einer
 * bestimmten Fassung der Schnittstelle fragen. */
typedef struct CK_INTERFACE {
	CK_CHAR     *pInterfaceName;
	CK_VOID_PTR pFunctionList;
	CK_FLAGS    flags;
} CK_INTERFACE;
typedef CK_INTERFACE *CK_INTERFACE_PTR;
typedef CK_INTERFACE_PTR *CK_INTERFACE_PTR_PTR;

struct CK_FUNCTION_LIST;
typedef struct CK_FUNCTION_LIST  CK_FUNCTION_LIST;
typedef CK_FUNCTION_LIST        *CK_FUNCTION_LIST_PTR;
typedef CK_FUNCTION_LIST_PTR    *CK_FUNCTION_LIST_PTR_PTR;

/* Die 68 Zeiger in genau dieser Reihenfolge. Der Aufrufer greift ueber den
 * Namen zu, aber der Binder ueber die Stelle - eine vertauschte Zeile
 * bedeutet, dass ein fremdes Programm C_Decrypt aufruft und C_Digest
 * bekommt. */
struct CK_FUNCTION_LIST {
	CK_VERSION version;
	CK_RV (*C_Initialize)(CK_VOID_PTR);
	CK_RV (*C_Finalize)(CK_VOID_PTR);
	CK_RV (*C_GetInfo)(CK_INFO_PTR);
	CK_RV (*C_GetFunctionList)(CK_FUNCTION_LIST_PTR_PTR);
	CK_RV (*C_GetSlotList)(CK_BBOOL, CK_SLOT_ID_PTR, CK_ULONG_PTR);
	CK_RV (*C_GetSlotInfo)(CK_SLOT_ID, CK_SLOT_INFO_PTR);
	CK_RV (*C_GetTokenInfo)(CK_SLOT_ID, CK_TOKEN_INFO_PTR);
	CK_RV (*C_GetMechanismList)(CK_SLOT_ID, CK_MECHANISM_TYPE_PTR, CK_ULONG_PTR);
	CK_RV (*C_GetMechanismInfo)(CK_SLOT_ID, CK_MECHANISM_TYPE, CK_MECHANISM_INFO_PTR);
	CK_RV (*C_InitToken)(CK_SLOT_ID, CK_UTF8CHAR_PTR, CK_ULONG, CK_UTF8CHAR_PTR);
	CK_RV (*C_InitPIN)(CK_SESSION_HANDLE, CK_UTF8CHAR_PTR, CK_ULONG);
	CK_RV (*C_SetPIN)(CK_SESSION_HANDLE, CK_UTF8CHAR_PTR, CK_ULONG, CK_UTF8CHAR_PTR, CK_ULONG);
	CK_RV (*C_OpenSession)(CK_SLOT_ID, CK_FLAGS, CK_VOID_PTR, CK_NOTIFY, CK_SESSION_HANDLE_PTR);
	CK_RV (*C_CloseSession)(CK_SESSION_HANDLE);
	CK_RV (*C_CloseAllSessions)(CK_SLOT_ID);
	CK_RV (*C_GetSessionInfo)(CK_SESSION_HANDLE, CK_SESSION_INFO_PTR);
	CK_RV (*C_GetOperationState)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_SetOperationState)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_OBJECT_HANDLE, CK_OBJECT_HANDLE);
	CK_RV (*C_Login)(CK_SESSION_HANDLE, CK_USER_TYPE, CK_UTF8CHAR_PTR, CK_ULONG);
	CK_RV (*C_Logout)(CK_SESSION_HANDLE);
	CK_RV (*C_CreateObject)(CK_SESSION_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR);
	CK_RV (*C_CopyObject)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR);
	CK_RV (*C_DestroyObject)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE);
	CK_RV (*C_GetObjectSize)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE, CK_ULONG_PTR);
	CK_RV (*C_GetAttributeValue)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG);
	CK_RV (*C_SetAttributeValue)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG);
	CK_RV (*C_FindObjectsInit)(CK_SESSION_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG);
	CK_RV (*C_FindObjects)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE_PTR, CK_ULONG, CK_ULONG_PTR);
	CK_RV (*C_FindObjectsFinal)(CK_SESSION_HANDLE);
	CK_RV (*C_EncryptInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
	CK_RV (*C_Encrypt)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_EncryptUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_EncryptFinal)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DecryptInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
	CK_RV (*C_Decrypt)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DecryptUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DecryptFinal)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DigestInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR);
	CK_RV (*C_Digest)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DigestUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG);
	CK_RV (*C_DigestKey)(CK_SESSION_HANDLE, CK_OBJECT_HANDLE);
	CK_RV (*C_DigestFinal)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_SignInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
	CK_RV (*C_Sign)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_SignUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG);
	CK_RV (*C_SignFinal)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_SignRecoverInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
	CK_RV (*C_SignRecover)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_VerifyInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
	CK_RV (*C_Verify)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG);
	CK_RV (*C_VerifyUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG);
	CK_RV (*C_VerifyFinal)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG);
	CK_RV (*C_VerifyRecoverInit)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE);
	CK_RV (*C_VerifyRecover)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DigestEncryptUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DecryptDigestUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_SignEncryptUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_DecryptVerifyUpdate)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_GenerateKey)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR);
	CK_RV (*C_GenerateKeyPair)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_ATTRIBUTE_PTR, CK_ULONG, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR, CK_OBJECT_HANDLE_PTR);
	CK_RV (*C_WrapKey)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE, CK_OBJECT_HANDLE, CK_BYTE_PTR, CK_ULONG_PTR);
	CK_RV (*C_UnwrapKey)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE, CK_BYTE_PTR, CK_ULONG, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR);
	CK_RV (*C_DeriveKey)(CK_SESSION_HANDLE, CK_MECHANISM_PTR, CK_OBJECT_HANDLE, CK_ATTRIBUTE_PTR, CK_ULONG, CK_OBJECT_HANDLE_PTR);
	CK_RV (*C_SeedRandom)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG);
	CK_RV (*C_GenerateRandom)(CK_SESSION_HANDLE, CK_BYTE_PTR, CK_ULONG);
	CK_RV (*C_GetFunctionStatus)(CK_SESSION_HANDLE);
	CK_RV (*C_CancelFunction)(CK_SESSION_HANDLE);
	CK_RV (*C_WaitForSlotEvent)(CK_FLAGS, CK_SLOT_ID_PTR, CK_VOID_PTR);
};

#if defined(_WIN32) || defined(_WIN64)
#pragma pack(pop)
#endif

/* Die Tabelle. Sie wird in pkcs11_list.c gefuellt und von C_GetFunctionList
 * herausgegeben. */
CK_FUNCTION_LIST_PTR osk_function_list(void);
CK_INTERFACE_PTR     osk_interface(void);

/* Kleine Helfer, die in C einfacher sind als in Go: Felder fester Laenge
 * werden in PKCS#11 mit LEERZEICHEN gefuellt, nicht mit Nullbytes. Wer das
 * verwechselt, liefert Namen, die manche Software als Muell anzeigt. */
void osk_pad(CK_UTF8CHAR *field, size_t size, const char *text);

/* Groessen, die nur der C-Uebersetzer kennt. Die Go-Seite fragt sie ab,
 * statt sie zu annehmen - CK_ULONG ist unter Windows vier Bytes und unter
 * Linux acht. */
size_t osk_sizeof_ulong(void);
size_t osk_sizeof_bbool(void);

/* Zugriff auf die gepackten Strukturen - Umsetzung in pkcs11_fields.c.
 * Warum das in C steht und nicht in Go, steht im Kopf jener Datei. */
int      osk_init_args_check(CK_VOID_PTR args);
void     osk_info_fill(CK_INFO_PTR info, const char *manufacturer,
                       const char *description, CK_BYTE major, CK_BYTE minor,
                       CK_BYTE cryptokiMajor, CK_BYTE cryptokiMinor);
void     osk_slot_info_fill(CK_SLOT_INFO_PTR info, const char *description,
                            const char *manufacturer, CK_FLAGS flags,
                            CK_BYTE fwMajor, CK_BYTE fwMinor);
void     osk_token_info_fill(CK_TOKEN_INFO_PTR info, const char *label,
                             const char *manufacturer, const char *model,
                             const char *serial, CK_FLAGS flags,
                             CK_ULONG minPin, CK_ULONG maxPin,
                             CK_BYTE fwMajor, CK_BYTE fwMinor);
void     osk_session_info_fill(CK_SESSION_INFO_PTR info, CK_SLOT_ID slot,
                               CK_STATE state, CK_FLAGS flags);
void     osk_mech_info_fill(CK_MECHANISM_INFO_PTR info, CK_ULONG min,
                            CK_ULONG max, CK_FLAGS flags);
CK_ULONG    osk_mech_type(CK_MECHANISM_PTR m);
CK_ULONG    osk_mech_param_len(CK_MECHANISM_PTR m);
CK_VOID_PTR osk_mech_param(CK_MECHANISM_PTR m);
CK_ULONG    osk_attr_type(CK_ATTRIBUTE_PTR list, CK_ULONG index);
CK_VOID_PTR osk_attr_value(CK_ATTRIBUTE_PTR list, CK_ULONG index);
CK_ULONG    osk_attr_len(CK_ATTRIBUTE_PTR list, CK_ULONG index);
void        osk_attr_unknown(CK_ATTRIBUTE_PTR list, CK_ULONG index);
CK_RV       osk_attr_write(CK_ATTRIBUTE_PTR list, CK_ULONG index,
                           const void *data, CK_ULONG length);
void     osk_interface_copy(CK_INTERFACE_PTR target);
void     osk_ulong_set(CK_ULONG_PTR target, CK_ULONG value);
CK_ULONG osk_ulong_get(CK_ULONG_PTR source);
void     osk_slot_set(CK_SLOT_ID_PTR list, CK_ULONG index, CK_SLOT_ID value);
void     osk_object_set(CK_OBJECT_HANDLE_PTR list, CK_ULONG index, CK_OBJECT_HANDLE value);
void     osk_mech_list_set(CK_MECHANISM_TYPE_PTR list, CK_ULONG index, CK_MECHANISM_TYPE value);
void     osk_bytes_copy(CK_BYTE_PTR target, const void *source, CK_ULONG length);
CK_BYTE  osk_version_major(CK_VERSION_PTR v);
CK_BYTE  osk_version_minor(CK_VERSION_PTR v);

#ifdef __cplusplus
}
#endif

#endif /* OSK_PKCS11_H */
