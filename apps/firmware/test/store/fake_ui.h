/* Anzeige- und Freigabe-Attrappe. Sie merkt sich, WAS gefragt wurde - damit
 * der Test beweisen kann, dass vor jedem Geheimnis wirklich gefragt wird. */
#ifndef FAKE_UI_H
#define FAKE_UI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "osk_ui.h"

void fake_ui_reset(void);

/* true = der Nutzer drueckt, false = er will nicht (OSK_ERR_DENIED). */
void fake_ui_answer(bool yes);

size_t fake_ui_asks(void);
osk_ask_t fake_ui_last_kind(void);
const char *fake_ui_last_subject(void);
const char *fake_ui_last_detail(void);

#endif
