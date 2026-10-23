/* Was der Treiber von der Platine braucht, aber nicht selbst weiss.
 *
 * osk_se050_open() aus osk_se050.h nimmt keine Parameter - der Vertrag
 * sagt nur "Chip anmelden". Woher der I2C-Bus und die SCP03-Schluessel
 * kommen, haengt am Geraet, und es sollte nicht in den Vertrag wandern,
 * damit alle anderen Straenge damit nichts zu tun haben.
 *
 * Darum diese zwei Haken. Auf dem Geraet erfuellt sie die Platinen- bzw.
 * Tresorschicht, im Pruefbau am PC der Test. Es gibt absichtlich keine
 * Vorgabe-Umsetzung: fehlt sie, bricht das Binden - und nicht erst der
 * Betrieb mit einem Schluessel aus Nullen. */
#ifndef SE050_PLATFORM_H
#define SE050_PLATFORM_H

#include "osk_result.h"
#include "se050_bus.h"
#include "se050_scp03.h"

/* Den I2C-Bus zum SE050 liefern. Der Zeiger muss die ganze Laufzeit
 * gueltig bleiben; der Treiber gibt ihn nicht frei. */
osk_result_t osk_se050_platform_bus(osk_se050_bus_t **bus_out);

/* Die festen SCP03-Schluessel dieses Geraetepaars liefern.
 *
 * Sie duerfen NICHT im Klartext im Flash des nRF52840 liegen - dessen
 * Auslesesperre gilt uns als gebrochen. Vorgesehener Weg: beim Einrichten
 * im SE050 erzeugt, dort gegen den Tresorschluessel verpackt abgelegt und
 * hier nur fuer die Dauer des Kanalaufbaus entpackt.
 *
 * Der Treiber loescht die Kopie sofort nach dem Aufbau wieder. */
osk_result_t osk_se050_platform_scp_keys(osk_scp03_static_keys_t *keys_out);

/* Kennung des AESKey-Anmeldeobjekts, gegen das der Geraetekanal aufgebaut
 * wird. Eine AESKey-Sitzung laeuft beim SE050 immer gegen ein benanntes
 * Objekt (AN12413 3.6.3.2) - es gibt keinen namenlosen Kanal. Das Objekt
 * entsteht beim Einrichten; die Kennung gehoert zur Platine, nicht zum
 * Treiber. */
osk_result_t osk_se050_platform_channel_obj(uint32_t *obj_id_out);

#endif
