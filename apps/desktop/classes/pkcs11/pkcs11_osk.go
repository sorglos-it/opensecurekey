//go:build cgo

// Die C-Seite der PKCS#11-Bibliothek.
//
// Hier passiert nichts Fachliches: jede Funktion holt die Angaben aus den
// Zeigern, ruft die Umsetzung in classes/pkcs11_token.go und schreibt das
// Ergebnis zurueck. Alles, was man pruefen kann, steht darum dort - diese
// Datei ist Uebersetzungsarbeit.
//
// Gebaut wird sie als .dll/.so/.dylib:
//
//	go build -buildmode=c-shared -o osk-pkcs11.dll ./classes/pkcs11
//
// Damit erreicht der Tresor Firefox, Thunderbird, OpenSSL, Java, ssh -I und
// VPN-Programme - alle auf einmal, weil alle diese Schnittstelle schon
// kennen.
//
// Zwei Eigenheiten, die man wissen muss:
//
//   - Diese Bibliothek laeuft IM fremden Programm. Welches Programm sie
//     geladen hat, kann sie nicht nachsehen, ohne ihm zu glauben; der Name
//     im Geraeteprotokoll ist darum eine Behauptung, und das ist er bei
//     diesem Weg besonders deutlich (beim ssh-Agenten nennt ihn immerhin das
//     Betriebssystem).
//   - Die Einstellungen kommen aus der Datei, die OSK_CONFIG nennt. Ohne sie
//     gelten die Vorgaben: echtes Geraet, keine Attrappe. Das ist Absicht -
//     eine Bibliothek, die in Firefox steckt, soll nicht nebenbei eine
//     Attrappe im Netz suchen. "Neben dem Programm" waere hier der Ordner
//     von firefox.exe und damit sinnlos.
package main

/*
#include <stdlib.h>
#include "pkcs11_osk.h"
*/
import "C"

import (
	"os"
	"sync"
	"unsafe"

	"osk/classes"
)

func main() {}

// Die Fassung, die C_GetInfo nennt.
const (
	cryptokiMajor = 2
	cryptokiMinor = 40
)

var (
	// stateMu schuetzt alles darunter. PKCS#11 erlaubt Aufrufe aus mehreren
	// Faeden, sobald CKF_OS_LOCKING_OK gesetzt ist, und Firefox nutzt das.
	stateMu sync.Mutex
	token   *classes.Pkcs11Token
	app     *classes.App
)

// --- Allgemeines ---------------------------------------------------------------

//export C_Initialize
func C_Initialize(args C.CK_VOID_PTR) C.CK_RV {
	stateMu.Lock()
	defer stateMu.Unlock()
	if token != nil {
		return C.CK_RV(classes.CkrCryptokiAlreadyInit)
	}
	// pReserved MUSS leer sein, und eigene Sperren des Aufrufers nehmen wir
	// nicht an: unsere Sperren sind die von Go, und zwei Sperrsysteme
	// uebereinander sind ein Verklemmen mit Ansage. Ohne CKF_OS_LOCKING_OK
	// darf die Bibliothek laut Spezifikation nur aus einem Faden benutzt
	// werden - das halten wir ohnehin ein, weil wir selbst sperren.
	switch C.osk_init_args_check(args) {
	case 1:
		return C.CK_RV(classes.CkrArgumentsBad)
	case 2:
		return C.CK_RV(classes.CkrFunctionNotSupported)
	}
	cfg, _ := classes.LoadConfig(configPath())
	app = classes.NewApp(os.DirFS("."), cfg)
	token = classes.NewPkcs11Token(app)
	token.SetHost(hostName())
	return C.CK_RV(classes.CkrOK)
}

// configPath nennt die Einstellungsdatei. Nur OSK_CONFIG - siehe Kopf.
func configPath() string {
	return os.Getenv("OSK_CONFIG")
}

