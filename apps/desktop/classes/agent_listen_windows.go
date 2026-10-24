//go:build windows

// Der Agent unter Windows: eine benannte Pipe.
//
// Windows hat kein Dateirecht, das auf einen Socket passt - der Weg ist die
// benannte Pipe, und genau den geht auch der Agent, den Windows selbst
// mitbringt. Drei Dinge entscheiden hier ueber die Sicherheit, und alle drei
// stehen im Code darunter:
//
//   - Die Pipe bekommt eine Rechteliste, die NUR den angemeldeten Benutzer
//     zulaesst. Ohne sie darf jeder angemeldete Benutzer des Rechners
//     verbinden - und in seinem Namen unterschreiben lassen.
//   - Die erste Instanz wird mit FILE_FLAG_FIRST_PIPE_INSTANCE angelegt.
//     Haette ein anderes Programm den Namen schon belegt, wuerde unser
//     Aufruf sonst gelingen und ssh landete bei dem fremden Programm.
//   - PIPE_REJECT_REMOTE_CLIENTS weist Verbindungen aus dem Netz ab. Eine
//     Pipe ist ueber das Netz erreichbar; der Agent darf das nie sein.
//
// Ohne cgo: nur syscall und die DLLs, die Windows selbst mitbringt - wie in
// hid_windows.go.
package classes

import (
	"net"
	"os"
	"sync"
	"syscall"
	"time"
	"unsafe"
)

var (
	dllAdvapi = syscall.NewLazyDLL("advapi32.dll")

	procCreateNamedPipe     = dllKernel.NewProc("CreateNamedPipeW")
	procConnectNamedPipe    = dllKernel.NewProc("ConnectNamedPipe")
	procDisconnectNamedPipe = dllKernel.NewProc("DisconnectNamedPipe")
	procClientProcessID     = dllKernel.NewProc("GetNamedPipeClientProcessId")
	procProcessImageName    = dllKernel.NewProc("QueryFullProcessImageNameW")
	procStringToSD          = dllAdvapi.NewProc("ConvertStringSecurityDescriptorToSecurityDescriptorW")
	procLocalFree           = dllKernel.NewProc("LocalFree")
	procSetEvent            = dllKernel.NewProc("SetEvent")
	procWaitMultiple        = dllKernel.NewProc("WaitForMultipleObjects")
)

const (
	pipeAccessDuplex      = 0x00000003
	pipeFirstInstance     = 0x00080000
	pipeRejectRemote      = 0x00000008
	pipeUnlimitedInstance = 255
	pipeBufferSize        = 64 * 1024
	// Fehlernummern von Windows. Sie stehen hier als Zahl, weil nicht jede
	// davon in der Standardbibliothek einen Namen hat - und eine Nummer,
	// die nur halb benannt ist, ist schlechter als eine mit Kommentar.
	errPipeConnected    = 535 // ERROR_PIPE_CONNECTED
	errPipeBusy         = 231 // ERROR_PIPE_BUSY
	errBrokenPipe       = 109 // ERROR_BROKEN_PIPE
	errNoData           = 232 // ERROR_NO_DATA
	errPipeNotConnected = 233 // ERROR_PIPE_NOT_CONNECTED
	errOperationAborted = 995 // ERROR_OPERATION_ABORTED
	errAlreadyExists    = 183 // ERROR_ALREADY_EXISTS
	waitInfinite        = 0xFFFFFFFF
	sddlRevision1       = 1
	// queryLimitedInformation genuegt, um den Programmpfad eines fremden
	// Prozesses zu lesen - mehr Rechte braucht es dafuer nicht.
	processQueryLimitedInformation = 0x1000
)

// DefaultAgentAddress nennt den Platz, an dem der Agent lauscht.
//
// Der Name traegt die Benutzernummer nicht: eine Pipe ist ueber ihre
// Rechteliste geschuetzt, nicht ueber ihren Namen. Zwei gleichzeitig
// angemeldete Benutzer kommen sich trotzdem nicht in die Quere - der zweite
// bekommt den Namen nicht mehr und sagt das auch.
func DefaultAgentAddress() string {
	if custom := os.Getenv(AgentEnvName); custom != "" {
		return custom
	}
	return `\\.\pipe\osk-ssh-agent`
}

