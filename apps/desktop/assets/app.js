// Oberflaeche: holt Texte und Daten ueber /api/*. Kein Geheimnis landet je in
// einer Adresse, in der Sitzungsablage oder im Verlauf - nur im Fenster, und
// die Zwischenablage wird nach kurzer Zeit geleert.
"use strict";

const wipeMs = Number(document.body.dataset.wipeMs || 20000);
const problem = document.getElementById("problem");

// Alle sichtbaren Texte kommen aus den Sprachdateien, nicht aus dem Code.
let texts = {};
let typeNames = {};

function typeName(number) {
	return typeNames[String(number)] || "";
}

function t(key) {
	return texts[key] || "";
}

function showProblem(text) {
	if (!problem) {
		return;
	}
	problem.textContent = text;
	problem.hidden = !text;
}

async function call(path, body) {
	showProblem("");
	const options = {
		method: body === undefined ? "GET" : "POST",
		headers: { "Accept": "application/json" },
	};
	if (body !== undefined) {
		options.headers["Content-Type"] = "application/json";
		options.body = JSON.stringify(body);
	}
	let answer;
	try {
		answer = await fetch(path, options);
	} catch (err) {
		showProblem(t("error.network"));
		return null;
	}
	let reply;
	try {
		reply = await answer.json();
	} catch (err) {
		showProblem(t("error.internal"));
		return null;
	}
	if (!reply.ok) {
		showProblem(reply.error || "");
		return null;
	}
	return reply.data;
}

// Zustand in der Kopfzeile: welches Geraet, offen oder zu.
async function refreshState() {
	const field = document.getElementById("state");
	if (!field) {
		return;
	}
	const state = await call("/api/state");
	if (!state) {
		field.textContent = "";
		return;
	}
	const parts = [];
	if (state.connected) {
		parts.push(state.device || "");
		if (state.info && state.info.firmware) {
			parts.push(state.info.firmware);
		}
	}
	field.textContent = parts.filter(Boolean).join(" · ");
}

function dateText(seconds) {
	if (!seconds) {
		return "";
	}
	return new Date(seconds * 1000).toLocaleString();
}

function cell(row, text) {
	const td = document.createElement("td");
	td.textContent = text || "";
	row.appendChild(td);
	return td;
}

// Uebersicht und Suche
function setupOverview() {
	const table = document.getElementById("items");
	if (!table) {
		return;
	}
	const body = table.querySelector("tbody");
	const empty = document.getElementById("empty");
	const dialog = document.getElementById("secret");
	const value = document.getElementById("secret-value");
	const nameField = document.getElementById("secret-name");
	let wipeTimer = 0;

	async function load(query) {
		const items = await call("/api/items?q=" + encodeURIComponent(query || ""));
		body.textContent = "";
		if (!items || items.length === 0) {
			empty.hidden = false;
			table.hidden = true;
			return;
		}
		empty.hidden = true;
		table.hidden = false;
		for (const item of items) {
			const row = document.createElement("tr");
			cell(row, item.name);
			cell(row, typeName(item.type));
			cell(row, item.site);
			cell(row, dateText(item.changed));
			const actions = cell(row, "");
			const reveal = document.createElement("button");
			reveal.type = "button";
			reveal.textContent = t("action.reveal");
			reveal.addEventListener("click", () => show(item));
			actions.appendChild(reveal);
			const edit = document.createElement("a");
			edit.className = "button";
			edit.href = "/item?id=" + encodeURIComponent(String(item.id));
			edit.textContent = t("action.edit");
			actions.appendChild(edit);
			body.appendChild(row);
		}
	}

	async function show(item) {
		const data = await call("/api/reveal", { id: item.id });
		if (!data) {
			return;
		}
		nameField.textContent = item.name;
		value.textContent = data.secret;
		dialog.showModal();
	}

	function hide() {
		value.textContent = "";
		nameField.textContent = "";
		if (dialog.open) {
			dialog.close();
		}
	}

	document.getElementById("secret-close").addEventListener("click", hide);
	dialog.addEventListener("close", () => {
		value.textContent = "";
	});
	document.getElementById("secret-copy").addEventListener("click", async () => {
		try {
			await navigator.clipboard.writeText(value.textContent);
		} catch (err) {
			return;
		}
		// Zwischenablage nach kurzer Zeit leeren.
		window.clearTimeout(wipeTimer);
		wipeTimer = window.setTimeout(() => {
			navigator.clipboard.writeText("").catch(() => {});
		}, wipeMs);
	});

	const search = document.getElementById("search");
	search.addEventListener("submit", (event) => {
		event.preventDefault();
		load(document.getElementById("q").value);
	});
	load("");
}

