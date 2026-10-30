// Die Fallliste des Adressvergleichs.
//
//	node --test apps/web/tests/adressen.test.js
//
// Hier steht die Pruefung, an der die ganze Erweiterung haengt: gehoert
// dieser Eintrag zu dieser Seite? Ein Ja zu viel ist ein Passwort beim
// Angreifer. Ein Nein zu viel ist ein Anwender, der die Erweiterung
// abschaltet und sein Passwort wieder abtippt - und damit beim naechsten
// Angreifer landet. Beide Richtungen kosten, also werden beide geprueft.
//
// DIE LISTE HAT ZWEI HAELFTEN, UND BEIDE SIND PFLICHT:
//
//	PASST       - Paare, die zusammenfinden muessen
//	PASST NICHT - Paare, die NIE zusammenfinden duerfen
//
// Eine Pruefung, die nur die erste Haelfte hat, ist gruen, wenn die Regel
// "immer ja" lautet. Genau das ist der Fehler, den man nicht sieht.
//
// Jede Zeile wurde einzeln zum Anschlagen gebracht: mit einem oskMatches,
// das immer true liefert, fallen alle Zeilen der zweiten Haelfte; mit einem,
// das immer false liefert, alle der ersten. Gemessen am 30.10.2026, beide
// Male.
'use strict';

const test = require('node:test');
const assert = require('node:assert');
const { oskOriginOf, oskSiteHost, oskMatches, oskLooksForeign } =
  require('../assets/rules.js');

// host holt den Rechnernamen aus einer Seitenadresse, so wie es der
// Hintergrunddienst tut.
function host(url) {
  const where = oskOriginOf(url);
  assert.strictEqual(where.error, undefined, 'Seitenadresse unlesbar: ' + url);
  return where.host;
}

// --- PASST ------------------------------------------------------------------
//
// Was ein Mensch in das Feld eines Eintrags schreibt, und die Seite, auf der
// er dann steht. Dieselbe Tuer, nur anders aufgeschrieben.

const PASST = [
  // Dieselbe Schreibweise.
  ['example.org', 'https://example.org/'],
  // Das Feld als volle Adresse, mit Pfad - der Pfad zaehlt nicht.
  ['https://example.org/anmelden', 'https://example.org/'],
  // Das Feld nackt, die Seite tief im Pfad.
  ['example.org', 'https://example.org/kunde/42/konto?reiter=sicherheit'],
  // Grossschreibung auf beiden Seiten - ein Rechnername kennt keine.
  ['EXAMPLE.ORG', 'https://example.org/'],
  ['example.org', 'https://ExAmPlE.OrG/'],
  // Der abschliessende Punkt der Wurzelzone.
  ['example.org.', 'https://example.org/'],
  ['example.org', 'https://example.org./'],
  // Ein Port gehoert nicht zum Rechner. Das Geheimnis gehoert zum Haus,
  // nicht zu einer Tuer an ihm.
  ['example.org', 'https://example.org:8443/'],
  ['example.org:443', 'https://example.org/'],
  // Eine Unterdomaene, die AUSDRUECKLICH eingetragen ist. Sie passt, weil
  // sie dasteht - nicht, weil example.org dasteht.
  ['login.example.org', 'https://login.example.org/'],
  // Das Feld mit Pfad und ohne Verfahren, wie es viele schreiben.
  ['example.org/login', 'https://example.org/'],
  // Leerzeichen und Umbrueche AM RAND sind Tippfehler, keine Absicht - sie
  // fallen weg. Beim Schreiben dieser Liste stand ein Eintrag mit einem
  // Umbruch am Ende erst in der anderen Haelfte und schlug an: osk schneidet den Rand genauso ab
  // (TrimSpace in native_origin.go). Die Erweiterung strenger zu machen als
  // osk waere kein Gewinn - osk listete den Eintrag, und die Erweiterung
  // wiese ihn wortlos ab. Ein Umbruch MITTEN im Feld bleibt verboten.
  ['  example.org  ', 'https://example.org/'],
  ['example.org\r\n', 'https://example.org/'],
  // Der eigene Rechner, unverschluesselt - die einzige Ausnahme.
  ['localhost', 'http://localhost:8080/'],
  ['127.0.0.1', 'http://127.0.0.1:3000/'],
  // Eine Zahlenadresse.
  ['198.51.100.7', 'https://198.51.100.7/'],
  // Ein Name mit Umlaut, auf beiden Seiten gleich geschrieben. Der Browser
  // bringt beide Schreibweisen auf dieselbe ASCII-Form.
  ['münchen.example', 'https://münchen.example/'],
  ['xn--mnchen-3ya.example', 'https://münchen.example/'],
];