// winPipeAgent ist der Lauscher auf einer benannten Pipe.
type winPipeAgent struct {
	name     string
	wide     *uint16
	security *syscall.SecurityAttributes
	sdMemory uintptr

	mu      sync.Mutex
	closed  bool
	first   bool
	pending syscall.Handle
	pendOv  *syscall.Overlapped
	stop    syscall.Handle
}

// ListenAgent legt die Pipe an.
func ListenAgent(address string) (AgentListener, error) {
	if address == "" {
		address = DefaultAgentAddress()
	}
	wide, err := syscall.UTF16PtrFromString(address)
	if err != nil {
		return nil, Fail("error.agent.listen", address)
	}
	security, memory, err := ownerOnlySecurity()
	if err != nil {
		return nil, err
	}
	stop, err := createEvent()
	if err != nil {
		procLocalFree.Call(memory)
		return nil, Fail("error.agent.listen", address)
	}
	return &winPipeAgent{
		name:     address,
		wide:     wide,
		security: security,
		sdMemory: memory,
		first:    true,
		pending:  syscall.InvalidHandle,
		stop:     stop,
	}, nil
}

// ownerOnlySecurity baut eine Rechteliste, die nur den angemeldeten Benutzer
// zulaesst. Der Text ist SDDL: D: eine Rechteliste, P sperrt Erbschaft von
// oben, (A;;GA;;;<SID>) erlaubt diesem einen Benutzer alles. Dass nichts
// geerbt wird, ist dabei der Punkt - sonst stehen die Rechte des Ordners
// darueber mit in der Liste.
func ownerOnlySecurity() (*syscall.SecurityAttributes, uintptr, error) {
	sid, err := currentUserSid()
	if err != nil {
		return nil, 0, err
	}
	text, err := syscall.UTF16PtrFromString("D:P(A;;GA;;;" + sid + ")")
	if err != nil {
		return nil, 0, Fail("error.agent.rights")
	}
	var descriptor uintptr
	ok, _, callErr := procStringToSD.Call(uintptr(unsafe.Pointer(text)),
		sddlRevision1, uintptr(unsafe.Pointer(&descriptor)), 0)
	if ok == 0 || descriptor == 0 {
		return nil, 0, Fail("error.agent.rights", errText(callErr))
	}
	attributes := &syscall.SecurityAttributes{
		Length:             uint32(unsafe.Sizeof(syscall.SecurityAttributes{})),
		SecurityDescriptor: descriptor,
	}
	return attributes, descriptor, nil
}

// currentUserSid nennt die Kennung des angemeldeten Benutzers.
func currentUserSid() (string, error) {
	token, err := syscall.OpenCurrentProcessToken()
	if err != nil {
		return "", Fail("error.agent.rights")
	}
	defer token.Close()
	user, err := token.GetTokenUser()
	if err != nil {
		return "", Fail("error.agent.rights")
	}
	text, err := user.User.Sid.String()
	if err != nil {
		return "", Fail("error.agent.rights")
	}
	return text, nil
}

func (w *winPipeAgent) Address() string { return w.name }

// Accept legt eine neue Instanz der Pipe an und wartet auf einen Klienten.
func (w *winPipeAgent) Accept() (net.Conn, string, error) {
	handle, err := w.newInstance()
	if err != nil {
		return nil, "", err
	}
	connected, err := w.waitForClient(handle)
	if err != nil {
		syscall.CloseHandle(handle)
		return nil, "", err
	}
	if !connected {
		syscall.CloseHandle(handle)
		return nil, "", Fail("error.agent.closed")
	}
	conn, err := newPipeConn(handle, w.name)
	if err != nil {
		procDisconnectNamedPipe.Call(uintptr(handle))
		syscall.CloseHandle(handle)
		return nil, "", err
	}
	return conn, peerOfPipe(handle), nil
}