// hostName ist der Name des Programms, das uns geladen hat, soweit er ohne
// Nachfrage zu bekommen ist: der eigene Programmname des Prozesses.
func hostName() string {
	path, err := os.Executable()
	if err != nil {
		return ""
	}
	return path
}

//export C_Finalize
func C_Finalize(reserved C.CK_VOID_PTR) C.CK_RV {
	if reserved != nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	stateMu.Lock()
	defer stateMu.Unlock()
	if token == nil {
		return C.CK_RV(classes.CkrCryptokiNotInitialized)
	}
	token.CloseAllSessions()
	if app != nil {
		app.Disconnect()
	}
	token, app = nil, nil
	return C.CK_RV(classes.CkrOK)
}

//export C_GetInfo
func C_GetInfo(info C.CK_INFO_PTR) C.CK_RV {
	if info == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	manufacturer := C.CString(classes.Pkcs11Manufacturer)
	defer C.free(unsafe.Pointer(manufacturer))
	description := C.CString(classes.Pkcs11Library)
	defer C.free(unsafe.Pointer(description))
	major, minor := versionBytes()
	C.osk_info_fill(info, manufacturer, description,
		C.CK_BYTE(major), C.CK_BYTE(minor), cryptokiMajor, cryptokiMinor)
	return C.CK_RV(classes.CkrOK)
}

//export C_GetFunctionList
func C_GetFunctionList(list C.CK_FUNCTION_LIST_PTR_PTR) C.CK_RV {
	// Diese eine Funktion darf VOR C_Initialize gerufen werden - sie ist der
	// Einstieg, ueber den der Aufrufer alles andere erst findet.
	if list == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	*list = C.osk_function_list()
	return C.CK_RV(classes.CkrOK)
}

//export C_GetInterfaceList
func C_GetInterfaceList(interfaces C.CK_INTERFACE_PTR, count C.CK_ULONG_PTR) C.CK_RV {
	if count == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if interfaces == nil {
		C.osk_ulong_set(count, 1)
		return C.CK_RV(classes.CkrOK)
	}
	if C.osk_ulong_get(count) < 1 {
		C.osk_ulong_set(count, 1)
		return C.CK_RV(classes.CkrBufferTooSmall)
	}
	C.osk_interface_copy(interfaces)
	C.osk_ulong_set(count, 1)
	return C.CK_RV(classes.CkrOK)
}

