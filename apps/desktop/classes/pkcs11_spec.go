// Die Zahlen von PKCS#11 v3.0.
//
// Diese Datei ist Abschrift: Namen und Werte stehen so in der Spezifikation
// (OASIS PKCS #11 Cryptographic Token Interface Base Specification v3.0) und
// in ihrem Header. Werte einer Schnittstelle sind keine Umsetzung - wer sie
// aendert, spricht nicht mehr PKCS#11. Uebernommen ist nur, was gebraucht
// wird; alles darueber stammt nicht von hier.
//
// Die Umsetzung steht in pkcs11_token.go, die C-Seite in classes/pkcs11/.
package classes

// Rueckgabewerte (CK_RV).
const (
	CkrOK                          = 0x00000000
	CkrCancel                      = 0x00000001
	CkrHostMemory                  = 0x00000002
	CkrSlotIDInvalid               = 0x00000003
	CkrGeneralError                = 0x00000005
	CkrFunctionFailed              = 0x00000006
	CkrArgumentsBad                = 0x00000007
	CkrAttributeSensitive          = 0x00000011
	CkrAttributeTypeInvalid        = 0x00000012
	CkrDataInvalid                 = 0x00000020
	CkrDataLenRange                = 0x00000021
	CkrDeviceError                 = 0x00000030
	CkrDeviceRemoved               = 0x00000032
	CkrFunctionNotSupported        = 0x00000054
	CkrKeyHandleInvalid            = 0x00000060
	CkrKeyTypeInconsistent         = 0x00000063
	CkrKeyFunctionNotPermitted     = 0x00000068
	CkrMechanismInvalid            = 0x00000070
	CkrObjectHandleInvalid         = 0x00000082
	CkrOperationActive             = 0x00000090
	CkrOperationNotInitialized     = 0x00000091
	CkrPinIncorrect                = 0x000000A0
	CkrPinLocked                   = 0x000000A4
	CkrSessionClosed               = 0x000000B0
	CkrSessionCount                = 0x000000B1
	CkrSessionHandleInvalid        = 0x000000B3
	CkrSessionReadOnly             = 0x000000B5
	CkrSessionParallelNotSupported = 0x000000B4
	CkrTokenNotPresent             = 0x000000E0
	CkrUserNotLoggedIn             = 0x00000101
	CkrUserTypeInvalid             = 0x00000103
	CkrUserAlreadyLoggedIn         = 0x00000100
	CkrBufferTooSmall              = 0x00000150
	CkrCryptokiNotInitialized      = 0x00000190
	CkrCryptokiAlreadyInit         = 0x00000191
	CkrFunctionRejected            = 0x00000200
)

// Objektklassen (CKA_CLASS).
const (
	CkoData        = 0x00000000
	CkoCertificate = 0x00000001
	CkoPublicKey   = 0x00000002
	CkoPrivateKey  = 0x00000003
)

// Schluesselarten (CKA_KEY_TYPE).
const (
	CkkRSA       = 0x00000000
	CkkEC        = 0x00000003
	CkkECEdwards = 0x00000040
)

// Zertifikatsarten (CKA_CERTIFICATE_TYPE).
const CkcX509 = 0x00000000

// Benutzerarten fuer C_Login.
const (
	CkuSO              = 0
	CkuUser            = 1
	CkuContextSpecific = 2
)

// Merkmale (CK_ATTRIBUTE_TYPE).
const (
	CkaClass              = 0x00000000
	CkaToken              = 0x00000001
	CkaPrivate            = 0x00000002
	CkaLabel              = 0x00000003
	CkaValue              = 0x00000011
	CkaCertificateType    = 0x00000080
	CkaIssuer             = 0x00000081
	CkaSerialNumber       = 0x00000082
	CkaKeyType            = 0x00000100
	CkaSubject            = 0x00000101
	CkaID                 = 0x00000102
	CkaSensitive          = 0x00000103
	CkaEncrypt            = 0x00000104
	CkaDecrypt            = 0x00000105
	CkaWrap               = 0x00000106
	CkaUnwrap             = 0x00000107
	CkaSign               = 0x00000108
	CkaSignRecover        = 0x00000109
	CkaVerify             = 0x0000010A
	CkaVerifyRecover      = 0x0000010B
	CkaDerive             = 0x0000010C
	CkaModulus            = 0x00000120
	CkaModulusBits        = 0x00000121
	CkaPublicExponent     = 0x00000122
	CkaExtractable        = 0x00000162
	CkaLocal              = 0x00000163
	CkaNeverExtractable   = 0x00000164
	CkaAlwaysSensitive    = 0x00000165
	CkaModifiable         = 0x00000170
	CkaCopyable           = 0x00000171
	CkaDestroyable        = 0x00000172
	CkaECParams           = 0x00000180
	CkaECPoint            = 0x00000181
	CkaAlwaysAuthenticate = 0x00000202
)