// Eintrag anlegen oder aendern
function setupItem() {
	const form = document.getElementById("entry");
	if (!form) {
		return;
	}
	const id = Number(new URLSearchParams(window.location.search).get("id") || 0);
	document.getElementById("id").value = String(id);
	if (id > 0) {
		call("/api/items").then((items) => {
			if (!items) {
				return;
			}
			const found = items.find((item) => item.id === id);
			if (!found) {
				return;
			}
			document.getElementById("type").value = String(found.type);
			document.getElementById("name").value = found.name;
			document.getElementById("site").value = found.site;
			document.getElementById("profile").value = String(found.profile);
			document.getElementById("mirrored").checked = found.mirrored;
			document.getElementById("pairedHostOnly").checked = found.pairedHostOnly;
		});
	}
	form.addEventListener("submit", async (event) => {
		event.preventDefault();
		const secretField = document.getElementById("secret");
		const body = {
			id: Number(document.getElementById("id").value),
			type: Number(document.getElementById("type").value),
			profile: Number(document.getElementById("profile").value),
			name: document.getElementById("name").value,
			site: document.getElementById("site").value,
			mirrored: document.getElementById("mirrored").checked,
			pairedHostOnly: document.getElementById("pairedHostOnly").checked,
			secret: secretField.value,
		};
		const data = await call("/api/save", body);
		// Das Geheimnis verlaesst das Feld, sobald es geschickt ist.
		secretField.value = "";
		body.secret = "";
		if (data) {
			window.location.href = "/";
		}
	});
}

// Zertifikate und Nachweis
function setupCerts() {
	const table = document.getElementById("certs");
	if (!table) {
		return;
	}
	const body = table.querySelector("tbody");
	const empty = document.getElementById("certs-empty");
	call("/api/items").then((items) => {
		body.textContent = "";
		const certs = (items || []).filter((item) => item.type === 5);
		if (certs.length === 0) {
			empty.hidden = false;
			table.hidden = true;
			return;
		}
		empty.hidden = true;
		for (const item of certs) {
			const row = document.createElement("tr");
			cell(row, item.name);
			cell(row, item.site);
			cell(row, dateText(item.changed));
			body.appendChild(row);
		}
	});
	document.getElementById("attest").addEventListener("click", async () => {
		const random = new Uint8Array(32);
		window.crypto.getRandomValues(random);
		const challenge = btoa(String.fromCharCode.apply(null, Array.from(random)));
		const data = await call("/api/attest", { challenge: challenge });
		document.getElementById("attest-result").textContent =
			data ? data.signature.slice(0, 32) : "";
	});
}

// Geraete, Kopplung, Protokoll
function setupDevices() {
	const list = document.getElementById("devices");
	if (!list) {
		return;
	}
	const empty = document.getElementById("devices-empty");
	async function load() {
		const devices = await call("/api/devices");
		list.textContent = "";
		if (!devices || devices.length === 0) {
			empty.hidden = false;
			return;
		}
		empty.hidden = true;
		for (const device of devices) {
			const entry = document.createElement("li");
			const button = document.createElement("button");
			button.type = "button";
			button.textContent = device.label +
				(device.simulated ? " (" + t("devices.simulated") + ")" : "");
			button.addEventListener("click", async () => {
				if (await call("/api/use", { path: device.path })) {
					refreshState();
				}
			});
			entry.appendChild(button);
			list.appendChild(entry);
		}
	}
	load();

	document.getElementById("unlock").addEventListener("submit", async (event) => {
		event.preventDefault();
		const field = document.getElementById("pin");
		const pin = field.value;
		field.value = "";
		if (await call("/api/unlock", { pin: pin })) {
			refreshState();
		}
	});
	for (const button of document.querySelectorAll("button.pair")) {
		button.addEventListener("click", async () => {
			const data = await call("/api/pair", { kind: button.dataset.kind });
			document.getElementById("pair-result").textContent =
				data ? data.number : "";
		});
	}
	const logTable = document.getElementById("log");
	document.getElementById("log-load").addEventListener("click", async () => {
		const entries = await call("/api/log?n=100");
		const body = logTable.querySelector("tbody");
		body.textContent = "";
		for (const entry of entries || []) {
			const row = document.createElement("tr");
			cell(row, dateText(entry.time));
			cell(row, entry.event);
			cell(row, entry.askedBy);
			body.appendChild(row);
		}
	});
}