test('PASST: diese Paare muessen zusammenfinden', () => {
  for (const [site, url] of PASST) {
    assert.strictEqual(oskMatches(site, host(url)), true,
      'muesste passen: Eintrag "' + site + '" auf ' + url);
  }
});

// --- PASST NICHT ------------------------------------------------------------
//
// Jede Zeile hier ist ein Angriff oder ein Irrtum, der ein Passwort an die
// falsche Stelle traegt.

const PASST_NICHT = [
  // DER ANHAENGER. Der haeufigste Phishing-Name ueberhaupt: links steht
  // alles richtig, rechts steht der Angreifer.
  ['example.org', 'https://example.org.boese.tld/'],
  ['bank.example.org', 'https://bank.example.org.boese.tld/'],
  // DER TEILSTRING in die andere Richtung.
  ['example.org', 'https://notexample.org/'],
  ['example.org', 'https://example.org.co/'],
  ['example.org', 'https://xample.org/'],
  // DER BINDESTRICH statt des Punktes - sieht fluechtig gleich aus.
  ['bank.example.org', 'https://bank-example.org/'],
  ['example.org', 'https://example-org.tld/'],
  // UNTERDOMAENEN sind NICHT eingeschlossen, in keiner Richtung. Das ist
  // die Entscheidung, die am meisten kostet, und sie ist Absicht: wer
  // Unterdomaenen zulaesst, muss wissen, wo eine Domaene endet, und das
  // weiss niemand aus dem Namen allein.
  ['example.org', 'https://login.example.org/'],
  ['login.example.org', 'https://example.org/'],
  ['example.org', 'https://a.b.example.org/'],
  // Und erst recht nicht ueber eine Endung hinweg, unter der viele Leute
  // sitzen: diese beiden Namen gehoeren verschiedenen Kunden.
  ['kunde-a.example-hoster.net', 'https://kunde-b.example-hoster.net/'],
  // DER PLATZHALTER. Er ist keine Regel, er ist ein Eintrag, der nirgends
  // ausfuellt - auch nicht auf dem Namen, der danebensteht.
  ['*.example.org', 'https://login.example.org/'],
  ['*.example.org', 'https://example.org/'],
  ['*', 'https://example.org/'],
  // DER BENUTZERNAME IN DER ADRESSE. Der aelteste Trick der Zunft.
  ['https://example.org@boese.tld/', 'https://example.org/'],
  ['example.org', 'https://boese.tld/'],
  // GLEICH AUSSEHENDE ZEICHEN. Das kyrillische a (U+0430) in "bank".
  // Auf dem Bildschirm steht zweimal dasselbe; es sind zwei Rechner.
  ['bank.example', 'https://bаnk.example/'],
  ['bаnk.example', 'https://bank.example/'],
  // Dasselbe mit dem griechischen Omikron (U+03BF) in "shop".
  ['shop.example', 'https://shοp.example/'],
  // Der eingetragene Name in Punycode, die Seite in reinem Latein.
  ['xn--bnk-6cd.example', 'https://bank.example/'],
  // Ein Eintrag, der gar kein Rechnername ist.
  ['', 'https://example.org/'],
  ['   ', 'https://example.org/'],
  ['kein name', 'https://example.org/'],
  ['example.org/../boese.tld', 'https://boese.tld/'],
  // Ein Umbruch mitten im Feld.
  ['example.org\nboese.tld', 'https://example.org/'],
  // Ein anderes Verfahren im Feld.
  ['ftp://example.org', 'https://example.org/'],
  ['javascript:alert(1)', 'https://example.org/'],
  // Der eigene Rechner ist nicht jeder Rechner, dessen Name so anfaengt.
  ['localhost', 'https://localhost.boese.tld/'],
  ['127.0.0.1', 'https://127.0.0.1.boese.tld/'],
];

