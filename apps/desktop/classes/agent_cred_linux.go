//go:build linux

package classes

import "syscall"

// getPeerCred fragt den Kern nach der Gegenseite eines Sockets.
func getPeerCred(fd uintptr) (*syscall.Ucred, error) {
	return syscall.GetsockoptUcred(int(fd), syscall.SOL_SOCKET, syscall.SO_PEERCRED)
}
