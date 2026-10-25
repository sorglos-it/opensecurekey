/* Puffer sicher leeren - eine Hilfe, die mehrere Teile brauchen.
 *
 * Warum sie hier liegt und nicht bei einem der Teile: osk_wipe() war
 * zweimal geschrieben, in src/classes/ctap/osk_crypto.c und in
 * src/classes/se050/se050_crypto_common.c. Am PC fiel das nie auf, weil
 * dort jeder Teil einzeln gebunden wird; im Abbild des Geraets trafen sich
 * die beiden und das Binden brach ab (gemessen am 24.10.2026).
 *
 * Laege sie jetzt bei einem der beiden, haengte der andere daran - und
 * aufgerufen wird sie auch aus wallet. Darum gemeinsam.
 *
 * Umsetzung: src/osk_mem.c */
#ifndef OSK_MEM_H
#define OSK_MEM_H

#include <stddef.h>

/* Loescht einen Puffer so, dass der Uebersetzer es nicht wegoptimiert.
 * NULL und Laenge 0 sind erlaubt und tun nichts. */
void osk_wipe(void *p, size_t len);

#endif