// Sicherung speichern und laden
function setupBackup() {
	const save = document.getElementById("backup-save");
	if (!save) {
		return;
	}
	save.addEventListener("click", async () => {
		const data = await call("/api/backup", {});
		if (!data) {
			return;
		}
		saveBackup(data.blob);
	});
	document.getElementById("restore").addEventListener("submit", async (event) => {
		event.preventDefault();
		const file = document.getElementById("blob").files[0];
		if (!file) {
			return;
		}
		const blob = await fileToBase64(file);
		await call("/api/restore", { blob: blob });
	});
}

// saveBackup legt die verschluesselte Sicherung als Datei ab. Der Assistent
// und die Sicherungsseite nutzen denselben Weg - zwei Nachbauten wuerden
// auseinanderlaufen.
function saveBackup(blob) {
	const raw = atob(blob);
	const bytes = new Uint8Array(raw.length);
	for (let i = 0; i < raw.length; i += 1) {
		bytes[i] = raw.charCodeAt(i);
	}
	const url = URL.createObjectURL(new Blob([bytes],
		{ type: "application/octet-stream" }));
	const link = document.createElement("a");
	link.href = url;
	link.download = "opensecurekey.oskbak";
	link.click();
	URL.revokeObjectURL(url);
}

// Einrichtungsassistent: vier Schritte in einer Seite. Der Ablauf ist fest -
// zuerst die Betriebsart, dann die PIN, dann die Sicherung, dann der Zettel.
// Vor- und zuruecksetzen geht nur ueber diese Funktion, damit kein Schritt
// uebersprungen werden kann.
function setupWizard() {
	const first = document.getElementById("step-mode");
	if (!first) {
		return;
	}
	const order = ["mode", "pin", "backup", "sheet"];
	// Die Wahl steht nur hier, nicht in der Adresse und nicht in einer
	// Ablage des Browsers: eine unumkehrbare Wahl soll kein Lesezeichen
	// mitbringen koennen.
	let chosenMode = null;

	function show(name) {
		for (const step of order) {
			const section = document.getElementById("step-" + step);
			const tab = document.getElementById("tab-" + step);
			const current = step === name;
			section.hidden = !current;
			if (current) {
				tab.setAttribute("aria-current", "step");
			} else {
				tab.removeAttribute("aria-current");
			}
		}
		// Der Fokus wandert auf die Ueberschrift des Schrittes - sonst
		// steht ein Vorleseprogramm nach dem Weiter noch oben am Anfang.
		const head = document.getElementById("head-" + name);
		if (head) {
			head.focus();
		}
	}

	// Erster Schritt: kein Knopf, solange nicht gewaehlt UND bestaetigt ist.
	const modeForm = document.getElementById("mode-form");
	const confirm = document.getElementById("mode-confirm");
	const modeNext = document.getElementById("mode-next");
	// Die Betriebsart ist eine Zahl, und "noch nichts gewaehlt" ist hier
	// null. Die Zahlen auf der Leitung fangen bei 1 an, damit eine
	// vergessene Angabe nicht still zu einer Betriebsart wird.
	function readMode() {
		const picked = modeForm.querySelector("input[name=mode]:checked");
		return picked ? Number(picked.value) : null;
	}
	function checkMode() {
		modeNext.disabled = !(readMode() !== null && confirm.checked);
	}
	modeForm.addEventListener("change", checkMode);
	checkMode();
	modeForm.addEventListener("submit", (event) => {
		event.preventDefault();
		const mode = readMode();
		if (mode === null) {
			showProblem(t("error.setup.mode"));
			return;
		}
		if (!confirm.checked) {
			showProblem(t("error.setup.confirm"));
			return;
		}
		showProblem("");
		chosenMode = mode;
		const label = document.getElementById("pin-mode");
		label.textContent = t("setup.mode.chosen") + ": " +
			t(mode === 2 ? "setup.only.title" : "setup.mirrored.title");
		show("pin");
	});

	// Zweiter Schritt: die PIN. Erst hier geht die Wahl zum Geraet - mit
	// ihr entsteht das Hauptgeheimnis.
	document.getElementById("pin-form").addEventListener("submit", async (event) => {
		event.preventDefault();
		const field = document.getElementById("pin-new");
		const again = document.getElementById("pin-again");
		if (field.value.length < 6) {
			showProblem(t("error.setup.pin.short"));
			return;
		}
		if (field.value !== again.value) {
			showProblem(t("error.setup.pin.mismatch"));
			return;
		}
		const data = await call("/api/setup", {
			mode: chosenMode,
			pin: field.value,
			confirm: true,
		});
		// Die Eingabefelder werden geleert, ob es geklappt hat oder nicht.
		field.value = "";
		again.value = "";
		if (!data) {
			return;
		}
		refreshState();
		show("backup");
	});

	// Dritter Schritt: die Sicherung. Weiter geht es erst, wenn der
	// Anwender sie geholt und bestaetigt hat.
	const saved = document.getElementById("backup-done");
	const backupNext = document.getElementById("backup-next");
	document.getElementById("backup-now").addEventListener("click", async () => {
		const data = await call("/api/backup", {});
		if (!data) {
			return;
		}
		saveBackup(data.blob);
	});
	saved.addEventListener("change", () => {
		backupNext.disabled = !saved.checked;
	});
	document.getElementById("backup-form").addEventListener("submit", (event) => {
		event.preventDefault();
		if (!saved.checked) {
			return;
		}
		show("sheet");
	});

	show("mode");
}

