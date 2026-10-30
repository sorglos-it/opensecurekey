// Die Entscheidungen der Erweiterung - und nur sie.
//
// Was hier steht, ist pruefbar: keine Verbindung, kein Fenster, kein Geraet.
// Darum liegt es in einer eigenen Datei, die sowohl der Browser laedt als
// auch der Pruefsatz (assets/rules.test.js, laeuft mit `node --test`). Eine
// Regel, die man nicht einzeln pruefen kann, prueft am Ende keiner.
//
// Drei Regeln stehen hier:
//
//   oskOriginOf     - welche Adresse hat die Seite, und darf hier ueberhaupt
//                     etwas ausgefuellt werden
//   oskSiteHost     - welcher Rechnername steht im Feld eines Eintrags
//   oskMatches      - gehoert dieser Eintrag zu dieser Seite
//   oskLooksForeign - traegt dieser Name Zeichen, die wie andere aussehen
//   oskPickLanguage - welche Sprachdatei wird genommen
//   oskFillForm     - welche Felder werden gefuellt (laeuft IN der Seite)
'use strict';

// oskOriginOf liest Verfahren und Rechnername aus einer Adresse.
//
// Zurueck kommt entweder {origin, host, shown} oder {error}. Ein Pfad, eine
// Abfrage und ein Anker werden weggelassen: fuer die Frage "gehoert dieses
// Geheimnis hierher" sind sie bedeutungslos, und sie koennen lang und
// irrefuehrend sein ("...?echt=bank.example.org").
//
// http gilt nur auf dem eigenen Rechner. Ueber eine unverschluesselte
// Verbindung geht kein Geheimnis - sonst liest es jeder im selben Netz mit.
function oskOriginOf(url) {
  let parsed;
  try {
    parsed = new URL(String(url));
  } catch (error) {
    return { error: 'notab' };
  }
  if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') {
    // about:, chrome:, view-source:, file: - dort gibt es nichts
    // auszufuellen, und eine Adresse im Sinne einer Seite auch nicht.
    return { error: 'notab' };
  }
  // Ein Benutzername in der Adresse ist der aelteste Trick der Zunft:
  // "https://bank.example.org@boese.tld/" sieht aus wie die Bank.
  // URL trennt ihn sauber ab, aber wir weisen ihn ausdruecklich ab - eine
  // Adresse mit Benutzernamen ist kein Ort, an dem wir ausfuellen.
  if (parsed.username || parsed.password) {
    return { error: 'notab' };
  }
  const host = String(parsed.hostname).toLowerCase().replace(/\.$/, '');
  if (!host) {
    return { error: 'notab' };
  }
  if (parsed.protocol === 'http:' && !oskIsLocal(host)) {
    return { error: 'insecure', shown: host };
  }
  return { origin: parsed.protocol + '//' + host, host, shown: host };
}

// oskIsLocal sagt, ob der Name den eigenen Rechner meint.
function oskIsLocal(host) {
  if (host === 'localhost' || host.endsWith('.localhost')) {
    return true;
  }
  if (host === '::1' || host === '[::1]') {
    return true;
  }
  // 127.0.0.0/8 ist ganz und gar der eigene Rechner.
  return /^127\.\d{1,3}\.\d{1,3}\.\d{1,3}$/.test(host);
}

// oskSiteHost holt den Rechnernamen aus dem Feld eines Eintrags.
//
// Das Feld schreibt ein Mensch. Er schreibt "example.org", oder
// "https://example.org/login", oder "example.org/login" - alles drei meint
// denselben Rechner, und alles drei wird hier auf denselben Namen gebracht.
//
// Ein Platzhalter wird abgewiesen und NICHT ausgewertet: "*.example.org" ist
// kein Rechnername. Wer ihn eintraegt, soll merken, dass der Eintrag
// nirgends ausfuellt - statt zu glauben, er habe eine Regel gesetzt.
//
// Diese Funktion ist das Gegenstueck zu SiteHost in osk
// (apps/desktop/classes/native_origin.go). Wer passt, entscheidet osk; das
// hier ist der zweite Riegel in der Erweiterung, kein Ersatz.
function oskSiteHost(site) {
  const text = String(site == null ? '' : site).trim();
  if (!text || text.length > 300) {
    return null;
  }
  // Steuerzeichen am ROHEN Text, vor jedem Putzen: ein Umbruch mitten in
  // der Adresse soll gesehen werden, und wer erst putzt, sieht ihn nie.
  for (const ch of text) {
    const code = ch.codePointAt(0);
    if (code < 0x20 || code === 0x7f) {
      return null;
    }
  }
  if (text.includes('*')) {
    return null;
  }
  let parsed;
  try {
    // Ohne Verfahren wird https davorgesetzt - erst dann ist es eine
    // Adresse, die URL lesen kann. Mit Verfahren bleibt es, wie es ist:
    // ein "ftp://..." im Feld soll auffallen und nicht stillschweigend
    // zu https werden.
    parsed = new URL(text.includes('//') ? text : 'https://' + text);
  } catch (error) {
    return null;
  }
  if (parsed.protocol !== 'https:' && parsed.protocol !== 'http:') {
    return null;
  }
  // Ein Benutzername im Feld des Eintrags ist genauso verdaechtig wie in
  // der Adresse der Seite.
  if (parsed.username || parsed.password) {
    return null;
  }
  const host = String(parsed.hostname).toLowerCase().replace(/\.$/, '');
  if (!host || host.length > 253) {
    return null;
  }
  return host;
}

