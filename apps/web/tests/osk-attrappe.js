// Eine Attrappe des Programms osk - fuer den Pruefsatz der Erweiterung.
//
// SIE PRUEFT DIE ANDERE SEITE DES VERTRAGS, NICHT DIE EIGENE.
//
// Das ist der Punkt. Eine Attrappe, die bloss hoeflich antwortet, bestaetigt
// nur, was die Erweiterung ohnehin erwartet - und die Erwartung ist genau
// das, was geprueft werden soll. Diese hier ist darum STRENG: sie nimmt
// eine Nachricht nur an, wenn sie dem WORTLAUT der vorhandenen Schnittstelle
// entspricht, und sie sagt bei jeder Abweichung, was falsch war.
//
// Der Wortlaut steht in apps/desktop/classes/native_messaging.go und
// apps/desktop/classes/native_origin.go. Gelesen am 30.10.2026; dieser
// Strang aendert dort nichts.
//
//	Rahmen    vier Bytes Laenge in der Reihenfolge des Rechners, dann JSON
//	Frage     {id, was, origin?, item?, feld?, pin?}
//	was       hello | state | unlock | lock | list | reveal
//	Antwort   {id, ok, error?, unlocked?, origin?, matches?, value?,
//	           claim?, device?}
//	matches   [{id, name, site, type}] - nie ein Geheimnis
//
// WAS DIESE ATTRAPPE ANNIMMT UND NICHT BEWEISEN KANN - die offenen Punkte,
// mit denen jede Pruefung gegen sie zu lesen ist:
//
//  1. Die Laenge steht in der Reihenfolge des Rechners (binary.NativeEndian).
//     Auf dem Prueflaufwerk ist das das kleine Ende; auf einem grossen Ende
//     waere es die andere, und das ist hier nicht nachgemessen.
//  2. Der Browser, nicht diese Attrappe, startet osk und uebergibt die
//     Kennung der Erweiterung als Aufrufparameter. Was der Browser dort
//     genau hineinschreibt, steht hier nur als Annahme.
//  3. Welche Fehlertexte osk schickt, haengt an seinen Sprachdateien. Hier
//     stehen die SCHLUESSEL (error.native.origin und so fort), nicht die
//     Saetze. Die Erweiterung zeigt den Text von osk unveraendert an - dass
//     er in der Sprache des Anwenders ankommt, ist hier nicht geprueft.
//  4. Ein echtes osk braucht fuer reveal eine Freigabe am Geraet und kann
//     dabei beliebig lange brauchen oder abbrechen. Diese Attrappe
//     antwortet sofort. Die Frist der Erweiterung (120 Sekunden) ist damit
//     nicht gemessen.
//  5. hello liefert hier ein erfundenes Geraet. Ob ein echtes Geraet
//     Seriennummer und Bezeichnung so fuellt, ist Sache des Geraetestrangs.
//  6. Der Tresor dieser Attrappe liegt im Arbeitsspeicher. Die 768 Eintraege
//     je Profil, die Reihenfolge der Liste und das Verhalten bei einem
//     vollen Tresor sind hier nicht nachgebildet.
'use strict';

// --- Rahmen -----------------------------------------------------------------

// encode schreibt eine Nachricht: vier Bytes Laenge, dann JSON.
function encode(value) {
  const body = Buffer.from(JSON.stringify(value), 'utf8');
  const header = Buffer.alloc(4);
  // Die Reihenfolge des Rechners - so steht es in readNativeMessage.
  header.writeUInt32LE(body.length, 0);
  return Buffer.concat([header, body]);
}

// decode liest eine Nachricht aus einem Puffer. Zurueck kommt der Wert und
// der Rest des Puffers.
function decode(buffer) {
  if (buffer.length < 4) {
    throw new Error('zu kurz fuer die Laenge');
  }
  const size = buffer.readUInt32LE(0);
  // osk weist 0 und mehr als ein Megabyte ab (error.native.size).
  if (size === 0 || size > 1024 * 1024) {
    throw new Error('error.native.size');
  }
  if (buffer.length < 4 + size) {
    throw new Error('Nachricht unvollstaendig');
  }
  const body = buffer.slice(4, 4 + size);
  return { value: JSON.parse(body.toString('utf8')), rest: buffer.slice(4 + size) };
}