// Der Notfallzettel wird gedruckt, nicht gespeichert.
function setupSheet() {
	const button = document.getElementById("print");
	if (!button) {
		return;
	}
	button.addEventListener("click", () => {
		window.print();
	});
}

// Firmware aufspielen
function setupFirmware() {
	const form = document.getElementById("firmware");
	if (!form) {
		return;
	}
	form.addEventListener("submit", async (event) => {
		event.preventDefault();
		const file = document.getElementById("image").files[0];
		const state = document.getElementById("firmware-state");
		if (!file) {
			return;
		}
		state.textContent = t("state.working");
		const image = await fileToBase64(file);
		const data = await call("/api/firmware", { image: image });
		state.textContent = data ? String(data.sent) : "";
	});
}

// Einstellungen
function setupSettings() {
	const form = document.getElementById("settings");
	if (!form) {
		return;
	}
	form.addEventListener("submit", async (event) => {
		event.preventDefault();
		await call("/api/settings", {
			lang: document.getElementById("lang").value,
			appName: document.getElementById("appName").value,
			port: Number(document.getElementById("port").value),
			simAddress: document.getElementById("simAddress").value,
		});
		window.location.reload();
	});
}

async function fileToBase64(file) {
	const buffer = new Uint8Array(await file.arrayBuffer());
	let text = "";
	for (let i = 0; i < buffer.length; i += 1) {
		text += String.fromCharCode(buffer[i]);
	}
	return btoa(text);
}

const lock = document.getElementById("lock");
if (lock) {
	lock.addEventListener("click", async () => {
		await call("/api/lock", {});
		refreshState();
	});
}

// Hinein und hinaus: CXF, Tabellen, KeePass, Leak-Pruefung.
//
// DIE ANZEIGE, DIE SAGT WOHIN, steht hier - aber sie ist nicht die
// Bedingung. Die sitzt in CxfExport auf der anderen Seite: wer diese Seite
// umgeht und /api/export selbst aufruft, bekommt ohne Ziel und ohne
// Zustimmung eine Absage. Diese Zeilen sorgen nur dafuer, dass der Anwender
// das Ziel LIEST, bevor er zustimmt.
function setupExchange() {
	const exportForm = document.getElementById("export");
	if (!exportForm) {
		return;
	}
	const where = document.getElementById("export-where");

	exportForm.addEventListener("submit", async (event) => {
		event.preventDefault();
		const to = document.getElementById("export-to").value.trim();
		if (!to) {
			showProblem(t("error.cxf.nodestination"));
			return;
		}
		// Das Ziel wird gezeigt, und erst die Bestaetigung schickt
		// confirm: true. Ein Haken, der vorher gesetzt wird, waere kein
		// Lesen.
		where.textContent = t("cxf.export.where").replace("%s", to);
		where.hidden = false;
		const frage = where.textContent + "\n\n" + t("cxf.export.warning");
		if (!window.confirm(frage)) {
			showProblem(t("cxf.export.cancelled"));
			return;
		}
		const data = await call("/api/export", { destination: to, confirm: true });
		if (!data) {
			return;
		}
		showReport(data.report);
		saveTextFile(data.file, "opensecurekey-cxf.json");
	});

	const importForm = document.getElementById("import");
	const formatField = document.getElementById("import-format");
	const passwordRow = document.getElementById("import-password-row");
	const report = document.getElementById("import-report");
	const lines = document.getElementById("import-lines");
	let pending = null;

	// Das Kennwortfeld gibt es nur fuer KeePass. Es immer zu zeigen
	// hiesse, nach einem Kennwort zu fragen, das niemand braucht.
	const syncFormat = () => {
		passwordRow.hidden = formatField.value !== "keepass";
	};
	formatField.addEventListener("change", syncFormat);
	syncFormat();

	importForm.addEventListener("submit", async (event) => {
		event.preventDefault();
		const file = document.getElementById("import-file").files[0];
		if (!file) {
			return;
		}
		pending = {
			format: formatField.value,
			data: await fileToBase64(file),
			password: document.getElementById("import-password").value,
			apply: false,
		};
		const data = await call("/api/import", pending);
		if (!data) {
			return;
		}
		// ERST der Bericht, DANN der Knopf. Was nicht mitkommt, steht
		// vor der Entscheidung und nicht hinter ihr.
		showImport(lines, data);
		report.hidden = false;
	});

	document.getElementById("import-apply").addEventListener("click", async () => {
		if (!pending) {
			return;
		}
		pending.apply = true;
		const data = await call("/api/import", pending);
		pending = null;
		document.getElementById("import-password").value = "";
		if (!data) {
			return;
		}
		showImport(lines, data);
		if (data.report) {
			showReport(data.report);
		}
	});

	const leaksForm = document.getElementById("leaks");
	const leaksLines = document.getElementById("leaks-lines");
	leaksForm.addEventListener("submit", async (event) => {
		event.preventDefault();
		const consent = document.getElementById("leaks-consent").checked;
		if (!consent) {
			showProblem(t("error.leak.noconsent"));
			return;
		}
		const data = await call("/api/leaks", { consent: true, all: true });
		if (!data) {
			return;
		}
		leaksLines.textContent = "";
		(data.findings || []).forEach((finding) => {
			const row = document.createElement("li");
			if (finding.reason) {
				row.textContent = finding.name + " - " + t(finding.reason);
			} else if (finding.count > 0) {
				row.textContent = finding.name + " - " +
					t("leak.found").replace("%d", finding.count);
			} else {
				row.textContent = finding.name + " - " + t("leak.clean");
			}
			leaksLines.appendChild(row);
		});
	});
}

