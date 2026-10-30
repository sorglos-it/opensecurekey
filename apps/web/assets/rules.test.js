// Pruefungen fuer die Entscheidungen der Erweiterung.
//
//	node --test apps/web/assets/rules.test.js
//
// Der Phishing-Schutz steht und faellt mit oskOriginOf: eine Adresse, die
// hier falsch gelesen wird, ist ein Passwort auf der falschen Seite. Darum
// stehen hier vor allem die Faelle, mit denen so eine Pruefung ueblicherweise
// ueberlistet wird.
//
// Fuer oskFillForm wird ein winziges Dokument nachgebaut - gerade so viel,
// wie die Funktion anfasst. Ein ganzer Browser waere dafuer nicht noetig und
// wuerde die Pruefung von einem Fremdpaket abhaengig machen.
'use strict';

const test = require('node:test');
const assert = require('node:assert');
const { oskOriginOf, oskIsLocal, oskPickLanguage, oskFillForm } =
  require('./rules.js');

test('https wird gelesen, Pfad und Abfrage fallen weg', () => {
  for (const url of [
    'https://example.org',
    'https://example.org/',
    'https://example.org/anmelden',
    'https://example.org/?weiter=https://boese.tld',
    'https://example.org/anmelden#oben',
    'https://example.org:8443/',
    'https://EXAMPLE.ORG/',
    'https://example.org./',
  ]) {
    const got = oskOriginOf(url);
    assert.strictEqual(got.error, undefined, url);
    assert.strictEqual(got.origin, 'https://example.org', url);
    assert.strictEqual(got.host, 'example.org', url);
  }
});

test('ein Benutzername in der Adresse wird abgewiesen', () => {
  // Der aelteste Trick: sieht aus wie die Bank, zeigt auf den Angreifer.
  for (const url of [
    'https://bank.example.org@boese.tld/',
    'https://bank.example.org:geheim@boese.tld/',
  ]) {
    assert.strictEqual(oskOriginOf(url).error, 'notab', url);
  }
});

test('http gilt nur auf dem eigenen Rechner', () => {
  assert.strictEqual(oskOriginOf('http://example.org/').error, 'insecure');
  assert.strictEqual(oskOriginOf('http://192.168.1.10/').error, 'insecure');
  for (const url of [
    'http://localhost:8101/',
    'http://osk.localhost/',
    'http://127.0.0.1:8101/',
    'http://127.1.2.3/',
  ]) {
    const got = oskOriginOf(url);
    assert.strictEqual(got.error, undefined, url);
  }
});

test('was keine Seite ist, ist keine Adresse', () => {
  for (const url of [
    'about:blank',
    'chrome://extensions',
    'file:///C:/geheim.txt',
    'view-source:https://example.org/',
    'javascript:alert(1)',
    'data:text/html,<p>x',
    '',
    'nicht einmal eine Adresse',
  ]) {
    assert.strictEqual(oskOriginOf(url).error, 'notab', url);
  }
});

test('der eigene Rechner wird genau erkannt', () => {
  for (const host of ['localhost', 'osk.localhost', '127.0.0.1', '127.9.9.9', '::1']) {
    assert.ok(oskIsLocal(host), host);
  }
  for (const host of [
    'localhost.boese.tld',   // nicht der eigene Rechner
    'notlocalhost',
    '127.0.0.1.boese.tld',
    '0.0.0.0',
    '192.168.0.1',
    'example.org',
  ]) {
    assert.ok(!oskIsLocal(host), host);
  }
});

test('die Sprache wird aus den vorhandenen Dateien gewaehlt', () => {
  const have = ['de', 'en', 'es', 'fr', 'it'];
  assert.strictEqual(oskPickLanguage(['de-DE', 'en'], have, 'en'), 'de');
  assert.strictEqual(oskPickLanguage(['pt-BR', 'it-IT'], have, 'en'), 'it');
  assert.strictEqual(oskPickLanguage(['pt', 'nl'], have, 'en'), 'en');
  assert.strictEqual(oskPickLanguage([], have, 'en'), 'en');
  assert.strictEqual(oskPickLanguage(null, have, 'en'), 'en');
  assert.strictEqual(oskPickLanguage(['DE'], have, 'en'), 'de');
  // Eine Sprache, die es nicht als Datei gibt, wird nicht genommen.
  assert.strictEqual(oskPickLanguage(['nl'], ['de', 'en'], 'en'), 'en');
});

// --- Ein winziges Dokument fuer oskFillForm ------------------------------------

