// Der Hintergrunddienst der Erweiterung.
//
// Er ist die einzige Stelle, die mit dem Programm osk spricht, und die
// einzige, die die Adresse der Seite feststellt.
//
// DIE ADRESSE KOMMT AUS DER ADRESSZEILE, NIE VON DER SEITE.
//
// Das ist der Kern des Phishing-Schutzes auf dieser Seite des Kanals: die
// Adresse wird mit chrome.tabs aus dem Reiter gelesen, also von der
// Oberflaeche des Browsers. Eine Seite kann sie nicht beeinflussen - sie
// koennte sonst behaupten, sie sei die Bank. Das Anzeigefenster bekommt die
// Adresse nur zum ANZEIGEN; was zaehlt, ist die, die hier gelesen wird.
//
// Der zweite Riegel sitzt in osk selbst (classes/native_origin.go). Zwei
// Riegel, weil dieser hier in einer uebernommenen Erweiterung faellt.
'use strict';

// Die Entscheidungen stehen in rules.js - dort sind sie einzeln pruefbar.
// Ein Dienst ohne Module laedt sie so; der Pfad ist relativ zu dieser Datei.
importScripts('rules.js');

// Der Name des Wirts. Er steht genauso in der Anmeldedatei des Browsers.
const HOST = 'li.stw.opensecurekey';

// Wie lange ein Geheimnis in der Zwischenablage bleibt.
const CLIPBOARD_WIPE_MS = 20000;

let port = null;
let nextId = 1;
const waiting = new Map();

// connect haelt eine Verbindung zum Wirt. Sie wird beim ersten Bedarf
// aufgebaut und bei einem Abbruch neu - ein Browser schlaeft den Dienst
// jederzeit ein.
function connect() {
  if (port) {
    return port;
  }
  port = chrome.runtime.connectNative(HOST);
  port.onMessage.addListener((message) => {
    const open = waiting.get(message && message.id);
    if (open) {
      waiting.delete(message.id);
      open.resolve(message);
    }
  });
  port.onDisconnect.addListener(() => {
    const reason = (chrome.runtime.lastError && chrome.runtime.lastError.message)
      ? chrome.runtime.lastError.message : 'disconnected';
    port = null;
    for (const [id, open] of waiting) {
      waiting.delete(id);
      open.reject(new Error(reason));
    }
  });
  return port;
}

// ask schickt eine Frage an osk und wartet auf die Antwort.
function ask(request) {
  return new Promise((resolve, reject) => {
    let channel;
    try {
      channel = connect();
    } catch (error) {
      reject(error);
      return;
    }
    const id = nextId++;
    waiting.set(id, { resolve, reject });
    // Eine Frage ohne Antwort darf nicht ewig haengen - sonst wartet das
    // Fenster still, und der Anwender weiss nicht, woran er ist.
    const timer = setTimeout(() => {
      if (waiting.delete(id)) {
        reject(new Error('timeout'));
      }
    }, 120000);
    const done = () => clearTimeout(timer);
    waiting.set(id, {
      resolve: (message) => { done(); resolve(message); },
      reject: (error) => { done(); reject(error); },
    });
    try {
      channel.postMessage(Object.assign({ id }, request));
    } catch (error) {
      waiting.delete(id);
      done();
      reject(error);
    }
  });
}

// activeOrigin liest die Adresse des Reiters, in dem der Anwender steht.
//
// AUS DER ADRESSZEILE, NIE VON DER SEITE. Das ist der Kern des
// Phishing-Schutzes auf dieser Seite des Kanals: chrome.tabs liefert die
// Adresse aus der Oberflaeche des Browsers, und eine Seite kann sie nicht
// beeinflussen. Sie koennte sonst behaupten, sie sei die Bank.
async function activeOrigin() {
  // lastFocusedWindow, NICHT currentWindow. Gemessen am 24.10.2026 in einem
  // echten Chromium: "currentWindow" meint das Fenster, in dem der RUFENDE
  // Code steht - und dieser Dienst steht in keinem Fenster. Die Abfrage kam
  // leer zurueck, und die Erweiterung meldete "keine Seite", obwohl eine
  // Seite offen war.
  let tabs = await chrome.tabs.query({ active: true, lastFocusedWindow: true });
  if (!tabs || tabs.length === 0) {
    // Kein Fenster hat den Fokus (das kommt vor, wenn der Browser im
    // Hintergrund steht): dann der aktive Reiter ueberhaupt.
    tabs = await chrome.tabs.query({ active: true });
  }
  const tab = tabs && tabs[0];
  if (!tab || !tab.url) {
    return { error: 'notab' };
  }
  const where = oskOriginOf(tab.url);
  if (where.error) {
    return where;
  }
  return { tabId: tab.id, origin: where.origin, shown: where.shown };
}