// oskMatches sagt, ob ein Eintrag zu dieser Seite gehoert.
//
// DIE REGEL, UND SIE HAT KEINE AUSNAHME: der Rechnername des Eintrags und
// der Rechnername der Seite muessen ZEICHEN FUER ZEICHEN gleich sein.
//
// Was daraus folgt, und zwar mit Absicht:
//
//   - Kein Teilstring. "example.org" passt nicht auf "notexample.org", und
//     "bank.example.org" passt nicht auf "bank.example.org.boese.tld".
//   - Keine Unterdomaenen. Ein Eintrag fuer "example.org" fuellt NICHT auf
//     "login.example.org". Wer beides braucht, legt zwei Eintraege an.
//
// Der zweite Punkt kostet Bequemlichkeit, und er ist trotzdem richtig. Wer
// Unterdomaenen zulaesst, muss wissen, WO eine Domaene endet - und das weiss
// niemand aus dem Namen allein. "bank.de" endet nach zwei Teilen,
// "bank.co.uk" nach dreien, "bank.s3.amazonaws.com" nach keinem davon, und
// wer dort eine Grenze zieht, zieht sie bei jedem neuen Namensraum neu.
// Eine Regel, deren Grenze von einer gepflegten Liste abhaengt, ist genau
// dann falsch, wenn die Liste alt ist - und sie ist immer irgendwo alt.
//
// Verglichen wird die ASCII-Form (Punycode). Das ist kein Nebenbei: zwei
// Namen, die auf dem Bildschirm gleich aussehen, haben verschiedene
// ASCII-Formen, und genau daran scheitert der haeufigste Angriff auf diesen
// Vergleich. Siehe oskLooksForeign.
function oskMatches(site, host) {
  const left = oskSiteHost(site);
  if (!left) {
    return false;
  }
  return left === String(host == null ? '' : host).toLowerCase();
}

// oskLooksForeign sagt, ob der Name Zeichen traegt, die wie andere aussehen.
//
// DAS IST DER HAEUFIGSTE ANGRIFF AUF DIESEN MECHANISMUS. Das kyrillische a
// (U+0430) sieht aus wie das lateinische a. "bank.de" mit einem kyrillischen
// a ist ein anderer Rechner, aber auf dem Bildschirm ist es derselbe Name.
//
// Zwei Dinge halten das auf, und beide braucht es:
//
//  1. VERGLICHEN wird die ASCII-Form. Der Browser wandelt einen Namen mit
//     fremden Zeichen in Punycode: aus dem kyrillischen "bank.de" wird
//     "xn--bnk-6cd.de". Das ist nicht "bank.de", also passt kein Eintrag,
//     also wird nichts ausgefuellt. Der Schutz steht schon durch den
//     Zeichen-fuer-Zeichen-Vergleich; hier wird er nur sichtbar.
//  2. GEZEIGT wird dem Anwender ebenfalls die ASCII-Form, nie die huebsche.
//     Eine Oberflaeche, die "bank.de" anzeigt, waehrend der Rechner
//     "xn--bnk-6cd.de" heisst, luegt den Anwender an - freundlich, aber sie
//     luegt. Darum zeigt die Erweiterung ueberall das, was verglichen wird.
//
// Diese Funktion liefert den Anlass fuer den Hinweis, den der Anwender dazu
// sieht. Sie SPERRT nicht: ein Name mit fremden Zeichen kann vollkommen
// richtig sein (ein deutscher Umlaut, ein griechischer Laden), und wer ihn
// hinterlegt hat, soll ihn benutzen koennen. Sie sagt nur: schau genau hin.
function oskLooksForeign(host) {
  const text = String(host == null ? '' : host).toLowerCase();
  if (!text) {
    return false;
  }
  return text.split('.').some((label) => label.startsWith('xn--'));
}

// oskPickLanguage nimmt die erste Sprache, die es wirklich gibt.
function oskPickLanguage(preferred, available, fallback) {
  const list = Array.isArray(preferred) ? preferred : [];
  const have = Array.isArray(available) ? available : [];
  for (const entry of list) {
    const code = String(entry || '').slice(0, 2).toLowerCase();
    if (have.includes(code)) {
      return code;
    }
  }
  return fallback;
}