// newInstance legt eine Instanz an. Die erste verlangt, dass der Name noch
// frei ist - belegt ihn schon jemand, ist das ein Fehler und kein Grund,
// trotzdem zu lauschen.
func (w *winPipeAgent) newInstance() (syscall.Handle, error) {
	w.mu.Lock()
	if w.closed {
		w.mu.Unlock()
		return syscall.InvalidHandle, Fail("error.agent.closed")
	}
	mode := uintptr(pipeAccessDuplex | syscall.FILE_FLAG_OVERLAPPED)
	if w.first {
		mode |= pipeFirstInstance
	}
	w.mu.Unlock()

	handle, _, callErr := procCreateNamedPipe.Call(
		uintptr(unsafe.Pointer(w.wide)),
		mode,
		uintptr(pipeRejectRemote),
		uintptr(pipeUnlimitedInstance),
		uintptr(pipeBufferSize),
		uintptr(pipeBufferSize),
		0,
		uintptr(unsafe.Pointer(w.security)))
	if handle == 0 || handle == ^uintptr(0) {
		switch errNumber(callErr) {
		case errPipeBusy, errAlreadyExists, uintptr(syscall.ERROR_ACCESS_DENIED):
			// Den Namen hat schon jemand. Das ist der Fall, gegen den
			// FILE_FLAG_FIRST_PIPE_INSTANCE schuetzt, und er muss laut
			// gesagt werden, statt still weiterzulauschen.
			return syscall.InvalidHandle, Fail("error.agent.inuse", w.name)
		}
		return syscall.InvalidHandle, Fail("error.agent.listen", w.name)
	}
	w.mu.Lock()
	w.first = false
	w.mu.Unlock()
	return syscall.Handle(handle), nil
}

// waitForClient wartet, bis ein Programm verbindet - oder bis Close das
// Warten beendet.
func (w *winPipeAgent) waitForClient(handle syscall.Handle) (bool, error) {
	event, err := createEvent()
	if err != nil {
		return false, Fail("error.agent.listen", w.name)
	}
	defer syscall.CloseHandle(event)
	overlapped := &syscall.Overlapped{HEvent: event}

	ok, _, callErr := procConnectNamedPipe.Call(uintptr(handle),
		uintptr(unsafe.Pointer(overlapped)))
	if ok == 0 {
		switch errNumber(callErr) {
		case errPipeConnected:
			// Der Klient war schneller als wir. Das ist kein Fehler.
			return true, nil
		case uintptr(syscall.ERROR_IO_PENDING):
		default:
			return false, Fail("error.agent.listen", w.name)
		}
	}

	w.mu.Lock()
	if w.closed {
		w.mu.Unlock()
		procCancelIoEx.Call(uintptr(handle), uintptr(unsafe.Pointer(overlapped)))
		return false, Fail("error.agent.closed")
	}
	w.pending, w.pendOv = handle, overlapped
	w.mu.Unlock()

	// Zwei Dinge koennen das Warten beenden: ein Klient oder Close.
	handles := [2]syscall.Handle{event, w.stop}
	state, _, _ := procWaitMultiple.Call(2, uintptr(unsafe.Pointer(&handles[0])), 0,
		uintptr(uint32(waitInfinite)))

	w.mu.Lock()
	w.pending, w.pendOv = syscall.InvalidHandle, nil
	closed := w.closed
	w.mu.Unlock()

	if state == 0xFFFFFFFF {
		return false, Fail("error.agent.listen", w.name)
	}
	if state != 0 || closed {
		procCancelIoEx.Call(uintptr(handle), uintptr(unsafe.Pointer(overlapped)))
		syscall.WaitForSingleObject(event, 1000)
		return false, nil
	}
	var done uint32
	res, _, resErr := procOverlapped.Call(uintptr(handle),
		uintptr(unsafe.Pointer(overlapped)), uintptr(unsafe.Pointer(&done)), 0)
	if res == 0 {
		return false, Fail("error.agent.listen", errText(resErr))
	}
	return true, nil
}