// Die Fragen des Anzeigefensters. Es bekommt NIE die Erlaubnis, eine
// Adresse selbst zu nennen: jede Antwort bezieht sich auf den Reiter, der
// gerade vorn ist.
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  handle(request)
    .then((answer) => sendResponse(answer))
    .catch((error) => sendResponse({ ok: false, error: String(error && error.message) }));
  return true;
});

async function handle(request) {
  const was = request && request.was;
  if (was === 'state') {
    const where = await activeOrigin();
    if (where.error) {
      return { ok: false, reason: where.error, shown: where.shown || '' };
    }
    const hello = await ask({ was: 'hello' });
    if (!hello.ok) {
      return { ok: false, reason: 'device', error: hello.error, shown: where.shown };
    }
    if (!hello.unlocked) {
      return {
        ok: true, unlocked: false, shown: where.shown,
        origin: where.origin, device: hello.device, claim: hello.claim,
      };
    }
    const list = await ask({ was: 'list', origin: where.origin });
    if (!list.ok) {
      return { ok: false, reason: 'device', error: list.error, shown: where.shown };
    }
    return {
      ok: true, unlocked: true, shown: where.shown, origin: where.origin,
      matches: list.matches || [], device: hello.device, claim: hello.claim,
    };
  }

  if (was === 'unlock') {
    const answer = await ask({ was: 'unlock', pin: String(request.pin || '') });
    return { ok: !!answer.ok, error: answer.error };
  }

  if (was === 'lock') {
    const answer = await ask({ was: 'lock' });
    return { ok: !!answer.ok, error: answer.error };
  }

  if (was === 'fill' || was === 'copy') {
    const where = await activeOrigin();
    if (where.error) {
      return { ok: false, reason: where.error };
    }
    const item = Number(request.item);
    if (!Number.isInteger(item) || item <= 0) {
      return { ok: false, reason: 'item' };
    }
    // Benutzername und Geheimnis werden getrennt geholt: der Name braucht
    // keine Freigabe am Geraet, das Geheimnis schon.
    const user = await ask({
      was: 'reveal', origin: where.origin, item, feld: 'user',
    });
    const secret = await ask({
      was: 'reveal', origin: where.origin, item, feld: 'secret',
    });
    if (!user.ok || !secret.ok) {
      return {
        ok: false, reason: 'device',
        error: (secret.error || user.error),
      };
    }
    if (was === 'copy') {
      // Kopieren macht das Fenster selbst: der Dienst hat kein Dokument
      // und darf darum nicht in die Zwischenablage schreiben.
      return { ok: true, value: secret.value, wipeMs: CLIPBOARD_WIPE_MS };
    }
    const result = await fillTab(where.tabId, where.origin, user.value, secret.value);
    // Das Geheimnis wird hier nicht weitergegeben, nur das Ergebnis.
    return result;
  }

  return { ok: false, reason: 'unknown' };
}

// fillTab traegt die Werte in die Seite ein.
//
// Das Skript wird erst JETZT in die Seite gegeben, auf Knopfdruck, in genau
// diesen einen Reiter - nicht dauerhaft in alle Seiten. Darum braucht die
// Erweiterung kein Recht auf fremde Seiten; "activeTab" genuegt.
async function fillTab(tabId, origin, user, secret) {
  let results;
  try {
    results = await chrome.scripting.executeScript({
      target: { tabId, allFrames: false },
      func: oskFillForm,
      args: [origin, user, secret],
    });
  } catch (error) {
    return { ok: false, reason: 'noscript', error: String(error && error.message) };
  }
  const first = results && results[0] && results[0].result;
  if (!first || !first.ok) {
    return { ok: false, reason: (first && first.reason) || 'nofield' };
  }
  return { ok: true };
}
