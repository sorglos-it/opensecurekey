//go:build darwin && cgo

// HID unter macOS: IOKit. Das ist die einzige Datei, die cgo braucht - macOS
// bietet keinen Weg zu HID-Geraeten, der ohne die C-Schnittstelle auskommt.
// Windows und Linux bleiben davon unberuehrt und bauen ohne cgo.
//
// Nicht geprueft: hier stand kein Mac zur Verfuegung. Steht einer bereit,
// zuerst HidEnumerate gegen ein beliebiges HID-Geraet pruefen.
package classes

/*
#cgo LDFLAGS: -framework IOKit -framework CoreFoundation
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <string.h>

extern void oskInputReport(long handle, unsigned char *data, long length);

static void oskReportCallback(void *context, IOReturn result, void *sender,
                              IOHIDReportType type, uint32_t reportID,
                              uint8_t *report, CFIndex length) {
	(void)result; (void)sender; (void)type; (void)reportID;
	oskInputReport((long)context, report, (long)length);
}

// oskManager liefert einen Manager, der alle HID-Geraete sieht.
static IOHIDManagerRef oskManager(void) {
	IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault,
	                                             kIOHIDOptionsTypeNone);
	if (manager == NULL) {
		return NULL;
	}
	IOHIDManagerSetDeviceMatching(manager, NULL);
	if (IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
		CFRelease(manager);
		return NULL;
	}
	return manager;
}

static long oskNumberProperty(IOHIDDeviceRef device, CFStringRef key) {
	CFTypeRef value = IOHIDDeviceGetProperty(device, key);
	long out = 0;
	if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID()) {
		CFNumberGetValue((CFNumberRef)value, kCFNumberLongType, &out);
	}
	return out;
}

static void oskTextProperty(IOHIDDeviceRef device, CFStringRef key,
                            char *out, long cap) {
	out[0] = 0;
	CFTypeRef value = IOHIDDeviceGetProperty(device, key);
	if (value != NULL && CFGetTypeID(value) == CFStringGetTypeID()) {
		CFStringGetCString((CFStringRef)value, out, cap, kCFStringEncodingUTF8);
	}
}

// oskDeviceAt gibt das Geraet mit laufender Nummer index zurueck.
static IOHIDDeviceRef oskDeviceAt(IOHIDManagerRef manager, long index, long *count) {
	CFSetRef set = IOHIDManagerCopyDevices(manager);
	if (set == NULL) {
		*count = 0;
		return NULL;
	}
	CFIndex size = CFSetGetCount(set);
	*count = (long)size;
	if (index < 0 || index >= size) {
		CFRelease(set);
		return NULL;
	}
	const void **list = (const void **)malloc(sizeof(void *) * (size_t)size);
	if (list == NULL) {
		CFRelease(set);
		return NULL;
	}
	CFSetGetValues(set, list);
	IOHIDDeviceRef device = (IOHIDDeviceRef)list[index];
	CFRetain(device);
	free(list);
	CFRelease(set);
	return device;
}

static int oskOpenDevice(IOHIDDeviceRef device) {
	return IOHIDDeviceOpen(device, kIOHIDOptionsTypeSeizeDevice) == kIOReturnSuccess;
}

// oskListen haengt das Geraet an eine eigene Ereignisschleife und laeuft,
// bis die Schleife gestoppt wird.
static void oskListen(IOHIDDeviceRef device, long handle, uint8_t *buffer,
                      long bufferLen, CFRunLoopRef *loopOut) {
	CFRunLoopRef loop = CFRunLoopGetCurrent();
	*loopOut = loop;
	IOHIDDeviceRegisterInputReportCallback(device, buffer, (CFIndex)bufferLen,
	                                       oskReportCallback, (void *)handle);
	IOHIDDeviceScheduleWithRunLoop(device, loop, kCFRunLoopDefaultMode);
	CFRunLoopRun();
	IOHIDDeviceUnscheduleFromRunLoop(device, loop, kCFRunLoopDefaultMode);
}

static void oskStop(CFRunLoopRef loop) {
	if (loop != NULL) {
		CFRunLoopStop(loop);
	}
}

static int oskSetReport(IOHIDDeviceRef device, uint8_t *data, long length) {
	return IOHIDDeviceSetReport(device, kIOHIDReportTypeOutput, 0, data,
	                            (CFIndex)length) == kIOReturnSuccess;
}

static void oskClose(IOHIDDeviceRef device) {
	IOHIDDeviceClose(device, kIOHIDOptionsTypeNone);
	CFRelease(device);
}
*/
import "C"

import (
	"runtime"
	"strconv"
	"sync"
	"time"
	"unsafe"
)

// macListeners verbindet die C-Rueckrufe mit den offenen Verbindungen.
var macListeners = struct {
	mu   sync.Mutex
	next int64
	open map[int64]*macHid
}{open: map[int64]*macHid{}}

//export oskInputReport
func oskInputReport(handle C.long, data *C.uchar, length C.long) {
	if length <= 0 || length > ReportSize {
		return
	}
	macListeners.mu.Lock()
	dev := macListeners.open[int64(handle)]
	macListeners.mu.Unlock()
	if dev == nil {
		return
	}
	report := make([]byte, ReportSize)
	copy(report, C.GoBytes(unsafe.Pointer(data), C.int(length)))
	select {
	case dev.incoming <- report:
	default: // Warteschlange voll: der alte Bericht ist wertlos
	}
}

