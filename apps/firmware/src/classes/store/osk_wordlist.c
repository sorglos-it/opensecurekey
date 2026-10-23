/* Woerterliste fuer den Vorschlag eines Hauptkennworts.
 *
 * 256 kurze deutsche Woerter, also genau 8 Bit je Wort. Acht Woerter ergeben
 * 64 Bit - das ist die Staerke, auf die sich die Sicherungsdatei verlaesst.
 *
 * Die Liste ist selbst geschrieben, nicht uebernommen. Regeln dafuer:
 *  - nur Kleinbuchstaben a-z, keine Umlaute, kein Scharf-S: so laesst sich ein
 *    Kennwort auf jeder Tastatur der Welt eintippen
 *  - 3 bis 7 Buchstaben, leicht zu lesen und abzuschreiben
 *  - kein Wort ist der Anfang eines anderen, damit ein abgeschnittenes Wort
 *    nicht als ein anderes gelesen wird (im Test geprueft)
 *  - keine Woerter, die jemanden kraenken oder erschrecken */

#include "osk_backup.h"

const char *const osk_wordlist[OSK_WORDLIST_LEN] = {
    "abend",   "acker",	 "adler",   "affe",   "ahorn",	 "akte",   "alarm",   "algen",	 "allee",
    "alpen",   "altar",	 "ameise",  "amsel",  "anker",	 "apfel",  "april",   "arbeit",	 "archiv",
    "arena",   "armee",	 "arzt",    "asche",  "atem",	 "atlas",  "auge",    "auto",	 "bach",
    "backen",  "bahn",	 "balkon",  "band",   "bank",	 "bart",   "bauer",   "beere",	 "beet",
    "berg",    "besen",	 "beton",   "biene",  "bier",	 "bild",   "birke",   "blatt",	 "blei",
    "blick",   "blume",	 "boden",   "bogen",  "bohne",	 "boot",   "brief",   "brot",	 "bruch",
    "buch",    "bluse",	 "burg",    "busch",  "dach",	 "damm",   "dampf",   "datum",	 "deich",
    "dichte",  "diele",	 "ding",    "docht",  "dohle",	 "dorf",   "dose",    "draht",	 "drama",
    "druck",   "duene",	 "duft",    "ebene",  "ecke",	 "edel",   "egge",    "eiche",	 "eimer",
    "eisen",   "elch",	 "elfe",    "engel",  "enkel",	 "ernte",  "erbse",   "esche",	 "essig",
    "etage",   "eule",	 "fabrik",  "faden",  "fahne",	 "falke",  "farbe",   "fasan",	 "feder",
    "feige",   "feld",	 "fenster", "ferne",  "fest",	 "feuer",  "fichte",  "figur",	 "film",
    "finger",  "firma",	 "fisch",   "flagge", "flasche", "fleck",  "flora",   "floss",	 "flug",
    "fluss",   "forst",	 "forum",   "frage",  "frost",	 "frucht", "fuchs",   "funke",	 "gabel",
    "galerie", "gans",	 "garten",  "gasse",  "gebiet",	 "geige",  "gelee",   "gemuese", "genau",
    "gerste",  "gipfel", "glas",    "globus", "gnom",	 "gold",   "granit",  "gras",	 "grill",
    "grotte",  "gruppe", "gurke",   "hafen",  "hagel",	 "hahn",   "halle",   "hammer",	 "hand",
    "harfe",   "hase",	 "haus",    "hecke",  "heide",	 "helm",   "herbst",  "hirsch",	 "hobel",
    "hocker",  "hoehe",	 "holz",    "honig",  "hotel",	 "huegel", "huhn",    "hund",	 "hymne",
    "idee",    "igel",	 "insel",   "jacke",  "jagd",	 "jahr",   "jubel",   "kabel",	 "kaefer",
    "kahn",    "kajak",	 "kakao",   "kalb",   "kamel",	 "kamin",  "kanal",   "kante",	 "kappe",
    "karte",   "kasten", "katze",   "kegel",  "kelle",	 "kerze",  "kette",   "kiefer",	 "kiesel",
    "kino",    "kiste",	 "klang",   "klee",   "klinge",	 "knopf",  "koffer",  "kohle",	 "kolben",
    "kompass", "konto",	 "kopf",    "korb",   "korn",	 "kraft",  "kran",    "kreide",	 "kreis",
    "kritik",  "krone",	 "kuchen",  "kugel",  "kuh",	 "kunst",  "kupfer",  "kurve",	 "lampe",
    "land",    "laub",	 "lawine",  "leder",  "legende", "lehm",   "leiter",  "lerche",	 "licht",
    "lilie",   "linde",	 "linie",   "lippe",  "liste",	 "lob",	   "loeffel", "los",	 "luchs",
    "luft",    "lupe",	 "magnet",  "mandel", "mango",	 "mantel", "markt",   "marsch",	 "maske",
    "mast",    "matte",	 "mauer",   "maus",
};
