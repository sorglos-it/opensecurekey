/* Gemeinsame Rueckgabewerte aller Module.
 * Jede Funktion meldet Erfolg oder Grund des Fehlers - nie nur 0/1. */
#ifndef OSK_RESULT_H
#define OSK_RESULT_H

typedef enum {
	OSK_OK = 0,
	OSK_ERR_ARG,        /* Aufrufparameter unbrauchbar */
	OSK_ERR_IO,         /* Leitung, Bus oder Transport gestoert */
	OSK_ERR_TIMEOUT,
	OSK_ERR_DENIED,     /* Freigabe fehlt: PIN, Finger oder Taster */
	OSK_ERR_LOCKED,     /* Geraet gesperrt, zu viele Fehlversuche */
	OSK_ERR_NOT_FOUND,
	OSK_ERR_EXISTS,
	OSK_ERR_FULL,       /* Speicher im Chip oder Flash erschoepft */
	OSK_ERR_FORMAT,     /* Daten entsprechen nicht dem erwarteten Aufbau */
	OSK_ERR_UNSUPPORTED,
	OSK_ERR_INTERNAL,
} osk_result_t;

const char *osk_result_text(osk_result_t r);

#endif
