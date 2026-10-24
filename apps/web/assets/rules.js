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
  module.exports = { oskOriginOf, oskIsLocal, oskPickLanguage, oskFillForm };
}