// Close beendet das Lauschen und holt ein haengendes Accept heraus.
func (w *winPipeAgent) Close() error {
	w.mu.Lock()
	if w.closed {
		w.mu.Unlock()
		return nil
	}
	w.closed = true
	pending, overlapped := w.pending, w.pendOv
	w.mu.Unlock()

	if pending != syscall.InvalidHandle && overlapped != nil {
		procCancelIoEx.Call(uintptr(pending), uintptr(unsafe.Pointer(overlapped)))
	}
	// Das Ereignis loest ein Accept, das noch gar nicht im Warten steht.
	setEvent(w.stop)
	if w.sdMemory != 0 {
		procLocalFree.Call(w.sdMemory)
		w.sdMemory = 0
	}
	return nil
}

// peerOfPipe nennt das Programm am anderen Ende der Pipe. Windows sagt die
// Prozessnummer, daraus wird der Programmpfad gelesen - vom System, nicht
// vom Programm. Ein NAME bleibt es trotzdem.
func peerOfPipe(handle syscall.Handle) string {
	var pid uint32
	ok, _, _ := procClientProcessID.Call(uintptr(handle), uintptr(unsafe.Pointer(&pid)))
	if ok == 0 || pid == 0 {
		return ""
	}
	process, err := syscall.OpenProcess(processQueryLimitedInformation, false, pid)
	if err != nil {
		return ""
	}
	defer syscall.CloseHandle(process)
	buf := make([]uint16, 520)
	size := uint32(len(buf))
	ok, _, _ = procProcessImageName.Call(uintptr(process), 0,
		uintptr(unsafe.Pointer(&buf[0])), uintptr(unsafe.Pointer(&size)))
	if ok == 0 {
		return ""
	}
	return peerName(utf16Text(buf[:size]))
}

// setEvent setzt ein Ereignis - das Gegenstueck zu resetEvent.
func setEvent(handle syscall.Handle) {
	procSetEvent.Call(uintptr(handle))
}

// pipeConn ist eine verbundene Pipe als net.Conn. Lesen und Schreiben laufen
// ueberlappend mit je eigenem Ereignis, damit ein Zeitlimit wirklich
// abbricht - dieselbe Bauart wie bei HID.
type pipeConn struct {
	handle syscall.Handle
	name   string

	readMu     sync.Mutex
	writeMu    sync.Mutex
	readEvent  syscall.Handle
	writeEvent syscall.Handle

	deadlineMu    sync.Mutex
	readDeadline  time.Time
	writeDeadline time.Time

	closeOnce sync.Once
}

func newPipeConn(handle syscall.Handle, name string) (*pipeConn, error) {
	readEvent, err := createEvent()
	if err != nil {
		return nil, Fail("error.agent.listen", name)
	}
	writeEvent, err := createEvent()
	if err != nil {
		syscall.CloseHandle(readEvent)
		return nil, Fail("error.agent.listen", name)
	}
	return &pipeConn{handle: handle, name: name,
		readEvent: readEvent, writeEvent: writeEvent}, nil
}

func (p *pipeConn) Read(buf []byte) (int, error) {
	if len(buf) == 0 {
		return 0, nil
	}
	p.readMu.Lock()
	defer p.readMu.Unlock()
	p.deadlineMu.Lock()
	deadline := p.readDeadline
	p.deadlineMu.Unlock()

	var overlapped syscall.Overlapped
	overlapped.HEvent = p.readEvent
	resetEvent(p.readEvent)
	var read uint32
	err := syscall.ReadFile(p.handle, buf, &read, &overlapped)
	if err == syscall.ERROR_IO_PENDING {
		if waitErr := p.wait(p.readEvent, &overlapped, deadline, &read); waitErr != nil {
			return 0, waitErr
		}
	} else if err != nil {
		return 0, pipeError(err)
	}
	if read == 0 {
		return 0, net.ErrClosed
	}
	return int(read), nil
}

