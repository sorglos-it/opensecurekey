/* SE050-Attrappe fuer den Test am PC. KEINE Sicherheit.
 *
 * Sie stellt genau das nach, worauf sich der Tresor stuetzt:
 *  - eine geraetegebundene Ableitung, die je Geraet anders ausfaellt
 *  - eine PIN-Pruefung mit Fehlversuchszaehler
 *  - einen Schluesselaustausch, bei dem beide Seiten denselben Wert erhalten
 *
 * Der Austausch ist ein Spielzeug: die Attrappe rechnet das gemeinsame
 * Geheimnis aus den beiden oeffentlichen Teilen. Damit laesst sich der ABLAUF
 * von Stick zu Stick pruefen - die Geheimhaltung des echten ECDH natuerlich
 * nicht. Die kommt vom Chip. */
#ifndef FAKE_SE050_H
#define FAKE_SE050_H

#include <stdint.h>
#include <stddef.h>

/* Welches Geraet die Attrappe gerade ist. Jedes hat sein eigenes
 * Grundgeheimnis - ein Tresor von Geraet A ist auf Geraet B unlesbar. */
void fake_se050_device(uint32_t id);

/* Die PIN, die als richtig gilt. */
void fake_se050_set_pin(const char *pin);
void fake_se050_reset(void);

uint8_t fake_se050_tries_left(void);

#endif
