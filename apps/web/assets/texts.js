// Die Texte der Erweiterung.
//
// Kein Text steht im Code: jeder sichtbare Satz kommt ueber einen Schluessel
// aus lang/<code>.json. Fehlt ein Schluessel oder eine Sprache, springt
// Englisch ein - nie ein leerer Platz und nie der nackte Schluessel.
//
// Eine Abweichung von der Hausregel, und zwar eine, die die Plattform
// erzwingt: eine Erweiterung kann ihren eigenen Ordner nicht auflisten. Die
// vorhandenen Sprachen stehen darum in assets/langs.json - einer DATENdatei.
// Eine neue Sprache heisst also: eine Datei nach lang/ kopieren und eine
// Zeile in langs.json. Keine Codeaenderung, kein Neubau.
'use strict';

const FALLBACK = 'en';

let texts = {};
let ready = null;

// load holt die Texte: erst Englisch als Unterlage, dann die Sprache des
// Browsers darueber.
function load() {
  if (ready) {
    return ready;
  }
  ready = (async () => {
    const known = await readJson('assets/langs.json');
    const languages = (known && Array.isArray(known.languages))
      ? known.languages : [FALLBACK];
    const wanted = pickLanguage(languages);
    const merged = {};
    for (const code of [FALLBACK, wanted]) {
      if (!code || !languages.includes(code)) {
        continue;
      }
      const one = await readJson('lang/' + code + '.json');
      if (one) {
        Object.assign(merged, one);
      }
    }
    texts = merged;
    return texts;
  })();
  return ready;
}

// pickLanguage nimmt die Sprache des Browsers. Die Regel selbst steht in
// rules.js, damit sie einzeln geprueft werden kann.
function pickLanguage(languages) {
  const list = (navigator.languages && navigator.languages.length)
    ? navigator.languages : [navigator.language || FALLBACK];
  return oskPickLanguage(list, languages, FALLBACK);
}

// readJson liest eine Datei der Erweiterung. Nur eigene Dateien - die
// Adresse wird nie aus einer Nachricht gebaut.
async function readJson(path) {
  if (!/^[a-z0-9/_.-]+$/.test(path)) {
    return null;
  }
  try {
    const answer = await fetch(chrome.runtime.getURL(path));
    if (!answer.ok) {
      return null;
    }
    return await answer.json();
  } catch (error) {
    return null;
  }
}

// t gibt den Text zum Schluessel. %s wird durch das Argument ersetzt.
function t(key, value) {
  const text = texts[key] || key;
  if (value === undefined) {
    return text;
  }
  return text.replace('%s', String(value));
}

// applyTexts setzt alle Texte einer Seite: jedes Element mit data-text
// bekommt seinen Text, jedes mit data-placeholder seinen Platzhalter.
function applyTexts(root) {
  for (const node of root.querySelectorAll('[data-text]')) {
    node.textContent = t(node.dataset.text);
  }
  for (const node of root.querySelectorAll('[data-placeholder]')) {
    node.placeholder = t(node.dataset.placeholder);
  }
  for (const node of root.querySelectorAll('[data-label]')) {
    node.setAttribute('aria-label', t(node.dataset.label));
  }
}