func (p *pipeConn) Write(buf []byte) (int, error) {
	if len(buf) == 0 {
		return 0, nil
	}
	p.writeMu.Lock()
	defer p.writeMu.Unlock()
	p.deadlineMu.Lock()
	deadline := p.writeDeadline
	p.deadlineMu.Unlock()

	var overlapped syscall.Overlapped
	overlapped.HEvent = p.writeEvent
	resetEvent(p.writeEvent)
	var written uint32
	err := syscall.WriteFile(p.handle, buf, &written, &overlapped)
	if err == syscall.ERROR_IO_PENDING {
		if waitErr := p.wait(p.writeEvent, &overlapped, deadline, &written); waitErr != nil {
			return 0, waitErr
		}
	} else if err != nil {
		return 0, pipeError(err)
	}
	if int(written) != len(buf) {
		return int(written), net.ErrClosed
	}
	return int(written), nil
}

// wait wartet auf das Ende des Zugriffs und bricht ihn beim Zeitlimit ab.
// Ohne Abbruch liefe der Zugriff im Hintergrund weiter und schriebe in einen
// Puffer, den niemand mehr liest.
func (p *pipeConn) wait(event syscall.Handle, overlapped *syscall.Overlapped,
	deadline time.Time, done *uint32) error {
	timeout := uint32(waitInfinite)
	if !deadline.IsZero() {
		left := time.Until(deadline)
		if left <= 0 {
			procCancelIoEx.Call(uintptr(p.handle), uintptr(unsafe.Pointer(overlapped)))
			syscall.WaitForSingleObject(event, 1000)
			return os.ErrDeadlineExceeded
		}
		timeout = uint32(left / time.Millisecond)
		if timeout == 0 {
			timeout = 1
		}
	}
	state, waitErr := syscall.WaitForSingleObject(event, timeout)
	if state != 0 || waitErr != nil {
		procCancelIoEx.Call(uintptr(p.handle), uintptr(unsafe.Pointer(overlapped)))
		syscall.WaitForSingleObject(event, 1000)
		return os.ErrDeadlineExceeded
	}
	ok, _, resErr := procOverlapped.Call(uintptr(p.handle),
		uintptr(unsafe.Pointer(overlapped)), uintptr(unsafe.Pointer(done)), 0)
	if ok == 0 {
		return pipeError(resErr)
	}
	return nil
}

func (p *pipeConn) Close() error {
	var err error
	p.closeOnce.Do(func() {
		procCancelIoEx.Call(uintptr(p.handle), 0)
		procDisconnectNamedPipe.Call(uintptr(p.handle))
		syscall.CloseHandle(p.readEvent)
		syscall.CloseHandle(p.writeEvent)
		err = syscall.CloseHandle(p.handle)
	})
	return err
}

func (p *pipeConn) SetDeadline(when time.Time) error {
	p.deadlineMu.Lock()
	defer p.deadlineMu.Unlock()
	p.readDeadline, p.writeDeadline = when, when
	return nil
}

func (p *pipeConn) SetReadDeadline(when time.Time) error {
	p.deadlineMu.Lock()
	defer p.deadlineMu.Unlock()
	p.readDeadline = when
	return nil
}

func (p *pipeConn) SetWriteDeadline(when time.Time) error {
	p.deadlineMu.Lock()
	defer p.deadlineMu.Unlock()
	p.writeDeadline = when
	return nil
}

func (p *pipeConn) LocalAddr() net.Addr  { return pipeAddr(p.name) }
func (p *pipeConn) RemoteAddr() net.Addr { return pipeAddr(p.name) }

// pipeAddr ist die Adresse einer Pipe.
type pipeAddr string

func (a pipeAddr) Network() string { return "pipe" }
func (a pipeAddr) String() string  { return string(a) }

// pipeError macht aus einem Systemfehler etwas, das die Leseschleife als
// Ende erkennt.
func pipeError(err error) error {
	if number, ok := err.(syscall.Errno); ok {
		switch uintptr(number) {
		case errBrokenPipe, errNoData, errPipeNotConnected, errOperationAborted:
			return net.ErrClosed
		}
	}
	return err
}