// oskFillForm traegt die Werte in die Seite ein. Diese Funktion laeuft IN
// der Seite: sie wird als Quelltext hineingegeben und sieht darum nichts
// aus dieser Datei. Alles, was sie braucht, muss in ihr stehen.
//
// Der letzte Riegel steckt in der ersten Zeile: die Adresse wird noch
// einmal geprueft, aus dem Dokument selbst. Haette sich der Reiter zwischen
// dem Lesen der Adresse und dem Eintragen weiterbewegt, landete das
// Geheimnis sonst auf der neuen Seite. Dieses Fenster ist klein - aber es
// ist da, und Phishing lebt von kleinen Fenstern.
function oskFillForm(expectedOrigin, user, secret) {
  // Riegel 1: wir stehen im Hauptdokument. Der Dienst gibt dieses Skript mit
  // allFrames:false hinein, also kann das hier nicht anders sein - und
  // genau darum steht die Abfrage da. Wer eines Tages allFrames:true
  // schreibt, faellt hier auf und nicht beim Anwender.
  if (window.top !== window.self) {
    return { ok: false, reason: 'frame' };
  }
  // Riegel 2: die Adresse wird noch einmal aus dem Dokument gelesen.
  const here = location.protocol + '//' +
    String(location.hostname).toLowerCase().replace(/\.$/, '');
  if (here !== expectedOrigin) {
    return { ok: false, reason: 'moved' };
  }
  const usable = (node) => {
    if (node.disabled || node.readOnly) {
      return false;
    }
    const box = node.getBoundingClientRect();
    return box.width > 0 && box.height > 0;
  };
  const passwords = Array.prototype.slice.call(
    document.querySelectorAll('input[type="password"]')).filter(usable);
  if (passwords.length === 0) {
    // Kein Passwortfeld in DIESEM Dokument - aber vielleicht steckt eines
    // in einem eingebetteten Rahmen. Das ist der haeufige Fall bei
    // Anmeldungen ueber einen Dritten und bei Bezahlseiten.
    //
    // WIR FUELLEN DORT NICHT. Der Rahmen gehoert einer anderen Software als
    // der Seite, deren Adresse in der Adresszeile steht; was er anzeigt,
    // ist eine Abfrage dieser anderen Software, und der Eintrag gilt fuer
    // die Adresse in der Zeile. Still "nichts gefunden" zu melden waere
    // aber falsch: der Anwender SIEHT ein Anmeldefeld. Darum wird der Fall
    // benannt, und der Anwender erfaehrt, wessen Rahmen es ist.
    const frames = Array.prototype.slice.call(
      document.querySelectorAll('iframe, frame')).filter(usable);
    if (frames.length > 0) {
      let foreign = '';
      for (const frame of frames) {
        let host = '';
        try {
          host = new URL(frame.src, location.href).hostname.toLowerCase();
        } catch (error) {
          host = '';
        }
        if (host && host !== location.hostname.toLowerCase()) {
          foreign = host;
          break;
        }
      }
      return { ok: false, reason: 'frame', frame: foreign };
    }
    return { ok: false, reason: 'nofield' };
  }
  const password = passwords[0];
  const scope = password.form || document;
  const names = Array.prototype.slice.call(scope.querySelectorAll(
    'input[type="text"], input[type="email"], input[type="tel"], input:not([type])'
  )).filter(usable);

  const set = (node, value) => {
    node.focus();
    // Der Wert wird ueber den Setter des Prototyps gesetzt. Seiten mit
    // eigenem Zustand (React und Verwandte) merken eine direkte Zuweisung
    // sonst nicht und schicken beim Absenden das leere Feld.
    const setter = Object.getOwnPropertyDescriptor(
      HTMLInputElement.prototype, 'value');
    if (setter && setter.set) {
      setter.set.call(node, value);
    } else {
      node.value = value;
    }
    node.dispatchEvent(new Event('input', { bubbles: true }));
    node.dispatchEvent(new Event('change', { bubbles: true }));
  };

  // Das Namensfeld ist das LETZTE vor dem Passwortfeld - so stehen
  // Anmeldeformulare, und so trifft man auch die Seiten, die vor dem
  // Namensfeld noch ein Suchfeld haben.
  let nameField = null;
  for (const candidate of names) {
    if (password.compareDocumentPosition(candidate) &
        Node.DOCUMENT_POSITION_PRECEDING) {
      nameField = candidate;
    }
  }
  if (nameField && user) {
    set(nameField, user);
  }
  set(password, secret);
  password.blur();
  return { ok: true, name: !!(nameField && user) };
}

// Fuer den Pruefsatz mit node. Im Browser gibt es kein module, darum die
// Abfrage - und darum ist diese Datei ein gewoehnliches Skript und kein
// Modul.
if (typeof module !== 'undefined' && module.exports) {
  module.exports = {
    oskOriginOf, oskIsLocal, oskSiteHost, oskMatches, oskLooksForeign,
    oskPickLanguage, oskFillForm,
  };
}