// showImport zeigt, was hineinkommt und was nicht. Die Liste der
// zurueckgebliebenen Zeilen steht ZULETZT: was man zuletzt liest, bleibt
// haengen.
function showImport(lines, data) {
	lines.textContent = "";
	const add = (text) => {
		const row = document.createElement("li");
		row.textContent = text;
		lines.appendChild(row);
	};
	add(t("cxf.import.from").replace("%s", data.from || ""));
	(data.warnings || []).forEach((line) => add("! " + t(line.reason)));
	add(t("cxf.import.ask").replace("%d", data.count || 0));
	if ((data.left || []).length > 0) {
		add(t("cxf.report.leftheader"));
		data.left.forEach((line) => {
			add("- " + (line.name || "") + " " + t(line.reason));
		});
	}
}

// showReport zeigt den Bericht eines Exports oder eines Anlegens. Was nicht
// mitgekommen ist, steht darin - eine Sicherung, die stillschweigend die
// Haelfte auslaesst, ist schlimmer als keine.
function showReport(report) {
	if (!report) {
		return;
	}
	const teile = [t(reportKey(report))];
	(report.flattened || []).forEach((line) => {
		teile.push("~ " + (line.name || "") + " " + t(line.reason));
	});
	if ((report.left || []).length > 0) {
		teile.push(t("cxf.report.leftheader"));
		(report.left || []).forEach((line) => {
			teile.push("- " + (line.name || "") + " " + t(line.reason));
		});
	}
	showProblem(teile.join("\n"));
}

function reportKey(report) {
	if ((report.left || []).length > 0) {
		return "cxf.report.incomplete";
	}
	if ((report.flattened || []).length > 0) {
		return "cxf.report.flattened";
	}
	return "cxf.report.complete";
}

// saveTextFile legt einen Text als Datei ab. Derselbe Weg wie saveBackup -
// nur ohne das Umrechnen aus Base64, denn CXF ist schon Text.
function saveTextFile(text, name) {
	const url = URL.createObjectURL(new Blob([text],
		{ type: "application/json" }));
	const link = document.createElement("a");
	link.href = url;
	link.download = name;
	link.click();
	URL.revokeObjectURL(url);
}

// Erst die Texte, dann die Seiten - sonst stehen Knoepfe ohne Beschriftung da.
async function init() {
	texts = (await call("/api/texts")) || {};
	typeNames = (await call("/api/types")) || {};
	refreshState();
	setupOverview();
	setupItem();
	setupCerts();
	setupDevices();
	setupBackup();
	setupExchange();
	setupFirmware();
	setupSettings();
	setupWizard();
	setupSheet();
}

init();