// --- Der Tresor der Attrappe -------------------------------------------------

// Eintraege, wie sie ein Anwender anlegt: Nummer, Name, Adresse, Art.
const EINTRAEGE = [
  { id: 1, name: 'anna', site: 'example.org', type: 1, secret: 'gehe1m' },
  { id: 2, name: 'anna', site: 'https://login.example.org/', type: 1,
    secret: 'and3rs' },
  { id: 3, name: 'anna', site: 'bank.example', type: 1, secret: 'dr1ttes' },
];

// --- Die Regeln von osk, nachgebaut nach native_origin.go --------------------

// checkOrigin prueft die Adresse, die die Erweiterung schickt. Streng: alles,
// was CheckOrigin abweist, wird auch hier abgewiesen.
function checkOrigin(raw) {
  if (typeof raw !== 'string' || raw.length > 300) {
    return null;
  }
  for (const ch of raw) {
    const code = ch.codePointAt(0);
    if (ch === ' ' || ch === '\t') {
      continue;
    }
    if (code < 0x20 || code === 0x7f) {
      return null;
    }
  }
  const text = raw.trim();
  if (!text) {
    return null;
  }
  let parsed;
  try {
    parsed = new URL(text);
  } catch (error) {
    return null;
  }
  if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') {
    return null;
  }
  if (parsed.username || parsed.password) {
    return null;
  }
  // Die Erweiterung schickt eine Adresse OHNE Pfad. Kommt doch einer, ist
  // das nicht das, was vereinbart wurde - und die Attrappe laesst es nicht
  // durchgehen, auch wenn es harmlos aussieht.
  if (parsed.pathname !== '' && parsed.pathname !== '/') {
    return null;
  }
  if (parsed.search || parsed.hash) {
    return null;
  }
  const host = parsed.hostname.toLowerCase().replace(/\.$/, '');
  if (!host || host.length > 253) {
    return null;
  }
  // Ein Rechnername aus Buchstaben, Zahlen, Strich, Punkt - bei einer
  // Zahlenadresse auch Doppelpunkt. Alles andere ist ein Umgehungsversuch.
  if (!/^[a-z0-9.:[\]-]+$/.test(host)) {
    return null;
  }
  if (host.startsWith('.') || host.includes('..')) {
    return null;
  }
  const local = host === 'localhost' || host.endsWith('.localhost') ||
    host === '[::1]' || /^127\.\d+\.\d+\.\d+$/.test(host);
  if (parsed.protocol === 'http:' && !local) {
    return { error: 'error.native.insecure' };
  }
  return { scheme: parsed.protocol.slice(0, -1), host };
}

// siteHost holt den Rechnernamen aus dem Feld eines Eintrags - SiteHost.
function siteHost(site) {
  const text = String(site || '').trim();
  if (!text || text.includes('*')) {
    return null;
  }
  let parsed;
  try {
    parsed = new URL(text.includes('//') ? text : 'https://' + text);
  } catch (error) {
    return null;
  }
  if (parsed.username || parsed.password) {
    return null;
  }
  return parsed.hostname.toLowerCase().replace(/\.$/, '') || null;
}

// --- Die Attrappe ------------------------------------------------------------

// Host ist ein Gespraech. Er merkt sich, ob entsperrt ist, und was die
// Erweiterung gefragt hat - der Pruefsatz sieht beides nach.
class Host {
  constructor(options) {
    const settings = options || {};
    this.pin = settings.pin || '1234';
    this.unlocked = false;
    // Jede angenommene Frage, in der Reihenfolge des Eingangs.
    this.seen = [];
    // Jede abgewiesene Frage, mit dem Grund. Eine Erweiterung, die sich an
    // den Vertrag haelt, laesst diese Liste leer.
    this.refused = [];
  }

