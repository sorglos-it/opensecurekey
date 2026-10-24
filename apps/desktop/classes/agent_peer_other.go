//go:build !windows && !linux

// Auf allen uebrigen Systemen mit Socket (macOS, BSD) nennt der Agent kein
// fragendes Programm. Der Weg dorthin ist dort ein anderer (LOCAL_PEERPID),
// und ungeprueft wuerde hier nur ein Name stehen, fuer den niemand
// einstehen kann. Lieber kein Name als ein erfundener - im Geraeteprotokoll
// steht dann der Agent selbst.
package classes

import "net"

func peerOfUnixConn(conn *net.UnixConn) string { return "" }
