//go:build windows

// HID unter Windows: Geraete finden ueber SetupAPI, Eigenschaften ueber
// hid.dll, lesen und schreiben ueberlappend, damit ein Zeitlimit moeglich ist.
// Ohne cgo - nur syscall und die DLLs, die Windows selbst mitbringt.
package classes

import (
	"sync"
	"syscall"
	"time"
	"unsafe"
)

var (
	dllSetupAPI = syscall.NewLazyDLL("setupapi.dll")
	dllHid      = syscall.NewLazyDLL("hid.dll")
	dllKernel   = syscall.NewLazyDLL("kernel32.dll")

	procGetClassDevs      = dllSetupAPI.NewProc("SetupDiGetClassDevsW")
	procDestroyDeviceList = dllSetupAPI.NewProc("SetupDiDestroyDeviceInfoList")
	procEnumInterfaces    = dllSetupAPI.NewProc("SetupDiEnumDeviceInterfaces")
	procInterfaceDetail   = dllSetupAPI.NewProc("SetupDiGetDeviceInterfaceDetailW")

	procHidGuid       = dllHid.NewProc("HidD_GetHidGuid")
	procHidAttributes = dllHid.NewProc("HidD_GetAttributes")
	procHidPreparsed  = dllHid.NewProc("HidD_GetPreparsedData")
	procHidFreeData   = dllHid.NewProc("HidD_FreePreparsedData")
	procHidCaps       = dllHid.NewProc("HidP_GetCaps")
	procHidProduct    = dllHid.NewProc("HidD_GetProductString")
	procHidSerial     = dllHid.NewProc("HidD_GetSerialNumberString")
	procCancelIoEx    = dllKernel.NewProc("CancelIoEx")
	procCreateEvent   = dllKernel.NewProc("CreateEventW")
	procResetEvent    = dllKernel.NewProc("ResetEvent")
	procOverlapped    = dllKernel.NewProc("GetOverlappedResult")
)

const (
	digcfPresent         = 0x02
	digcfDeviceInterface = 0x10
	errInsufficientBuf   = 122
	errNoMoreItems       = 259
)

type guid struct {
	Data1 uint32
	Data2 uint16
	Data3 uint16
	Data4 [8]byte
}

type spDeviceInterfaceData struct {
	Size               uint32
	InterfaceClassGuid guid
	Flags              uint32
	Reserved           uintptr
}

type hidAttributes struct {
	Size      uint32
	VendorID  uint16
	ProductID uint16
	Version   uint16
}

// hidCaps ist der Anfang von HIDP_CAPS; mehr brauchen wir nicht, der Rest
// wird nur als Platz mitgefuehrt.
type hidCaps struct {
	Usage                     uint16
	UsagePage                 uint16
	InputReportByteLength     uint16
	OutputReportByteLength    uint16
	FeatureReportByteLength   uint16
	Reserved                  [17]uint16
	NumberLinkCollectionNodes uint16
	NumberInputButtonCaps     uint16
	NumberInputValueCaps      uint16
	NumberInputDataIndices    uint16
	NumberOutputButtonCaps    uint16
	NumberOutputValueCaps     uint16
	NumberOutputDataIndices   uint16
	NumberFeatureButtonCaps   uint16
	NumberFeatureValueCaps    uint16
	NumberFeatureDataIndices  uint16
}

// HidEnumerate zaehlt alle HID-Geraete des Rechners auf.
func HidEnumerate() ([]HidInfo, error) {
	var classGuid guid
	procHidGuid.Call(uintptr(unsafe.Pointer(&classGuid)))

	set, _, err := procGetClassDevs.Call(uintptr(unsafe.Pointer(&classGuid)), 0, 0,
		digcfPresent|digcfDeviceInterface)
	if set == 0 || set == ^uintptr(0) {
		return nil, Fail("error.hid.enumerate", errText(err))
	}
	defer procDestroyDeviceList.Call(set)

	var out []HidInfo
	for index := uint32(0); ; index++ {
		data := spDeviceInterfaceData{Size: uint32(unsafe.Sizeof(spDeviceInterfaceData{}))}
		ok, _, callErr := procEnumInterfaces.Call(set, 0,
			uintptr(unsafe.Pointer(&classGuid)), uintptr(index),
			uintptr(unsafe.Pointer(&data)))
		if ok == 0 {
			if errNumber(callErr) == errNoMoreItems {
				break
			}
			break
		}
		path, pathErr := interfacePath(set, &data)
		if pathErr != nil || path == "" {
			continue
		}
		info, infoErr := describeDevice(path)
		if infoErr != nil {
			continue
		}
		out = append(out, info)
	}
	return out, nil
}