// Verfahren (CK_MECHANISM_TYPE).
const (
	CkmRSAPKCS       = 0x00000001
	CkmSHA1RSAPKCS   = 0x00000006
	CkmSHA256RSAPKCS = 0x00000040
	CkmSHA384RSAPKCS = 0x00000041
	CkmSHA512RSAPKCS = 0x00000042
	CkmECDSA         = 0x00001041
	CkmECDSASHA256   = 0x00001044
	CkmECDSASHA384   = 0x00001045
	CkmECDSASHA512   = 0x00001046
	CkmEDDSA         = 0x00001057
)

// Kennzeichen eines Verfahrens (CK_MECHANISM_INFO.flags).
const (
	CkfHW     = 0x00000001
	CkfSign   = 0x00000800
	CkfVerify = 0x00002000
)

// Kennzeichen eines Steckplatzes (CK_SLOT_INFO.flags).
const (
	CkfTokenPresent    = 0x00000001
	CkfRemovableDevice = 0x00000002
	CkfSlotHW          = 0x00000004
)

// Kennzeichen eines Tokens (CK_TOKEN_INFO.flags).
const (
	CkfRNG                = 0x00000001
	CkfWriteProtected     = 0x00000002
	CkfLoginRequired      = 0x00000004
	CkfUserPinInitialized = 0x00000008
	CkfTokenInitialized   = 0x00000400
	CkfProtectedAuthPath  = 0x00000100
)

// Kennzeichen beim Oeffnen einer Sitzung (C_OpenSession.flags).
const (
	CkfRWSession     = 0x00000002
	CkfSerialSession = 0x00000004
)

// Zustand einer Sitzung (CK_SESSION_INFO.state).
const (
	CksRoPublicSession = 0
	CksRoUserFunctions = 1
)

// Werte, die PKCS#11 fuer "unbekannt" vorsieht.
const (
	CkUnavailableInformation = ^uint64(0)
	CkEffectivelyInfinite    = 0
)

// Pkcs11SlotID ist die Nummer unseres einzigen Steckplatzes. Ein Geraet, ein
// Steckplatz - mehrere Profile sind keine Steckplaetze, sie sind Eintraege
// mit verschiedenen Profilnummern.
const Pkcs11SlotID = 1

// Pkcs11Error ist ein Fehler mit dem Rueckgabewert von PKCS#11. Die C-Seite
// gibt genau diese Zahl zurueck; ein Text waere dort nutzlos, weil kein
// Aufrufer ihn liest.
type Pkcs11Error struct {
	RV  uint64
	Key string
}

func (e *Pkcs11Error) Error() string { return e.Key }

// pkcs11Fail baut einen Fehler mit Rueckgabewert.
func pkcs11Fail(rv uint64, key string) *Pkcs11Error {
	return &Pkcs11Error{RV: rv, Key: key}
}

// Pkcs11RV macht aus einem Fehler den Rueckgabewert. Ein Fehler des
// Geraets wird dabei auf den naechstliegenden Wert von PKCS#11 abgebildet -
// die fremde Software kennt nur diese Zahlen.
func Pkcs11RV(err error) uint64 {
	if err == nil {
		return CkrOK
	}
	if coded, ok := err.(*Pkcs11Error); ok {
		return coded.RV
	}
	problem, ok := err.(*Problem)
	if !ok {
		return CkrGeneralError
	}
	switch problem.Key {
	case "error.device.none", "error.sim.connect", "error.sim.address":
		return CkrTokenNotPresent
	case "error.device.locked":
		return CkrUserNotLoggedIn
	case "error.device.denied":
		return CkrFunctionRejected
	case "error.device.notfound":
		return CkrObjectHandleInvalid
	case "error.device.unsupported":
		return CkrFunctionNotSupported
	case "error.device.io", "error.hid.read", "error.hid.write", "error.hid.timeout":
		return CkrDeviceError
	case "error.device.arg", "error.item.secret", "error.item.type":
		return CkrArgumentsBad
	}
	return CkrFunctionFailed
}