//export C_GetInterface
func C_GetInterface(name C.CK_UTF8CHAR_PTR, version C.CK_VERSION_PTR,
	iface C.CK_INTERFACE_PTR_PTR, flags C.CK_FLAGS) C.CK_RV {
	if iface == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if name != nil && C.GoString((*C.char)(unsafe.Pointer(name))) != "PKCS 11" {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if version != nil && (C.osk_version_major(version) != cryptokiMajor ||
		C.osk_version_minor(version) != cryptokiMinor) {
		// Nach einer Fassung fragen, die wir nicht haben, ist kein Fehler
		// des Aufrufers - er bekommt nur nichts.
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if flags != 0 {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	*iface = C.osk_interface()
	return C.CK_RV(classes.CkrOK)
}

// --- Steckplatz und Token -------------------------------------------------------

//export C_GetSlotList
func C_GetSlotList(present C.CK_BBOOL, slots C.CK_SLOT_ID_PTR, count C.CK_ULONG_PTR) C.CK_RV {
	if count == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	have := token.SlotPresent()
	stateMu.Unlock()

	// Wir haben genau einen Steckplatz. Fragt der Aufrufer nur nach
	// besetzten und steckt nichts, ist die Antwort eine leere Liste - nicht
	// ein Fehler.
	wanted := 1
	if present == C.CK_TRUE && !have {
		wanted = 0
	}
	if slots == nil {
		C.osk_ulong_set(count, C.CK_ULONG(wanted))
		return C.CK_RV(classes.CkrOK)
	}
	if int(C.osk_ulong_get(count)) < wanted {
		C.osk_ulong_set(count, C.CK_ULONG(wanted))
		return C.CK_RV(classes.CkrBufferTooSmall)
	}
	if wanted == 1 {
		C.osk_slot_set(slots, 0, C.CK_SLOT_ID(classes.Pkcs11SlotID))
	}
	C.osk_ulong_set(count, C.CK_ULONG(wanted))
	return C.CK_RV(classes.CkrOK)
}

//export C_GetSlotInfo
func C_GetSlotInfo(slot C.CK_SLOT_ID, info C.CK_SLOT_INFO_PTR) C.CK_RV {
	if info == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needSlot(slot); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	have := token.SlotPresent()
	stateMu.Unlock()

	flags := uint64(classes.CkfRemovableDevice | classes.CkfSlotHW)
	if have {
		flags |= classes.CkfTokenPresent
	}
	description := C.CString(classes.Pkcs11Library)
	defer C.free(unsafe.Pointer(description))
	manufacturer := C.CString(classes.Pkcs11Manufacturer)
	defer C.free(unsafe.Pointer(manufacturer))
	major, minor := versionBytes()
	C.osk_slot_info_fill(info, description, manufacturer, C.CK_FLAGS(flags),
		C.CK_BYTE(major), C.CK_BYTE(minor))
	return C.CK_RV(classes.CkrOK)
}

//export C_GetTokenInfo
func C_GetTokenInfo(slot C.CK_SLOT_ID, info C.CK_TOKEN_INFO_PTR) C.CK_RV {
	if info == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needSlot(slot); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	label, serial, firmware := token.TokenLabel()
	stateMu.Unlock()
	if label == "" {
		return C.CK_RV(classes.CkrTokenNotPresent)
	}

	// Schreibgeschuetzt, Anmeldung noetig, PIN ist eingerichtet. Mehr sagen
	// wir nicht zu: insbesondere KEIN CKF_RNG - Zufall gibt dieses Token
	// nicht heraus.
	flags := uint64(classes.CkfWriteProtected | classes.CkfLoginRequired |
		classes.CkfUserPinInitialized | classes.CkfTokenInitialized)
	cLabel := C.CString(label)
	defer C.free(unsafe.Pointer(cLabel))
	manufacturer := C.CString(classes.Pkcs11Manufacturer)
	defer C.free(unsafe.Pointer(manufacturer))
	model := C.CString("OpenSecureKey")
	defer C.free(unsafe.Pointer(model))
	cSerial := C.CString(serial)
	defer C.free(unsafe.Pointer(cSerial))
	firmwareMajor, firmwareMinor := splitVersion(firmware)
	C.osk_token_info_fill(info, cLabel, manufacturer, model, cSerial,
		C.CK_FLAGS(flags), 4, 64,
		C.CK_BYTE(firmwareMajor), C.CK_BYTE(firmwareMinor))
	return C.CK_RV(classes.CkrOK)
}

//export C_GetMechanismList
func C_GetMechanismList(slot C.CK_SLOT_ID, list C.CK_MECHANISM_TYPE_PTR,
	count C.CK_ULONG_PTR) C.CK_RV {
	if count == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needSlot(slot); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	mechanisms := token.Mechanisms()
	stateMu.Unlock()

	if list == nil {
		C.osk_ulong_set(count, C.CK_ULONG(len(mechanisms)))
		return C.CK_RV(classes.CkrOK)
	}
	if int(C.osk_ulong_get(count)) < len(mechanisms) {
		C.osk_ulong_set(count, C.CK_ULONG(len(mechanisms)))
		return C.CK_RV(classes.CkrBufferTooSmall)
	}
	for index, mechanism := range mechanisms {
		C.osk_mech_list_set(list, C.CK_ULONG(index), C.CK_MECHANISM_TYPE(mechanism))
	}
	C.osk_ulong_set(count, C.CK_ULONG(len(mechanisms)))
	return C.CK_RV(classes.CkrOK)
}

//export C_GetMechanismInfo
func C_GetMechanismInfo(slot C.CK_SLOT_ID, mechanism C.CK_MECHANISM_TYPE,
	info C.CK_MECHANISM_INFO_PTR) C.CK_RV {
	if info == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needSlot(slot); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	min, max, flags, err := token.MechanismInfo(uint64(mechanism))
	stateMu.Unlock()
	if err != nil {
		return C.CK_RV(classes.Pkcs11RV(err))
	}
	C.osk_mech_info_fill(info, C.CK_ULONG(min), C.CK_ULONG(max), C.CK_FLAGS(flags))
	return C.CK_RV(classes.CkrOK)
}

// --- Sitzungen ------------------------------------------------------------------

//export C_OpenSession
func C_OpenSession(slot C.CK_SLOT_ID, flags C.CK_FLAGS, application C.CK_VOID_PTR,
	notify C.CK_NOTIFY, session C.CK_SESSION_HANDLE_PTR) C.CK_RV {
	if session == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needSlot(slot); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	// CKF_RW_SESSION wird angenommen, nicht abgelehnt - warum, steht
	// ausfuehrlich bei Pkcs11Token.OpenSession. Kurz: OpenSSH fragt immer
	// danach, und schreiben kann hier ohnehin niemand.
	stateMu.Lock()
	handle, err := token.OpenSession(uint64(flags))
	stateMu.Unlock()
	if err != nil {
		return C.CK_RV(classes.Pkcs11RV(err))
	}
	*session = C.CK_SESSION_HANDLE(handle)
	return C.CK_RV(classes.CkrOK)
}

//export C_CloseSession
func C_CloseSession(session C.CK_SESSION_HANDLE) C.CK_RV {
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	err := token.CloseSession(uint64(session))
	stateMu.Unlock()
	return C.CK_RV(classes.Pkcs11RV(err))
}

//export C_CloseAllSessions
func C_CloseAllSessions(slot C.CK_SLOT_ID) C.CK_RV {
	if rv := needSlot(slot); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	err := token.CloseAllSessions()
	stateMu.Unlock()
	return C.CK_RV(classes.Pkcs11RV(err))
}

//export C_GetSessionInfo
func C_GetSessionInfo(session C.CK_SESSION_HANDLE, info C.CK_SESSION_INFO_PTR) C.CK_RV {
	if info == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	state, err := token.SessionState(uint64(session))
	stateMu.Unlock()
	if err != nil {
		return C.CK_RV(classes.Pkcs11RV(err))
	}
	C.osk_session_info_fill(info, C.CK_SLOT_ID(classes.Pkcs11SlotID),
		C.CK_STATE(state), C.CK_FLAGS(classes.CkfSerialSession))
	return C.CK_RV(classes.CkrOK)
}

//export C_Login
func C_Login(session C.CK_SESSION_HANDLE, user C.CK_USER_TYPE,
	pin C.CK_UTF8CHAR_PTR, length C.CK_ULONG) C.CK_RV {
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	var secret []byte
	if pin != nil && length > 0 {
		if length > 64 {
			return C.CK_RV(classes.CkrPinIncorrect)
		}
		secret = C.GoBytes(unsafe.Pointer(pin), C.int(length))
	}
	stateMu.Lock()
	err := token.Login(uint64(session), uint64(user), secret)
	stateMu.Unlock()
	return C.CK_RV(classes.Pkcs11RV(err))
}

//export C_Logout
func C_Logout(session C.CK_SESSION_HANDLE) C.CK_RV {
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	err := token.Logout(uint64(session))
	stateMu.Unlock()
	return C.CK_RV(classes.Pkcs11RV(err))
}

// --- Objekte --------------------------------------------------------------------

//export C_FindObjectsInit
func C_FindObjectsInit(session C.CK_SESSION_HANDLE, template C.CK_ATTRIBUTE_PTR,
	count C.CK_ULONG) C.CK_RV {
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	wanted, err := readTemplate(template, count)
	if err != nil {
		return C.CK_RV(classes.Pkcs11RV(err))
	}
	stateMu.Lock()
	_, findErr := token.FindObjects(uint64(session), wanted)
	stateMu.Unlock()
	return C.CK_RV(classes.Pkcs11RV(findErr))
}

//export C_FindObjects
func C_FindObjects(session C.CK_SESSION_HANDLE, objects C.CK_OBJECT_HANDLE_PTR,
	max C.CK_ULONG, count C.CK_ULONG_PTR) C.CK_RV {
	if count == nil || (objects == nil && max > 0) {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	found, err := token.TakeFound(uint64(session), int(max))
	stateMu.Unlock()
	if err != nil {
		return C.CK_RV(classes.Pkcs11RV(err))
	}
	for index, handle := range found {
		C.osk_object_set(objects, C.CK_ULONG(index), C.CK_OBJECT_HANDLE(handle))
	}
	C.osk_ulong_set(count, C.CK_ULONG(len(found)))
	return C.CK_RV(classes.CkrOK)
}

//export C_FindObjectsFinal
func C_FindObjectsFinal(session C.CK_SESSION_HANDLE) C.CK_RV {
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	stateMu.Lock()
	err := token.FindObjectsFinal(uint64(session))
	stateMu.Unlock()
	return C.CK_RV(classes.Pkcs11RV(err))
}

//export C_GetObjectSize
func C_GetObjectSize(session C.CK_SESSION_HANDLE, object C.CK_OBJECT_HANDLE,
	size C.CK_ULONG_PTR) C.CK_RV {
	if size == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	// PKCS#11 erlaubt "unbekannt" ausdruecklich. Eine Zahl zu nennen hiesse,
	// etwas ueber den Platz im Chip zu behaupten.
	unknown := C.CK_ULONG(0)
	unknown--
	C.osk_ulong_set(size, unknown)
	return C.CK_RV(classes.CkrOK)
}

//export C_GetAttributeValue
func C_GetAttributeValue(session C.CK_SESSION_HANDLE, object C.CK_OBJECT_HANDLE,
	template C.CK_ATTRIBUTE_PTR, count C.CK_ULONG) C.CK_RV {
	if template == nil || count == 0 {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	// PKCS#11 will, dass ALLE Merkmale bearbeitet werden und am Ende der
	// erste Fehler zurueckkommt - nicht, dass beim ersten Fehler abgebrochen
	// wird. Sonst sieht der Aufrufer die uebrigen Werte nie.
	result := C.CK_RV(classes.CkrOK)
	for index := C.CK_ULONG(0); index < count; index++ {
		kind := uint64(C.osk_attr_type(template, index))
		stateMu.Lock()
		value, err := token.Attribute(uint64(object), kind)
		stateMu.Unlock()
		if err != nil {
			// So verlangt es die Spezifikation: Laenge auf -1, damit der
			// Aufrufer sieht, WELCHES Merkmal gefehlt hat.
			C.osk_attr_unknown(template, index)
			if result == C.CK_RV(classes.CkrOK) {
				result = C.CK_RV(classes.Pkcs11RV(err))
			}
			continue
		}
		if rv := writeValue(template, index, value); rv != C.CK_RV(classes.CkrOK) {
			if result == C.CK_RV(classes.CkrOK) {
				result = rv
			}
		}
	}
	return result
}

// --- Unterschreiben -------------------------------------------------------------

//export C_SignInit
func C_SignInit(session C.CK_SESSION_HANDLE, mechanism C.CK_MECHANISM_PTR,
	key C.CK_OBJECT_HANDLE) C.CK_RV {
	if mechanism == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	// Keines unserer Verfahren hat Parameter. Sind welche dabei, meint der
	// Aufrufer etwas anderes, als wir tun wuerden.
	if C.osk_mech_param_len(mechanism) != 0 || C.osk_mech_param(mechanism) != nil {
		return C.CK_RV(classes.CkrMechanismInvalid)
	}
	stateMu.Lock()
	err := token.SignInit(uint64(session), uint64(key),
		uint64(C.osk_mech_type(mechanism)))
	stateMu.Unlock()
	return C.CK_RV(classes.Pkcs11RV(err))
}

//export C_Sign
func C_Sign(session C.CK_SESSION_HANDLE, data C.CK_BYTE_PTR, length C.CK_ULONG,
	signature C.CK_BYTE_PTR, signatureLen C.CK_ULONG_PTR) C.CK_RV {
	if signatureLen == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	// Der uebliche erste Aufruf: nur nach der Laenge fragen. Dabei darf der
	// Vorgang NICHT beendet werden - der Aufrufer ruft gleich nochmal.
	if signature == nil {
		stateMu.Lock()
		size, err := token.Pkcs11SignatureSize(uint64(session))
		stateMu.Unlock()
		if err != nil {
			return C.CK_RV(classes.Pkcs11RV(err))
		}
		C.osk_ulong_set(signatureLen, C.CK_ULONG(size))
		return C.CK_RV(classes.CkrOK)
	}
	var input []byte
	if data != nil && length > 0 {
		input = C.GoBytes(unsafe.Pointer(data), C.int(length))
	}
	stateMu.Lock()
	out, err := token.Sign(uint64(session), input)
	stateMu.Unlock()
	classes.Wipe(input)
	if err != nil {
		return C.CK_RV(classes.Pkcs11RV(err))
	}
	if int(C.osk_ulong_get(signatureLen)) < len(out) {
		C.osk_ulong_set(signatureLen, C.CK_ULONG(len(out)))
		return C.CK_RV(classes.CkrBufferTooSmall)
	}
	C.osk_bytes_copy(signature, unsafe.Pointer(&out[0]), C.CK_ULONG(len(out)))
	C.osk_ulong_set(signatureLen, C.CK_ULONG(len(out)))
	return C.CK_RV(classes.CkrOK)
}

//export C_SignUpdate
func C_SignUpdate(session C.CK_SESSION_HANDLE, part C.CK_BYTE_PTR, length C.CK_ULONG) C.CK_RV {
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	var input []byte
	if part != nil && length > 0 {
		input = C.GoBytes(unsafe.Pointer(part), C.int(length))
	}
	stateMu.Lock()
	err := token.SignUpdate(uint64(session), input)
	stateMu.Unlock()
	classes.Wipe(input)
	return C.CK_RV(classes.Pkcs11RV(err))
}

//export C_SignFinal
func C_SignFinal(session C.CK_SESSION_HANDLE, signature C.CK_BYTE_PTR,
	signatureLen C.CK_ULONG_PTR) C.CK_RV {
	if signatureLen == nil {
		return C.CK_RV(classes.CkrArgumentsBad)
	}
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	if signature == nil {
		stateMu.Lock()
		size, err := token.Pkcs11SignatureSize(uint64(session))
		stateMu.Unlock()
		if err != nil {
			return C.CK_RV(classes.Pkcs11RV(err))
		}
		C.osk_ulong_set(signatureLen, C.CK_ULONG(size))
		return C.CK_RV(classes.CkrOK)
	}
	stateMu.Lock()
	out, err := token.SignFinal(uint64(session))
	stateMu.Unlock()
	if err != nil {
		return C.CK_RV(classes.Pkcs11RV(err))
	}
	if int(C.osk_ulong_get(signatureLen)) < len(out) {
		C.osk_ulong_set(signatureLen, C.CK_ULONG(len(out)))
		return C.CK_RV(classes.CkrBufferTooSmall)
	}
	C.osk_bytes_copy(signature, unsafe.Pointer(&out[0]), C.CK_ULONG(len(out)))
	C.osk_ulong_set(signatureLen, C.CK_ULONG(len(out)))
	return C.CK_RV(classes.CkrOK)
}

// --- Was PKCS#11 verlangt, aber hier nichts tut ---------------------------------

//export C_GetFunctionStatus
func C_GetFunctionStatus(session C.CK_SESSION_HANDLE) C.CK_RV {
	// So steht es in der Spezifikation: diese Funktion gibt es nur noch aus
	// Ruecksicht auf alte Programme, und sie antwortet immer dasselbe.
	return C.CK_RV(0x00000051) // CKR_FUNCTION_NOT_PARALLEL
}

//export C_CancelFunction
func C_CancelFunction(session C.CK_SESSION_HANDLE) C.CK_RV {
	return C.CK_RV(0x00000051) // CKR_FUNCTION_NOT_PARALLEL
}

//export C_WaitForSlotEvent
func C_WaitForSlotEvent(flags C.CK_FLAGS, slot C.CK_SLOT_ID_PTR, reserved C.CK_VOID_PTR) C.CK_RV {
	if rv := needToken(); rv != classes.CkrOK {
		return C.CK_RV(rv)
	}
	// Wir melden kein Stecken und kein Ziehen. "Keine Nachricht" ist dafuer
	// der vorgesehene Rueckgabewert - blockierendes Warten waere eine
	// Zusage, die wir nicht halten koennen.
	return C.CK_RV(classes.CkrFunctionNotSupported)
}

// --- Helfer ---------------------------------------------------------------------

// needToken prueft, dass C_Initialize gelaufen ist.
func needToken() uint64 {
	stateMu.Lock()
	defer stateMu.Unlock()
	if token == nil {
		return classes.CkrCryptokiNotInitialized
	}
	return classes.CkrOK
}

// needSlot prueft Token und Steckplatznummer.
func needSlot(slot C.CK_SLOT_ID) uint64 {
	if rv := needToken(); rv != classes.CkrOK {
		return rv
	}
	if uint64(slot) != classes.Pkcs11SlotID {
		return classes.CkrSlotIDInvalid
	}
	return classes.CkrOK
}

// readTemplate liest eine Suchvorlage. Werte in Groesse eines CK_ULONG oder
// eines CK_BBOOL werden als Zahl bzw. Wahrheitswert gelesen - genau so
// schreibt sie der Aufrufer, und die Groessen kommen vom C-Uebersetzer, nicht
// aus einer Annahme.
func readTemplate(template C.CK_ATTRIBUTE_PTR, count C.CK_ULONG) ([]classes.Pkcs11Attribute, error) {
	if count == 0 || template == nil {
		return nil, nil
	}
	if count > 256 {
		// Eine Vorlage mit tausenden Merkmalen ist kein Suchwunsch.
		return nil, classes.Pkcs11ErrorFor(classes.CkrArgumentsBad)
	}
	out := make([]classes.Pkcs11Attribute, 0, int(count))
	ulongSize := C.CK_ULONG(C.osk_sizeof_ulong())
	boolSize := C.CK_ULONG(C.osk_sizeof_bbool())
	for index := C.CK_ULONG(0); index < count; index++ {
		kind := uint64(C.osk_attr_type(template, index))
		value := C.osk_attr_value(template, index)
		length := C.osk_attr_len(template, index)
		wanted := classes.Pkcs11Attribute{Type: kind}
		switch {
		case value == nil || length == 0:
			wanted.Value = classes.Pkcs11BytesValue(nil)
		case length == boolSize && isBoolAttribute(kind):
			raw := C.GoBytes(unsafe.Pointer(value), C.int(length))
			wanted.Value = classes.Pkcs11BoolValue(raw[0] != 0)
		case length == ulongSize && isUlongAttribute(kind):
			raw := C.GoBytes(unsafe.Pointer(value), C.int(length))
			wanted.Value = classes.Pkcs11UlongValue(ulongFromBytes(raw))
		default:
			wanted.Value = classes.Pkcs11BytesValue(C.GoBytes(unsafe.Pointer(value), C.int(length)))
		}
		out = append(out, wanted)
	}
	return out, nil
}

// isBoolAttribute nennt die Merkmale, die ein Wahrheitswert sind. Ohne diese
// Liste waere ein einzelnes Byte nicht von einem Wahrheitswert zu
// unterscheiden - und CKA_ID mit einem Byte Laenge kommt vor.
func isBoolAttribute(kind uint64) bool {
	switch kind {
	case classes.CkaToken, classes.CkaPrivate, classes.CkaSensitive,
		classes.CkaEncrypt, classes.CkaDecrypt, classes.CkaWrap,
		classes.CkaUnwrap, classes.CkaSign, classes.CkaSignRecover,
		classes.CkaVerify, classes.CkaVerifyRecover, classes.CkaDerive,
		classes.CkaExtractable, classes.CkaLocal, classes.CkaNeverExtractable,
		classes.CkaAlwaysSensitive, classes.CkaModifiable, classes.CkaCopyable,
		classes.CkaDestroyable, classes.CkaAlwaysAuthenticate:
		return true
	}
	return false
}

// isUlongAttribute nennt die Merkmale, die eine Zahl sind.
func isUlongAttribute(kind uint64) bool {
	switch kind {
	case classes.CkaClass, classes.CkaKeyType, classes.CkaCertificateType,
		classes.CkaModulusBits:
		return true
	}
	return false
}

// ulongFromBytes liest ein CK_ULONG in der Reihenfolge dieses Rechners.
func ulongFromBytes(raw []byte) uint64 {
	value := uint64(0)
	for index := len(raw) - 1; index >= 0; index-- {
		value = value<<8 | uint64(raw[index])
	}
	return value
}

// writeValue schreibt einen Merkmalswert in ein CK_ATTRIBUTE. Die Groessen
// von CK_ULONG und CK_BBOOL kommen vom C-Uebersetzer, nicht aus einer
// Annahme: unter Windows ist CK_ULONG vier Bytes breit, unter Linux acht.
func writeValue(template C.CK_ATTRIBUTE_PTR, index C.CK_ULONG,
	value classes.Pkcs11Value) C.CK_RV {
	var raw []byte
	switch value.Kind {
	case classes.Pkcs11Bool:
		raw = make([]byte, int(C.osk_sizeof_bbool()))
		if value.Bool {
			raw[0] = 1
		}
	case classes.Pkcs11Ulong:
		size := int(C.osk_sizeof_ulong())
		raw = make([]byte, size)
		rest := value.Ulong
		for position := 0; position < size; position++ {
			raw[position] = byte(rest)
			rest >>= 8
		}
	default:
		raw = value.Bytes
	}
	if len(raw) == 0 {
		return C.CK_RV(C.osk_attr_write(template, index, nil, 0))
	}
	return C.CK_RV(C.osk_attr_write(template, index,
		unsafe.Pointer(&raw[0]), C.CK_ULONG(len(raw))))
}

// versionBytes macht aus der Fassung des Programms zwei Zahlen.
func versionBytes() (byte, byte) {
	return splitVersion(classes.Version)
}

// splitVersion liest die ersten zwei Zahlen aus einer Fassung wie 0.0.1.0.
func splitVersion(text string) (byte, byte) {
	parts := [2]byte{}
	which, number := 0, 0
	for index := 0; index <= len(text); index++ {
		if index == len(text) || text[index] == '.' || text[index] == '-' {
			if which < 2 {
				if number > 255 {
					number = 255
				}
				parts[which] = byte(number)
			}
			which++
			number = 0
			if index < len(text) && text[index] == '-' {
				break
			}
			continue
		}
		if text[index] < '0' || text[index] > '9' {
			break
		}
		number = number*10 + int(text[index]-'0')
	}
	return parts[0], parts[1]
}