// interfacePath holt den Geraetepfad in zwei Schritten: erst die Groesse,
// dann die Daten.
func interfacePath(set uintptr, data *spDeviceInterfaceData) (string, error) {
	var needed uint32
	procInterfaceDetail.Call(set, uintptr(unsafe.Pointer(data)), 0, 0,
		uintptr(unsafe.Pointer(&needed)), 0)
	if needed < 8 || needed > 4096 {
		return "", Fail("error.hid.enumerate", "detail")
	}
	buf := make([]byte, needed)
	// SP_DEVICE_INTERFACE_DETAIL_DATA_W beginnt mit cbSize; 64-Bit erwartet 8.
	*(*uint32)(unsafe.Pointer(&buf[0])) = 8
	ok, _, err := procInterfaceDetail.Call(set, uintptr(unsafe.Pointer(data)),
		uintptr(unsafe.Pointer(&buf[0])), uintptr(needed),
		uintptr(unsafe.Pointer(&needed)), 0)
	if ok == 0 {
		return "", Fail("error.hid.enumerate", errText(err))
	}
	chars := (*[2048]uint16)(unsafe.Pointer(&buf[4]))[: (needed-4)/2 : (needed-4)/2]
	return utf16Text(chars), nil
}

// describeDevice oeffnet das Geraet nur lesend-ohne-Zugriff, um Kennungen
// abzufragen. Das gelingt auch, wenn ein anderes Programm es benutzt.
func describeDevice(path string) (HidInfo, error) {
	handle, err := openDevice(path, false)
	if err != nil {
		return HidInfo{}, err
	}
	defer syscall.CloseHandle(handle)

	attrs := hidAttributes{Size: uint32(unsafe.Sizeof(hidAttributes{}))}
	if ok, _, _ := procHidAttributes.Call(uintptr(handle), uintptr(unsafe.Pointer(&attrs))); ok == 0 {
		return HidInfo{}, Fail("error.hid.open", path)
	}
	info := HidInfo{
		Path:      path,
		VendorID:  attrs.VendorID,
		ProductID: attrs.ProductID,
		Product:   deviceText(handle, procHidProduct),
		Serial:    deviceText(handle, procHidSerial),
	}
	if caps, ok := deviceCaps(handle); ok {
		info.UsagePage = caps.UsagePage
		info.Usage = caps.Usage
	}
	return info, nil
}

func deviceCaps(handle syscall.Handle) (hidCaps, bool) {
	var preparsed uintptr
	if ok, _, _ := procHidPreparsed.Call(uintptr(handle), uintptr(unsafe.Pointer(&preparsed))); ok == 0 {
		return hidCaps{}, false
	}
	defer procHidFreeData.Call(preparsed)
	var caps hidCaps
	if status, _, _ := procHidCaps.Call(preparsed, uintptr(unsafe.Pointer(&caps))); status != 0x00110000 {
		return hidCaps{}, false
	}
	return caps, true
}

func deviceText(handle syscall.Handle, proc *syscall.LazyProc) string {
	buf := make([]uint16, 128)
	ok, _, _ := proc.Call(uintptr(handle), uintptr(unsafe.Pointer(&buf[0])),
		uintptr(len(buf)*2))
	if ok == 0 {
		return ""
	}
	return utf16Text(buf)
}

func openDevice(path string, forIO bool) (syscall.Handle, error) {
	wide, err := syscall.UTF16PtrFromString(path)
	if err != nil {
		return syscall.InvalidHandle, Fail("error.hid.open", path)
	}
	access := uint32(0)
	flags := uint32(0)
	if forIO {
		access = syscall.GENERIC_READ | syscall.GENERIC_WRITE
		flags = syscall.FILE_FLAG_OVERLAPPED
	}
	handle, err := syscall.CreateFile(wide, access,
		syscall.FILE_SHARE_READ|syscall.FILE_SHARE_WRITE, nil,
		syscall.OPEN_EXISTING, flags, 0)
	if err != nil {
		return syscall.InvalidHandle, Fail("error.hid.open", path)
	}
	return handle, nil
}

// winHid ist eine offene Verbindung. Lesen und Schreiben laufen ueberlappend
// mit je eigenem Ereignis, damit ein Zeitlimit wirklich abbricht.
type winHid struct {
	handle     syscall.Handle
	path       string
	label      string
	inputSize  int
	outputSize int
	mu         sync.Mutex
	readEvent  syscall.Handle
	writeEvent syscall.Handle
}

// HidOpen oeffnet das Geraet zum Austausch von Berichten.
func HidOpen(path string) (Transport, error) {
	info, err := describeDevice(path)
	if err != nil {
		return nil, err
	}
	caps := hidCaps{}
	handle, err := openDevice(path, true)
	if err != nil {
		return nil, err
	}
	if c, ok := deviceCaps(handle); ok {
		caps = c
	}
	readEvent, evErr := createEvent()
	if evErr != nil {
		syscall.CloseHandle(handle)
		return nil, Fail("error.hid.open", path)
	}
	writeEvent, evErr := createEvent()
	if evErr != nil {
		syscall.CloseHandle(readEvent)
		syscall.CloseHandle(handle)
		return nil, Fail("error.hid.open", path)
	}
	// Windows erwartet immer die vollen Berichtslaengen aus den Caps, mit
	// der Berichtsnummer als erstem Byte. Fehlen die Angaben, nehmen wir
	// 64 Byte plus Nummer an.
	in := int(caps.InputReportByteLength)
	outLen := int(caps.OutputReportByteLength)
	if in < ReportSize+1 {
		in = ReportSize + 1
	}
	if outLen < ReportSize+1 {
		outLen = ReportSize + 1
	}
	return &winHid{
		handle:     handle,
		path:       path,
		label:      info.Label(),
		inputSize:  in,
		outputSize: outLen,
		readEvent:  readEvent,
		writeEvent: writeEvent,
	}, nil
}