test('PASST NICHT: diese Paare duerfen NIE zusammenfinden', () => {
  for (const [site, url] of PASST_NICHT) {
    const where = oskOriginOf(url);
    if (where.error) {
      // Die Seite ist schon als Ort abgewiesen - dann gibt es dort nichts
      // auszufuellen, und das Paar ist erst recht getrennt.
      continue;
    }
    assert.strictEqual(oskMatches(site, where.host), false,
      'duerfte NICHT passen: Eintrag "' + site + '" auf ' + url);
  }
});

// --- Die Liste selbst -------------------------------------------------------
//
// Eine Fallliste, die versehentlich leer wird, ist gruen und prueft nichts.

test('beide Haelften der Liste sind gefuellt', () => {
  assert.ok(PASST.length >= 15, 'zu wenige Paare, die passen muessen');
  assert.ok(PASST_NICHT.length >= 25,
    'zu wenige Paare, die nicht passen duerfen');
});

test('die Liste schlaegt an, wenn die Regel nachgibt', () => {
  // Die Gegenprobe im Pruefsatz selbst: waere der Vergleich grosszuegiger -
  // etwa "endet auf" statt "ist gleich" -, muesste mindestens eine Zeile
  // der zweiten Haelfte fallen. Das wird hier nachgemessen, damit niemand
  // die Liste fuer eine Formalie haelt.
  const grosszuegig = (site, pageHost) => {
    const left = oskSiteHost(site);
    return !!left && (pageHost === left || pageHost.endsWith('.' + left));
  };
  const gefallen = PASST_NICHT.filter(([site, url]) => {
    const where = oskOriginOf(url);
    return !where.error && grosszuegig(site, where.host);
  });
  assert.ok(gefallen.length > 0,
    'die Liste faengt eine Unterdomaenen-Regel nicht - dann prueft sie nichts');
});

// --- Gleich aussehende Zeichen ----------------------------------------------

test('ein Name mit fremden Zeichen wird als solcher erkannt', () => {
  assert.strictEqual(oskLooksForeign(host('https://bаnk.example/')), true);
  assert.strictEqual(oskLooksForeign(host('https://münchen.example/')), true);
  assert.strictEqual(oskLooksForeign(host('https://bank.example/')), false);
  assert.strictEqual(oskLooksForeign(host('https://127.0.0.1/')), false);
});

test('gezeigt wird die Form, die verglichen wird', () => {
  // Das Fenster zeigt "shown". Stuende dort der huebsche Name, waehrend ein
  // anderer Rechner gemeint ist, waere die Anzeige eine Luege - freundlich,
  // aber eine Luege.
  const where = oskOriginOf('https://bаnk.example/');
  assert.strictEqual(where.shown, where.host);
  assert.strictEqual(where.shown.startsWith('xn--'), true,
    'der huebsche Name darf nicht angezeigt werden');
  assert.notStrictEqual(where.shown, 'bank.example');
});

test('oskSiteHost bringt beide Schreibweisen auf dieselbe Form', () => {
  assert.strictEqual(oskSiteHost('münchen.example'),
    'xn--mnchen-3ya.example');
  assert.strictEqual(oskSiteHost('xn--mnchen-3ya.example'),
    'xn--mnchen-3ya.example');
  assert.strictEqual(oskSiteHost('bаnk.example'), 'xn--bnk-6cd.example');
});
