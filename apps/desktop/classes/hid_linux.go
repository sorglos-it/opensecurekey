//go:build linux

// HID unter Linux: /dev/hidraw*. Die Kennungen stehen in sysfs, gelesen wird
// mit poll, damit das Zeitlimit greift. Ohne cgo, nur syscall.
package classes

import (
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
	"unsafe"
)

const sysHidraw = "/sys/class/hidraw"

// HidEnumerate liest die Geraeteliste aus sysfs. Fehlt sysfs, gibt es keine
// Geraete - geraten wird nichts.
func HidEnumerate() ([]HidInfo, error) {
	entries, err := os.ReadDir(sysHidraw)
	if err != nil {
		if os.IsNotExist(err) {
			return nil, nil
		}
		return nil, Fail("error.hid.enumerate", err.Error())
	}
	var out []HidInfo
	for _, e := range entries {
		name := e.Name()
		if !strings.HasPrefix(name, "hidraw") {
			continue
		}
		info, infoErr := readUevent(filepath.Join(sysHidraw, name, "device", "uevent"))
		if infoErr != nil {
			continue
		}
		info.Path = "/dev/" + name
		out = append(out, info)
	}
	return out, nil
}

// readUevent liest HID_ID, HID_NAME und HID_UNIQ aus der uevent-Datei.
func readUevent(path string) (HidInfo, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return HidInfo{}, err
	}
	if len(data) > 64*1024 {
		return HidInfo{}, Fail("error.hid.enumerate", path)
	}
	var info HidInfo
	for _, line := range strings.Split(string(data), "\n") {
		key, value, found := strings.Cut(line, "=")
		if !found {
			continue
		}
		switch key {
		case "HID_ID":
			// Aufbau: Bus:xVVVV:xPPPP
			parts := strings.Split(value, ":")
			if len(parts) == 3 {
				info.VendorID = parseHex16(parts[1])
				info.ProductID = parseHex16(parts[2])
			}
		case "HID_NAME":
			info.Product = value
		case "HID_UNIQ":
			info.Serial = value
		}
	}
	return info, nil
}

func parseHex16(text string) uint16 {
	text = strings.TrimPrefix(strings.TrimSpace(text), "x")
	n, err := strconv.ParseUint(text, 16, 32)
	if err != nil {
		return 0
	}
	return uint16(n)
}

// linuxHid ist eine offene Verbindung zu /dev/hidrawN.
type linuxHid struct {
	file  *os.File
	label string
	mu    sync.Mutex
}

// HidOpen oeffnet ein hidraw-Geraet. Der Pfad muss unter /dev/ liegen und
// darf keine Pfadwechsel enthalten.
func HidOpen(path string) (Transport, error) {
	clean := filepath.Clean(path)
	if !strings.HasPrefix(clean, "/dev/hidraw") || strings.Contains(clean, "..") {
		return nil, Fail("error.hid.open", path)
	}
	file, err := os.OpenFile(clean, os.O_RDWR, 0)
	if err != nil {
		return nil, Fail("error.hid.open", err.Error())
	}
	label := clean
	if info, uErr := readUevent(filepath.Join(sysHidraw,
		filepath.Base(clean), "device", "uevent")); uErr == nil {
		info.Path = clean
		label = info.Label()
	}
	return &linuxHid{file: file, label: label}, nil
}

func (l *linuxHid) Describe() string { return l.label }

func (l *linuxHid) WriteReport(report []byte) error {
	if len(report) != ReportSize {
		return Fail("error.frame.size")
	}
	l.mu.Lock()
	defer l.mu.Unlock()

	// hidraw erwartet die Berichtsnummer als erstes Byte; wir benutzen 0.
	buf := make([]byte, ReportSize+1)
	copy(buf[1:], report)
	n, err := l.file.Write(buf)
	if err != nil {
		return Fail("error.hid.write", err.Error())
	}
	if n != len(buf) {
		return Fail("error.hid.write", "short")
	}
	return nil
}

func (l *linuxHid) ReadReport(timeout time.Duration) ([]byte, error) {
	l.mu.Lock()
	defer l.mu.Unlock()

	if err := waitReadable(int(l.file.Fd()), timeout); err != nil {
		return nil, err
	}
	buf := make([]byte, ReportSize+1)
	n, err := l.file.Read(buf)
	if err != nil {
		return nil, Fail("error.hid.read", err.Error())
	}
	switch {
	case n == ReportSize:
		out := make([]byte, ReportSize)
		copy(out, buf[:ReportSize])
		return out, nil
	case n == ReportSize+1 && buf[0] == 0:
		out := make([]byte, ReportSize)
		copy(out, buf[1:])
		return out, nil
	default:
		return nil, Fail("error.frame.size")
	}
}

func (l *linuxHid) Close() error {
	l.mu.Lock()
	defer l.mu.Unlock()
	return l.file.Close()
}

type pollFd struct {
	fd      int32
	events  int16
	revents int16
}

// waitReadable wartet mit poll auf Daten. Wird poll unterbrochen (EINTR),
// wird die Restzeit weiter abgewartet.
func waitReadable(fd int, timeout time.Duration) error {
	deadline := time.Now().Add(timeout)
	for {
		left := time.Until(deadline)
		if left <= 0 {
			return Fail("error.hid.timeout")
		}
		fds := []pollFd{{fd: int32(fd), events: 0x0001}} // POLLIN
		n, _, errno := syscall.Syscall(syscall.SYS_POLL,
			uintptr(unsafe.Pointer(&fds[0])), 1,
			uintptr(left/time.Millisecond))
		if errno == syscall.EINTR {
			continue
		}
		if errno != 0 {
			return Fail("error.hid.read", errno.Error())
		}
		if n == 0 {
			return Fail("error.hid.timeout")
		}
		if fds[0].revents&0x0001 != 0 {
			return nil
		}
		return Fail("error.hid.read", "poll")
	}
}
