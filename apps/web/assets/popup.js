// Das Anzeigefenster der Erweiterung.
//
// Es zeigt an und fragt nach - es entscheidet nichts. Welche Eintraege zu
// dieser Seite gehoeren, sagt der Hintergrunddienst (und der fragt osk);
// ausgefuellt wird nur, was von dort kommt. Dieses Fenster kennt keine
// Adresse, die es selbst gewaehlt hat, und es sieht ein Geheimnis nur in dem
// einen Moment, in dem es es in die Zwischenablage legt.
'use strict';

const el = (id) => document.getElementById(id);

// show zeigt genau einen der drei Bereiche.
function show(which) {
  for (const name of ['trouble', 'locked', 'open']) {
    el(name).hidden = (name !== which);
  }
}

// note schreibt die Zeile unter den Bereichen. Sie ist eine Statuszeile mit
// aria-live, damit ein Vorleseprogramm sie mitbekommt.
function note(text) {
  el('note').textContent = text || '';
}

// ask fragt den Hintergrunddienst.
function ask(request) {
  return new Promise((resolve) => {
    chrome.runtime.sendMessage(request, (answer) => {
      if (chrome.runtime.lastError) {
        resolve({ ok: false, reason: 'host',
          error: chrome.runtime.lastError.message });
        return;
      }
      resolve(answer || { ok: false, reason: 'host' });
    });
  });
}

// refresh holt den Zustand und baut das Fenster neu.
async function refresh() {
  const answer = await ask({ was: 'state' });
  el('site').textContent = answer.shown || '';
  el('device').textContent = '';
  el('claim').textContent = '';

  if (!answer.ok) {
    show('trouble');
    el('troubleText').textContent = troubleText(answer);
    return;
  }
  if (answer.device) {
    const label = answer.device.simulated
      ? t('popup.simulated')
      : (answer.device.serial || answer.device.label || '');
    el('device').textContent = t('popup.device') + ': ' + label;
  }
  // Der Satz zur Behauptung kommt von osk, damit er ueberall derselbe ist.
  el('claim').textContent = answer.claim || t('popup.claim');

  if (!answer.unlocked) {
    show('locked');
    el('pin').value = '';
    el('pin').focus();
    return;
  }
  show('open');
  buildMatches(answer.matches || []);
}

// troubleText sagt, was fehlt - in Worten, die weiterhelfen.
function troubleText(answer) {
  switch (answer.reason) {
    case 'insecure':
      return t('popup.insecure');
    case 'notab':
      return t('popup.nofield');
    case 'host':
      return t('popup.nohost');
    case 'device':
      return answer.error || t('popup.nodevice');
    default:
      return answer.error || t('popup.error');
  }
}

// buildMatches baut die Liste der Eintraege dieser Adresse.
function buildMatches(matches) {
  const list = el('matches');
  list.textContent = '';
  el('none').hidden = matches.length > 0;
  for (const match of matches) {
    const row = document.createElement('li');

    const name = document.createElement('span');
    name.className = 'name';
    name.textContent = match.name;
    row.appendChild(name);

    const site = document.createElement('span');
    site.className = 'where';
    site.textContent = match.site;
    row.appendChild(site);

    const fill = document.createElement('button');
    fill.type = 'button';
    fill.textContent = t('popup.fill');
    fill.addEventListener('click', () => doFill(match.id));
    row.appendChild(fill);

    const copy = document.createElement('button');
    copy.type = 'button';
    copy.className = 'quiet';
    copy.textContent = t('popup.copy');
    copy.addEventListener('click', () => doCopy(match.id));
    row.appendChild(copy);

    list.appendChild(row);
  }
}

// busy sperrt die Knoepfe, solange das Geraet gefragt wird. Ohne das
// klickt ein ungeduldiger Anwender zweimal und loest zwei Freigaben aus.
function busy(on) {
  for (const button of document.querySelectorAll('button')) {
    button.disabled = on;
  }
  if (on) {
    note(t('popup.wait'));
  }
}

async function doFill(item) {
  busy(true);
  const answer = await ask({ was: 'fill', item });
  busy(false);
  if (answer.ok) {
    note(t('popup.filled'));
    return;
  }
  note(fillTrouble(answer));
}

async function doCopy(item) {
  busy(true);
  const answer = await ask({ was: 'copy', item });
  busy(false);
  if (!answer.ok) {
    note(fillTrouble(answer));
    return;
  }
  try {
    await navigator.clipboard.writeText(answer.value);
  } catch (error) {
    note(t('popup.error'));
    return;
  }
  const seconds = Math.round((answer.wipeMs || 20000) / 1000);
  note(t('popup.copied', seconds));
  // Die Zwischenablage wird wieder geleert. Das gelingt nur, solange dieses
  // Fenster offen ist - darum steht die Frist in der Meldung, damit
  // niemand sich darauf verlaesst.
  setTimeout(() => {
    navigator.clipboard.writeText('').catch(() => {});
  }, answer.wipeMs || 20000);
}

function fillTrouble(answer) {
  switch (answer.reason) {
    case 'nofield':
      return t('popup.nofield');
    case 'moved':
      return t('popup.wrongsite');
    case 'insecure':
      return t('popup.insecure');
    case 'noscript':
      return t('popup.nofield');
    default:
      return answer.error || t('popup.error');
  }
}

// Start: erst die Texte, dann der Zustand.
(async () => {
  await load();
  applyTexts(document);
  el('unlockForm').addEventListener('submit', async (event) => {
    event.preventDefault();
    const pin = el('pin').value;
    el('pin').value = '';
    busy(true);
    const answer = await ask({ was: 'unlock', pin });
    busy(false);
    if (!answer.ok) {
      note(answer.error || t('popup.error'));
      return;
    }
    note('');
    await refresh();
  });
  el('lock').addEventListener('click', async () => {
    await ask({ was: 'lock' });
    await refresh();
  });
  await refresh();
})();
