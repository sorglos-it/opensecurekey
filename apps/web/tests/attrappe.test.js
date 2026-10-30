// Die Erweiterung gegen die Attrappe von osk.
//
//	node --test apps/web/tests/attrappe.test.js
//
// Geprueft wird hier nicht, ob die Attrappe hoeflich antwortet - das tut
// sie immer. Geprueft wird, ob die ERWEITERUNG sich an den Vertrag haelt,
// und ob sie bei einer Antwort, die gegen den Vertrag verstoesst, dicht
// bleibt. Die Attrappe weist jede Abweichung ab und schreibt sie in
// host.refused; eine leere Liste ist hier das Ergebnis, nicht die Annahme.
//
// Was diese Attrappe nicht beweisen kann, steht in osk-attrappe.js unter
// "WAS DIESE ATTRAPPE ANNIMMT UND NICHT BEWEISEN KANN".
'use strict';

const test = require('node:test');
const assert = require('node:assert');
const { Host, encode, decode } = require('./osk-attrappe.js');
const { oskOriginOf, oskMatches, oskSiteHost } = require('../assets/rules.js');

// --- Der Rahmen --------------------------------------------------------------

test('eine Nachricht geht durch den Rahmen und kommt gleich wieder heraus', () => {
  const frage = { id: 1, was: 'hello' };
  const { value, rest } = decode(encode(frage));
  assert.deepStrictEqual(value, frage);
  assert.strictEqual(rest.length, 0);
});

test('zwei Nachrichten hintereinander werden einzeln gelesen', () => {
  const strom = Buffer.concat([
    encode({ id: 1, was: 'hello' }),
    encode({ id: 2, was: 'state' }),
  ]);
  const erste = decode(strom);
  assert.strictEqual(erste.value.id, 1);
  const zweite = decode(erste.rest);
  assert.strictEqual(zweite.value.was, 'state');
});

test('eine Laenge von null wird abgewiesen', () => {
  const header = Buffer.alloc(4);
  assert.throws(() => decode(header), /error\.native\.size/);
});

// --- Was die Erweiterung schickt ---------------------------------------------

// Die Erweiterung baut ihre Adresse mit oskOriginOf und schickt
// origin = where.origin. Dass diese Zeichenkette fuer osk annehmbar ist,
// wird hier fuer JEDE lesbare Adresse der Fallliste nachgemessen - nicht an
// einem Beispiel.
const SEITEN = [
  'https://example.org/',
  'https://example.org/anmelden?weiter=1#oben',
  'https://EXAMPLE.ORG:8443/tief/im/pfad',
  'https://login.example.org/',
  'http://localhost:8080/',
  'http://127.0.0.1:3000/x',
  'https://198.51.100.7/',
  'https://bаnk.example/',
];

test('jede Adresse der Erweiterung ist fuer osk eine gueltige Adresse', () => {
  const host = new Host();
  for (const url of SEITEN) {
    const where = oskOriginOf(url);
    assert.strictEqual(where.error, undefined, url);
    const antwort = host.handle({ id: 1, was: 'list', origin: where.origin });
    assert.notStrictEqual(antwort.error, 'error.native.origin',
      'osk haelt die Adresse der Erweiterung fuer ungueltig: ' +
      where.origin + ' (aus ' + url + ')');
    // Und osk gibt dieselbe Adresse zurueck, wie es sie gelesen hat.
    assert.strictEqual(antwort.origin, where.origin, url);
  }
  assert.deepStrictEqual(host.refused, [],
    'die Attrappe hat eine Frage der Erweiterung abgewiesen');
});

test('die Attrappe weist ab, was nicht im Vertrag steht', () => {
  // Die Gegenprobe: ohne sie wuesste niemand, ob host.refused ueberhaupt
  // jemals gefuellt wird.
  const host = new Host();
  host.handle({ was: 'hello' });
  host.handle({ id: 1, was: 'list', origin: 'https://example.org', extra: 1 });
  host.handle({ id: 2, was: 'unlock' });
  assert.strictEqual(host.refused.length, 3, host.refused.join(' | '));
});

test('eine Adresse mit Pfad wird von osk abgewiesen', () => {
  // Die Erweiterung schickt nie einen Pfad. Kaeme doch einer, waere das ein
  // Fehler in der Erweiterung - und osk faengt ihn.
  const host = new Host();
  for (const schlecht of [
    'https://example.org/anmelden',
    'https://example.org/?x=1',
    'https://example.org@boese.tld',
    'ftp://example.org',
    'http://example.org',
    'https://exa mple.org',
  ]) {
    const antwort = host.handle({ id: 1, was: 'list', origin: schlecht });
    assert.strictEqual(antwort.ok, false, schlecht);
  }
});

// --- Was die Erweiterung mit der Antwort macht --------------------------------