  // handle beantwortet eine Frage.
  handle(request) {
    const reply = { id: (request && request.id) || 0, ok: false };
    // DER VERTRAG, ZEILE FUER ZEILE. Was hier nicht stimmt, wird abgewiesen
    // und vermerkt - nicht stillschweigend geradegebogen.
    if (!request || typeof request !== 'object') {
      return this.refuse(reply, 'keine Nachricht');
    }
    if (!Number.isInteger(request.id) || request.id <= 0) {
      // osk schickt die Nummer zurueck, damit die Erweiterung zuordnen
      // kann. Ohne Nummer gibt es keine Zuordnung.
      return this.refuse(reply, 'id fehlt oder ist keine Zahl');
    }
    if (typeof request.was !== 'string') {
      return this.refuse(reply, 'was fehlt');
    }
    for (const key of Object.keys(request)) {
      if (!['id', 'was', 'origin', 'item', 'feld', 'pin'].includes(key)) {
        return this.refuse(reply, 'unbekanntes Feld: ' + key);
      }
    }
    this.seen.push(request);
    switch (request.was) {
      case 'hello':
        return this.hello(reply);
      case 'state':
        reply.ok = true;
        reply.unlocked = this.unlocked;
        return reply;
      case 'unlock':
        if (typeof request.pin !== 'string') {
          return this.refuse(reply, 'unlock ohne pin');
        }
        if (request.pin !== this.pin) {
          reply.error = 'error.device.pin';
          return reply;
        }
        this.unlocked = true;
        reply.ok = true;
        reply.unlocked = true;
        return reply;
      case 'lock':
        this.unlocked = false;
        reply.ok = true;
        reply.unlocked = false;
        return reply;
      case 'list':
        return this.list(reply, request);
      case 'reveal':
        return this.reveal(reply, request);
      default:
        reply.error = 'error.native.unknown';
        return reply;
    }
  }

  refuse(reply, why) {
    this.refused.push(why);
    reply.error = 'vertragsbruch: ' + why;
    return reply;
  }

  hello(reply) {
    reply.ok = true;
    reply.unlocked = this.unlocked;
    reply.claim = 'claim.appname';
    reply.device = { label: 'Attrappe', serial: 'ATT-0001', simulated: true };
    return reply;
  }

  list(reply, request) {
    const where = checkOrigin(request.origin);
    if (!where) {
      reply.error = 'error.native.origin';
      return reply;
    }
    if (where.error) {
      reply.error = where.error;
      return reply;
    }
    reply.ok = true;
    reply.origin = where.scheme + '://' + where.host;
    // Passt keiner, ist die Antwort eine LEERE Liste - und ausdruecklich
    // keine Liste der uebrigen Eintraege. Die Erweiterung erfaehrt nie, was
    // sonst im Tresor liegt.
    reply.matches = EINTRAEGE
      .filter((item) => siteHost(item.site) === where.host)
      .map((item) => ({
        id: item.id, name: item.name, site: item.site, type: item.type,
      }));
    return reply;
  }

  reveal(reply, request) {
    const where = checkOrigin(request.origin);
    if (!where || where.error) {
      reply.error = (where && where.error) || 'error.native.origin';
      return reply;
    }
    if (request.feld !== 'user' && request.feld !== 'secret') {
      reply.error = 'error.native.field';
      return reply;
    }
    const found = EINTRAEGE.find((item) => item.id === request.item);
    if (!found) {
      reply.error = 'error.device.notfound';
      return reply;
    }
    // DER EIGENTLICHE RIEGEL, und er sitzt hier noch einmal: auch wer die
    // Nummer kennt, bekommt nichts, wenn die Adresse nicht passt.
    if (siteHost(found.site) !== where.host) {
      reply.error = 'error.native.origin';
      return reply;
    }
    reply.ok = true;
    reply.origin = where.scheme + '://' + where.host;
    // Es gibt kein eigenes Feld fuer den Benutzernamen; osk gibt den Namen
    // des Eintrags heraus. Das ist nachgebildet, damit niemand im Pruefsatz
    // etwas anderes erwartet als das, was wirklich kommt.
    reply.value = request.feld === 'user' ? found.name : found.secret;
    return reply;
  }
}

module.exports = { Host, encode, decode, checkOrigin, siteHost, EINTRAEGE };