// HidEnumerate zaehlt die HID-Geraete des Rechners auf. Der Pfad ist die
// laufende Nummer im IOKit-Verzeichnis.
func HidEnumerate() ([]HidInfo, error) {
	manager := C.oskManager()
	if manager == 0 {
		return nil, Fail("error.hid.enumerate", "IOHIDManager")
	}
	defer C.CFRelease(C.CFTypeRef(manager))

	var count C.long
	if dev := C.oskDeviceAt(manager, 0, &count); dev != 0 {
		C.CFRelease(C.CFTypeRef(dev))
	}
	var out []HidInfo
	for i := C.long(0); i < count; i++ {
		var ignored C.long
		device := C.oskDeviceAt(manager, i, &ignored)
		if device == 0 {
			continue
		}
		out = append(out, describeMac(device, int64(i)))
		C.CFRelease(C.CFTypeRef(device))
	}
	return out, nil
}

func describeMac(device C.IOHIDDeviceRef, index int64) HidInfo {
	return HidInfo{
		Path:      strconv.FormatInt(index, 10),
		VendorID:  uint16(C.oskNumberProperty(device, C.CFSTR("VendorID"))),
		ProductID: uint16(C.oskNumberProperty(device, C.CFSTR("ProductID"))),
		UsagePage: uint16(C.oskNumberProperty(device, C.CFSTR("PrimaryUsagePage"))),
		Usage:     uint16(C.oskNumberProperty(device, C.CFSTR("PrimaryUsage"))),
		Product:   macText(device, C.CFSTR("Product")),
		Serial:    macText(device, C.CFSTR("SerialNumber")),
	}
}

func macText(device C.IOHIDDeviceRef, key C.CFStringRef) string {
	var buf [256]C.char
	C.oskTextProperty(device, key, &buf[0], C.long(len(buf)))
	return C.GoString(&buf[0])
}

// macHid ist eine offene Verbindung samt eigener Ereignisschleife.
type macHid struct {
	device   C.IOHIDDeviceRef
	buffer   *C.uchar
	loop     C.CFRunLoopRef
	handle   int64
	label    string
	incoming chan []byte
	mu       sync.Mutex
	closed   bool
}

// HidOpen oeffnet das Geraet mit der angegebenen laufenden Nummer.
func HidOpen(path string) (Transport, error) {
	index, err := strconv.ParseInt(path, 10, 32)
	if err != nil || index < 0 {
		return nil, Fail("error.hid.open", path)
	}
	manager := C.oskManager()
	if manager == 0 {
		return nil, Fail("error.hid.open", path)
	}
	defer C.CFRelease(C.CFTypeRef(manager))

	var count C.long
	device := C.oskDeviceAt(manager, C.long(index), &count)
	if device == 0 {
		return nil, Fail("error.hid.open", path)
	}
	if C.oskOpenDevice(device) == 0 {
		C.CFRelease(C.CFTypeRef(device))
		return nil, Fail("error.hid.open", path)
	}
	info := describeMac(device, index)
	dev := &macHid{
		device:   device,
		buffer:   (*C.uchar)(C.calloc(ReportSize, 1)),
		label:    info.Label(),
		incoming: make(chan []byte, 16),
	}
	macListeners.mu.Lock()
	macListeners.next++
	dev.handle = macListeners.next
	macListeners.open[dev.handle] = dev
	macListeners.mu.Unlock()

	ready := make(chan struct{})
	go func() {
		// Die Ereignisschleife gehoert diesem Betriebssystem-Faden.
		runtime.LockOSThread()
		defer runtime.UnlockOSThread()
		var loop C.CFRunLoopRef
		close(ready)
		C.oskListen(dev.device, C.long(dev.handle), dev.buffer, ReportSize, &loop)
		dev.mu.Lock()
		dev.loop = 0
		dev.mu.Unlock()
		_ = loop
	}()
	<-ready
	// Die Schleife braucht einen Augenblick, bis sie laeuft.
	time.Sleep(50 * time.Millisecond)
	return dev, nil
}

func (m *macHid) Describe() string { return m.label }

func (m *macHid) WriteReport(report []byte) error {
	if len(report) != ReportSize {
		return Fail("error.frame.size")
	}
	m.mu.Lock()
	defer m.mu.Unlock()
	if m.closed {
		return Fail("error.hid.write", "closed")
	}
	buf := C.CBytes(report)
	defer C.free(buf)
	if C.oskSetReport(m.device, (*C.uchar)(buf), C.long(len(report))) == 0 {
		return Fail("error.hid.write", "IOHIDDeviceSetReport")
	}
	return nil
}

func (m *macHid) ReadReport(timeout time.Duration) ([]byte, error) {
	select {
	case report := <-m.incoming:
		return report, nil
	case <-time.After(timeout):
		return nil, Fail("error.hid.timeout")
	}
}

func (m *macHid) Close() error {
	m.mu.Lock()
	if m.closed {
		m.mu.Unlock()
		return nil
	}
	m.closed = true
	loop := m.loop
	m.mu.Unlock()

	C.oskStop(loop)
	macListeners.mu.Lock()
	delete(macListeners.open, m.handle)
	macListeners.mu.Unlock()
	C.oskClose(m.device)
	C.free(unsafe.Pointer(m.buffer))
	return nil
}