test('die Liste von osk enthaelt nur Eintraege dieser Adresse', () => {
  const host = new Host();
  const where = oskOriginOf('https://example.org/');
  const antwort = host.handle({ id: 1, was: 'list', origin: where.origin });
  assert.strictEqual(antwort.ok, true);
  assert.strictEqual(antwort.matches.length, 1);
  assert.strictEqual(antwort.matches[0].site, 'example.org');
  // login.example.org ist NICHT dabei - das ist die Entscheidung, keine
  // Unterdomaenen einzuschliessen, an der Gegenstelle nachgemessen.
  assert.strictEqual(
    antwort.matches.some((m) => m.site.includes('login')), false);
});

test('kein Geheimnis in der Liste', () => {
  const host = new Host();
  const where = oskOriginOf('https://example.org/');
  const antwort = host.handle({ id: 1, was: 'list', origin: where.origin });
  for (const match of antwort.matches) {
    assert.deepStrictEqual(Object.keys(match).sort(),
      ['id', 'name', 'site', 'type']);
  }
});

test('die Erweiterung misst die Liste von osk nach', () => {
  // DAS IST DER ZWEITE RIEGEL. Eine Attrappe, die luegt: sie schickt einen
  // Eintrag mit einer fremden Adresse. Die Erweiterung (background.js
  // filtert mit oskMatches) darf ihn nicht durchlassen.
  const gelogen = [
    { id: 9, name: 'anna', site: 'boese.tld', type: 1 },
    { id: 1, name: 'anna', site: 'example.org', type: 1 },
    { id: 8, name: 'anna', site: '*.example.org', type: 1 },
    { id: 7, name: 'anna', site: 'example.org.boese.tld', type: 1 },
  ];
  const where = oskOriginOf('https://example.org/');
  const bleibt = gelogen.filter((m) => oskMatches(m.site, where.host));
  assert.deepStrictEqual(bleibt.map((m) => m.id), [1]);
});

test('ohne PIN gibt die Attrappe nichts heraus und merkt sich nichts', () => {
  const host = new Host();
  assert.strictEqual(host.handle({ id: 1, was: 'hello' }).unlocked, false);
  assert.strictEqual(
    host.handle({ id: 2, was: 'unlock', pin: 'falsch' }).ok, false);
  assert.strictEqual(host.unlocked, false);
  assert.strictEqual(
    host.handle({ id: 3, was: 'unlock', pin: '1234' }).ok, true);
  assert.strictEqual(host.unlocked, true);
});

test('reveal gibt nichts heraus, wenn die Adresse nicht passt', () => {
  const host = new Host();
  host.handle({ id: 1, was: 'unlock', pin: '1234' });
  // Eintrag 3 gehoert zu bank.example. Gefragt wird von example.org aus.
  const antwort = host.handle({
    id: 2, was: 'reveal', origin: 'https://example.org', item: 3,
    feld: 'secret',
  });
  assert.strictEqual(antwort.ok, false);
  assert.strictEqual(antwort.error, 'error.native.origin');
  assert.strictEqual(antwort.value, undefined);
});

test('reveal gibt heraus, wenn die Adresse passt', () => {
  const host = new Host();
  host.handle({ id: 1, was: 'unlock', pin: '1234' });
  const name = host.handle({
    id: 2, was: 'reveal', origin: 'https://example.org', item: 1,
    feld: 'user',
  });
  const secret = host.handle({
    id: 3, was: 'reveal', origin: 'https://example.org', item: 1,
    feld: 'secret',
  });
  assert.strictEqual(name.value, 'anna');
  assert.strictEqual(secret.value, 'gehe1m');
});

test('ein unbekanntes Feld bei reveal bekommt nichts', () => {
  const host = new Host();
  host.handle({ id: 1, was: 'unlock', pin: '1234' });
  const antwort = host.handle({
    id: 2, was: 'reveal', origin: 'https://example.org', item: 1,
    feld: 'alles',
  });
  assert.strictEqual(antwort.ok, false);
  assert.strictEqual(antwort.error, 'error.native.field');
});

test('die Attrappe und die Erweiterung lesen eine Adresse gleich', () => {
  // Beide Seiten bringen das Feld eines Eintrags auf einen Rechnernamen.
  // Weichen sie ab, listet osk einen Eintrag, den die Erweiterung dann
  // wortlos verwirft - der Anwender saehe einen Knopf, der nichts tut.
  const { siteHost } = require('./osk-attrappe.js');
  for (const site of [
    'example.org', 'https://example.org/login', 'EXAMPLE.ORG',
    'example.org.', 'example.org:443', 'login.example.org',
    '  example.org  ', 'localhost', '127.0.0.1', 'bank.example',
  ]) {
    assert.strictEqual(oskSiteHost(site), siteHost(site), site);
  }
});