func (w *winHid) Describe() string { return w.label }

func (w *winHid) WriteReport(report []byte) error {
	if len(report) != ReportSize {
		return Fail("error.frame.size")
	}
	w.mu.Lock()
	defer w.mu.Unlock()

	buf := make([]byte, w.outputSize)
	copy(buf[1:], report) // buf[0] bleibt 0: Bericht ohne Nummer
	var overlapped syscall.Overlapped
	overlapped.HEvent = w.writeEvent
	resetEvent(w.writeEvent)
	var written uint32
	err := syscall.WriteFile(w.handle, buf, &written, &overlapped)
	if err != nil && err != syscall.ERROR_IO_PENDING {
		return Fail("error.hid.write", errText(err))
	}
	if err == syscall.ERROR_IO_PENDING {
		if waitErr := w.wait(w.writeEvent, &overlapped, shortTimeout, &written); waitErr != nil {
			return waitErr
		}
	}
	if int(written) != len(buf) {
		return Fail("error.hid.write", "short")
	}
	return nil
}

func (w *winHid) ReadReport(timeout time.Duration) ([]byte, error) {
	w.mu.Lock()
	defer w.mu.Unlock()

	buf := make([]byte, w.inputSize)
	var overlapped syscall.Overlapped
	overlapped.HEvent = w.readEvent
	resetEvent(w.readEvent)
	var read uint32
	err := syscall.ReadFile(w.handle, buf, &read, &overlapped)
	if err != nil && err != syscall.ERROR_IO_PENDING {
		return nil, Fail("error.hid.read", errText(err))
	}
	if err == syscall.ERROR_IO_PENDING {
		if waitErr := w.wait(w.readEvent, &overlapped, timeout, &read); waitErr != nil {
			return nil, waitErr
		}
	}
	return trimReport(buf[:read])
}

// wait wartet auf das Ende des ueberlappenden Zugriffs und bricht ihn beim
// Zeitlimit ab, damit kein Zugriff im Hintergrund weiterlaeuft.
func (w *winHid) wait(event syscall.Handle, overlapped *syscall.Overlapped,
	timeout time.Duration, done *uint32) error {
	ms := uint32(timeout / time.Millisecond)
	if ms == 0 {
		ms = 1
	}
	state, waitErr := syscall.WaitForSingleObject(event, ms)
	if state != 0 || waitErr != nil {
		procCancelIoEx.Call(uintptr(w.handle), uintptr(unsafe.Pointer(overlapped)))
		syscall.WaitForSingleObject(event, 1000)
		return Fail("error.hid.timeout")
	}
	ok, _, resErr := procOverlapped.Call(uintptr(w.handle),
		uintptr(unsafe.Pointer(overlapped)), uintptr(unsafe.Pointer(done)), 0)
	if ok == 0 {
		return Fail("error.hid.read", errText(resErr))
	}
	return nil
}

func (w *winHid) Close() error {
	w.mu.Lock()
	defer w.mu.Unlock()
	syscall.CloseHandle(w.readEvent)
	syscall.CloseHandle(w.writeEvent)
	return syscall.CloseHandle(w.handle)
}

// trimReport nimmt die fuehrende Berichtsnummer weg, wenn sie dabei ist.
func trimReport(data []byte) ([]byte, error) {
	switch {
	case len(data) == ReportSize:
		out := make([]byte, ReportSize)
		copy(out, data)
		return out, nil
	case len(data) == ReportSize+1 && data[0] == 0:
		out := make([]byte, ReportSize)
		copy(out, data[1:])
		return out, nil
	case len(data) > ReportSize+1:
		out := make([]byte, ReportSize)
		copy(out, data[1:ReportSize+1])
		return out, nil
	default:
		return nil, Fail("error.frame.size")
	}
}

// createEvent legt ein Ereignis an, das von Hand zurueckgesetzt wird.
func createEvent() (syscall.Handle, error) {
	handle, _, err := procCreateEvent.Call(0, 1, 0, 0)
	if handle == 0 {
		return syscall.InvalidHandle, err
	}
	return syscall.Handle(handle), nil
}

func resetEvent(handle syscall.Handle) {
	procResetEvent.Call(uintptr(handle))
}

func utf16Text(buf []uint16) string {
	for i, c := range buf {
		if c == 0 {
			return syscall.UTF16ToString(buf[:i])
		}
	}
	return syscall.UTF16ToString(buf)
}

func errNumber(err error) uintptr {
	if e, ok := err.(syscall.Errno); ok {
		return uintptr(e)
	}
	return 0
}

func errText(err error) string {
	if err == nil {
		return ""
	}
	if e, ok := err.(syscall.Errno); ok && e == 0 {
		return ""
	}
	return err.Error()
}

var _ = errInsufficientBuf