// fakeInput ist ein Eingabefeld mit genau dem, was die Funktion anfasst.
function fakeInput(type, options) {
  const settings = options || {};
  return {
    type,
    form: settings.form || null,
    value: '',
    disabled: !!settings.disabled,
    readOnly: !!settings.readOnly,
    hidden: !!settings.hidden,
    events: [],
    focused: 0,
    blurred: 0,
    focus() { this.focused++; },
    blur() { this.blurred++; },
    getBoundingClientRect() {
      return this.hidden ? { width: 0, height: 0 } : { width: 180, height: 24 };
    },
    dispatchEvent(event) { this.events.push(event.type); return true; },
    compareDocumentPosition() { return 0; },
  };
}

// buildDocument baut die Globalen nach, die oskFillForm benutzt. Die
// Reihenfolge der Felder ist die Reihenfolge im Feld - genau wie im
// Dokument.
function buildDocument(order, origin) {
  const url = new URL(origin);
  const matches = (node, selector) => {
    if (selector.includes('iframe')) {
      return node.type === 'iframe';
    }
    if (selector.includes('password')) {
      return node.type === 'password';
    }
    return ['text', 'email', 'tel', ''].includes(node.type);
  };
  const query = (list) => (selector) => list.filter((n) => matches(n, selector));

  global.location = {
    protocol: url.protocol, hostname: url.hostname, href: origin,
  };
  // Das Fenster. Im Hauptdokument ist top dasselbe wie self; ein Rahmen
  // wird weiter unten eigens gebaut.
  global.window = {};
  global.window.top = global.window;
  global.window.self = global.window;
  global.URL = URL;
  global.Node = { DOCUMENT_POSITION_PRECEDING: 2 };
  global.Event = class {
    constructor(type) { this.type = type; }
  };
  global.HTMLInputElement = { prototype: {} };
  global.document = { querySelectorAll: query(order) };

  // Die Lage im Dokument: ein Feld steht vor dem Passwortfeld, wenn es im
  // Feld vorher kommt.
  for (const node of order) {
    node.compareDocumentPosition = (other) => {
      return order.indexOf(other) < order.indexOf(node)
        ? Node.DOCUMENT_POSITION_PRECEDING : 0;
    };
    if (node.form) {
      node.form.querySelectorAll = query(order.filter((n) => n.form === node.form));
    }
  }
  return order;
}

test('gefuellt wird das letzte Namensfeld vor dem Passwortfeld', () => {
  const suche = fakeInput('text');
  const name = fakeInput('email');
  const passwort = fakeInput('password');
  buildDocument([suche, name, passwort], 'https://example.org');

  const got = oskFillForm('https://example.org', 'thomas', 'geheim');
  assert.deepStrictEqual(got, { ok: true, name: true });
  assert.strictEqual(name.value, 'thomas');
  assert.strictEqual(passwort.value, 'geheim');
  // Das Suchfeld davor bleibt leer - sonst stuende der Benutzername in der
  // Suche und ginge mit ihr weg.
  assert.strictEqual(suche.value, '');
  // Die Seite muss die Aenderung mitbekommen.
  assert.deepStrictEqual(passwort.events, ['input', 'change']);
  assert.strictEqual(passwort.blurred, 1);
});

test('auf einer anderen Adresse wird NICHTS gefuellt', () => {
  const name = fakeInput('text');
  const passwort = fakeInput('password');
  buildDocument([name, passwort], 'https://boese.tld');

  const got = oskFillForm('https://example.org', 'thomas', 'geheim');
  assert.deepStrictEqual(got, { ok: false, reason: 'moved' });
  assert.strictEqual(passwort.value, '');
  assert.strictEqual(name.value, '');
});

test('eine Unterdomaene ist eine andere Adresse', () => {
  const passwort = fakeInput('password');
  buildDocument([passwort], 'https://login.example.org');
  assert.deepStrictEqual(oskFillForm('https://example.org', 'a', 'b'),
    { ok: false, reason: 'moved' });
  assert.strictEqual(passwort.value, '');
});

test('ohne Passwortfeld wird nichts gefuellt', () => {
  const name = fakeInput('text');
  buildDocument([name], 'https://example.org');
  assert.deepStrictEqual(oskFillForm('https://example.org', 'a', 'b'),
    { ok: false, reason: 'nofield' });
  assert.strictEqual(name.value, '');
});

test('unsichtbare und gesperrte Felder zaehlen nicht', () => {
  const falle = fakeInput('password', { hidden: true });
  const echt = fakeInput('password');
  buildDocument([falle, echt], 'https://example.org');
  const got = oskFillForm('https://example.org', '', 'geheim');
  assert.strictEqual(got.ok, true);
  // Ein unsichtbares Passwortfeld ist der klassische Koeder: es wird
  // mitgeschickt, ohne dass der Anwender es sieht.
  assert.strictEqual(falle.value, '');
  assert.strictEqual(echt.value, 'geheim');

  const gesperrt = fakeInput('password', { disabled: true });
  const offen = fakeInput('password');
  buildDocument([gesperrt, offen], 'https://example.org');
  oskFillForm('https://example.org', '', 'geheim');
  assert.strictEqual(gesperrt.value, '');
  assert.strictEqual(offen.value, 'geheim');
});

