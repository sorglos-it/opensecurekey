/* SE050-Attrappe fuer den Test am PC. KEINE Sicherheit.
 *
 * Sie stellt genau das nach, worauf sich der Tresor stuetzt:
 *  - eine geraetegebundene Ableitung, die je Geraet anders ausfaellt
 *  - eine PIN-Pruefung mit Fehlversuchszaehler
 *  - einen Schluesselaustausch, bei dem beide Seiten denselben Wert erhalten
 *  - monotone Zaehler, die NUR vorwaerts gehen
 *
 * Der Austausch ist ein Spielzeug: die Attrappe rechnet das gemeinsame
 * Geheimnis aus den beiden oeffentlichen Teilen. Damit laesst sich der ABLAUF
 * von Stick zu Stick pruefen - die Geheimhaltung des echten ECDH natuerlich
 * nicht. Die kommt vom Chip. */
#ifndef FAKE_SE050_H
#define FAKE_SE050_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Welches Geraet die Attrappe gerade ist. Jedes hat sein eigenes
 * Grundgeheimnis - ein Tresor von Geraet A ist auf Geraet B unlesbar. */
void fake_se050_device(uint32_t id);

/* Die PIN, die als richtig gilt. */
void fake_se050_set_pin(const char *pin);
void fake_se050_reset(void);

uint8_t fake_se050_tries_left(void);

/* Stand eines Zaehlers, ohne ihn anzufassen. (uint64_t)-1 heisst: gibt es
 * nicht. Nur fuer die Pruefsaetze - das Geraet kennt diesen Weg nicht. */
uint64_t fake_se050_counter_peek(uint32_t id);

/* Einen frisch angelegten Zaehler NICHT bei null anfangen lassen. Der
 * Anfangswert steht in AN12413 nicht; der Tresor darf sich also nicht auf
 * null verlassen, und genau das laesst sich damit nachstellen. */
void fake_se050_counter_start_at(uint64_t value);

/* Wie oft der Zaehler wirklich angehoben wurde. Jedes Anheben ist ein
 * Schreibvorgang im NVM des Chips - eine Zahl, die man kennen will, bevor
 * man sie je Anmeldung bezahlt. */
uint32_t fake_se050_raises(void);
void fake_se050_raises_reset(void);

/* Signatur gegen einen oeffentlichen Teil pruefen - fuer den Datentraeger,
 * der von einem ANDEREN Stick signiert sein kann. Naeheres in fake_se050.c. */
bool fake_se050_sig_matches(const uint8_t *pub, const uint8_t *msg, size_t msg_len,
			    const uint8_t *sig);

#endif