test('ohne Benutzernamen wird nur das Passwort gefuellt', () => {
  const name = fakeInput('text');
  const passwort = fakeInput('password');
  buildDocument([name, passwort], 'https://example.org');
  const got = oskFillForm('https://example.org', '', 'geheim');
  assert.deepStrictEqual(got, { ok: true, name: false });
  assert.strictEqual(name.value, '');
  assert.strictEqual(passwort.value, 'geheim');
});

test('ein Feld hinter dem Passwortfeld ist kein Namensfeld', () => {
  const passwort = fakeInput('password');
  const danach = fakeInput('text');
  buildDocument([passwort, danach], 'https://example.org');
  const got = oskFillForm('https://example.org', 'thomas', 'geheim');
  assert.deepStrictEqual(got, { ok: true, name: false });
  assert.strictEqual(danach.value, '');
});

// --- Fremde Rahmen -----------------------------------------------------------
//
// Das Geraet zeigt immer, WELCHE Software gerade fragt. Fuer die Erweiterung
// heisst das: eine Abfrage aus einem eingebetteten Rahmen darf nicht als die
// Hauptseite auftreten. Sie wird nicht gefuellt, und sie wird benannt.

function fakeFrame(src) {
  const node = fakeInput('iframe');
  node.src = src;
  return node;
}

test('in einem Rahmen wird ueberhaupt nicht gefuellt', () => {
  const password = fakeInput('password');
  buildDocument([password], 'https://example.org');
  // Jetzt steht das Skript in einem Rahmen: top ist nicht self.
  global.window.top = { fremd: true };
  const got = oskFillForm('https://example.org', 'anna', 'gehe1m');
  assert.strictEqual(got.ok, false);
  assert.strictEqual(got.reason, 'frame');
  assert.strictEqual(password.value, '',
    'im Rahmen darf nichts im Feld stehen');
  global.window.top = global.window;
});

test('ein Anmeldefeld in einem fremden Rahmen wird benannt, nicht gefuellt', () => {
  // Das Hauptdokument hat kein Passwortfeld - das steckt im Rahmen eines
  // Dritten. Still "nichts gefunden" zu melden waere falsch: der Anwender
  // SIEHT ein Anmeldefeld.
  buildDocument([fakeFrame('https://anmeldung.fremd.example/form')],
    'https://example.org');
  const got = oskFillForm('https://example.org', 'anna', 'gehe1m');
  assert.strictEqual(got.ok, false);
  assert.strictEqual(got.reason, 'frame');
  assert.strictEqual(got.frame, 'anmeldung.fremd.example');
});

test('ein Rahmen derselben Seite macht aus nichts kein Problem', () => {
  // Ein Rahmen, der zur Seite selbst gehoert, ist kein fremder Rahmen. Der
  // Fall bleibt "frame" - gefuellt wird dort auch nicht -, aber es wird
  // kein fremder Name genannt, den es nicht gibt.
  buildDocument([fakeFrame('https://example.org/teil')], 'https://example.org');
  const got = oskFillForm('https://example.org', 'anna', 'gehe1m');
  assert.strictEqual(got.reason, 'frame');
  assert.strictEqual(got.frame, '');
});

test('ohne Rahmen und ohne Feld bleibt es bei nofield', () => {
  buildDocument([fakeInput('text')], 'https://example.org');
  const got = oskFillForm('https://example.org', 'anna', 'gehe1m');
  assert.strictEqual(got.reason, 'nofield');
});

test('ein Passwortfeld im Hauptdokument gewinnt gegen einen Rahmen daneben', () => {
  const password = fakeInput('password');
  buildDocument([fakeFrame('https://werbung.fremd.example/'), password],
    'https://example.org');
  const got = oskFillForm('https://example.org', 'anna', 'gehe1m');
  assert.strictEqual(got.ok, true);
  assert.strictEqual(password.value, 'gehe1m');
});

test('passt die Adresse nicht, bleibt das Feld leer', () => {
  // EINE WARNUNG, DIE MAN WEGKLICKEN KANN, WAEHREND DAS FELD SCHON GEFUELLT
  // IST, IST KEINE. Darum wird hier nicht die Meldung geprueft, sondern das
  // Feld: es muss leer bleiben, und zwar bevor irgendjemand etwas sieht.
  const password = fakeInput('password');
  const name = fakeInput('text');
  buildDocument([name, password], 'https://boese.tld');
  const got = oskFillForm('https://example.org', 'anna', 'gehe1m');
  assert.strictEqual(got.ok, false);
  assert.strictEqual(got.reason, 'moved');
  assert.strictEqual(password.value, '');
  assert.strictEqual(name.value, '');
  assert.strictEqual(password.focused, 0, 'nicht einmal angefasst');
});
