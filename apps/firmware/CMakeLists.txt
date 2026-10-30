# Zephyr-Anwendung der Firmware. Gebaut wird sie ueber tools/build.py,
# nicht von Hand - dort kommt auch OSK_VERSION herein (siehe docs/build.md).
cmake_minimum_required(VERSION 3.20.0)

# --- Versionen: zwei Dateien, ein Name -----------------------------------
#
# Zephyr liest von sich aus eine Datei namens VERSION im Anwendungsordner
# und erwartet darin SEIN Format (VERSION_MAJOR = 0 ...). Unsere
# apps/firmware/VERSION ist die des Projekts ("0.0.1.0", siehe version.md)
# und gehoert tools/set_version.py - sie wird nicht umgeschrieben.
#
# Also schreiben wir Zephyr seine Fassung in den Bauordner und nennen ihm
# beide Dateien ausdruecklich. Beide Listen muessen gleich lang sein:
# Zephyr laeuft sie paarweise ab, und ein Paar ohne Partner laeuft in die
# Meldung "VERSION_MAJOR must be present" - mit leerer Klammer dahinter.
file(TO_CMAKE_PATH "$ENV{ZEPHYR_BASE}" OSK_ZEPHYR_BASE)

# Die Version wird in BEIDEN Schreibweisen gelesen: "0.0.1.0" (so steht
# sie heute da, siehe version.md) und Zephyrs "VERSION_MAJOR = 0 ...".
# Grund: Zephyr liest dieselbe Datei auch als Kconfig-Datei
# (Kconfig.zephyr, osource auf den Anwendungsordner - fest verdrahtet,
# kein Schalter davor). Welche Schreibweise im Repo gilt, entscheidet das
# Projekt; der Bau laeuft mit beiden.
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION OSK_VERSION_RAW)
string(STRIP "${OSK_VERSION_RAW}" OSK_VERSION_RAW)

if(OSK_VERSION_RAW MATCHES "^([0-9]+)\.([0-9]+)\.([0-9]+)\.([0-9]+)$")
  set(OSK_V_MAJOR ${CMAKE_MATCH_1})
  set(OSK_V_MINOR ${CMAKE_MATCH_2})
  set(OSK_V_PATCH ${CMAKE_MATCH_3})
  set(OSK_V_TWEAK ${CMAKE_MATCH_4})
elseif(OSK_VERSION_RAW MATCHES "VERSION_MAJOR[ 	]*=[ 	]*([0-9]+)")
  set(OSK_V_MAJOR ${CMAKE_MATCH_1})
  string(REGEX MATCH "VERSION_MINOR[ 	]*=[ 	]*([0-9]+)" _ "${OSK_VERSION_RAW}")
  set(OSK_V_MINOR ${CMAKE_MATCH_1})
  string(REGEX MATCH "PATCHLEVEL[ 	]*=[ 	]*([0-9]+)" _ "${OSK_VERSION_RAW}")
  set(OSK_V_PATCH ${CMAKE_MATCH_1})
  string(REGEX MATCH "VERSION_TWEAK[ 	]*=[ 	]*([0-9]+)" _ "${OSK_VERSION_RAW}")
  set(OSK_V_TWEAK ${CMAKE_MATCH_1})
else()
  message(FATAL_ERROR "apps/firmware/VERSION ist in keiner der beiden Schreibweisen lesbar: '${OSK_VERSION_RAW}'")
endif()
set(OSK_VERSION_RAW "${OSK_V_MAJOR}.${OSK_V_MINOR}.${OSK_V_PATCH}.${OSK_V_TWEAK}")

set(OSK_APP_VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/zephyr_app_version)
file(WRITE ${OSK_APP_VERSION_FILE}
     "VERSION_MAJOR = ${OSK_V_MAJOR}\nVERSION_MINOR = ${OSK_V_MINOR}\n"
     "PATCHLEVEL = ${OSK_V_PATCH}\nVERSION_TWEAK = ${OSK_V_TWEAK}\nEXTRAVERSION =\n")

# Warum CACHE, und nicht einfach set(): Das SDK leiht sich diese beiden
# Listen fuer seine EIGENE Version (NcsConfig.cmake liest nrf/VERSION) und
# raeumt sie danach mit set(VERSION_TYPE) wieder weg. Ein gewoehnliches
# set() von uns ist an dieser Stelle schon geloescht; Zephyr findet die
# Listen leer vor, leitet sie selbst her - und landet wieder bei unserer
# apps/firmware/VERSION. Ein Eintrag im Zwischenspeicher ueberlebt das
# Wegraeumen, weil set() ohne Wert nur die gewoehnliche Fassung entfernt.
# Gemessen am 24.10.2026 mit variable_watch.
set(VERSION_TYPE "KERNEL;APP" CACHE INTERNAL "Versionsarten fuer Zephyr")
set(VERSION_FILE "${OSK_ZEPHYR_BASE}/VERSION;${OSK_APP_VERSION_FILE}" CACHE INTERNAL "Versionsdateien fuer Zephyr")

# Die Weiche prueft sich selbst, statt in die Meldung von oben zu laufen.
message(STATUS "osk: VERSION_TYPE=${VERSION_TYPE}")
message(STATUS "osk: VERSION_FILE=${VERSION_FILE}")
foreach(f IN LISTS VERSION_FILE)
  if(NOT EXISTS ${f})
    message(FATAL_ERROR "Versionsdatei fehlt: ${f} (ZEPHYR_BASE=$ENV{ZEPHYR_BASE})")
  endif()
endforeach()

# --- Signierter Bootloader, zwei Plaetze, zwei Fassungen -------------------
#
# Die drei Zeilen, die apps/firmware/boot/app_update.conf und
# boot/mcuboot.conf beim Bau-Strang ANGEFORDERT haben, stehen hier. Sie
# muessen VOR find_package(Zephyr) stehen: danach ist die Aufteilung der
# Partitionen schon gelesen und ein Kind-Abbild schon eingerichtet.
#
# UNGEPRUEFT: Auf diesem Rechner ist kein nRF Connect SDK. Dieser Block ist
# geschrieben, nicht uebersetzt - siehe docs/signatur.md, "Was geprueft ist
# und was nicht".

# (1) Die feste Aufteilung der Speicher. Ohne sie legt das SDK den zweiten
#     Platz selbst ins INTERNE Flash - dann passt er nicht, und der Tresor
#     waere in Gefahr (boot/pm_static.yml sagt, warum die Reihenfolge nicht
#     verhandelbar ist).
set(PM_STATIC_YML_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boot/pm_static.yml
    CACHE FILEPATH "Feste Aufteilung der Speicher" FORCE)

# (2) Die Einstellungen von MCUboot als Kind-Abbild.
#     Der Name dieser Variablen hat sich zwischen den SDK-Fassungen geaendert
#     (mcuboot_EXTRA_CONF_FILE, frueher mcuboot_OVERLAY_CONFIG). Es werden
#     beide gesetzt: die Fassung, die das SDK nicht kennt, bleibt einfach
#     liegen - eine unbekannte CMake-Variable ist kein Fehler. Verlassen darf
#     man sich darauf nicht, darum sagt der erste echte Bau, welche greift.
set(OSK_MCUBOOT_CONF ${CMAKE_CURRENT_SOURCE_DIR}/boot/mcuboot.conf)
set(mcuboot_EXTRA_CONF_FILE ${OSK_MCUBOOT_CONF} CACHE STRING "MCUboot-Einstellungen" FORCE)
set(mcuboot_OVERLAY_CONFIG ${OSK_MCUBOOT_CONF} CACHE STRING "MCUboot-Einstellungen (alt)" FORCE)

# (3) Der Pfad des MCUboot-Schluessels - von AUSSEN, nie aus dem Repo.
#
# DAS IST DIE ZEILE, DEREN FEHLEN DAS ENDE DER GANZEN ABSICHERUNG WAERE:
# Ohne CONFIG_BOOT_SIGNATURE_KEY_FILE nimmt MCUboot seinen EIGENEN
# Beispielschluessel, und der ist oeffentlich bekannt. Ein Geraet, das damit
# gebaut ist, nimmt Firmware von jedem an, der die Datei aus dem Netz laedt.
# Es gibt dafuer keine Warnung - der Bau wird gruen.
#
# Darum: kein Schluessel, kein Bau. Lieber kein Abbild als ein Abbild, das
# so aussieht wie ein unterschriebenes.
if(NOT DEFINED OSK_MCUBOOT_KEY)
  set(OSK_MCUBOOT_KEY "$ENV{OSK_MCUBOOT_KEY}")
endif()
if(OSK_MESSBAU)
  # Der Messbau ist ausdruecklich kein Abbild fuer ein Geraet (siehe weiter
  # unten: sechs Platzhalter sind dabei). Er darf darum ohne Schluessel
  # laufen - und sagt es auch.
  message(WARNING "osk: MESSBAU ohne Signaturschluessel - das Ergebnis ist eine Groesse, kein Abbild")
elseif(NOT OSK_MCUBOOT_KEY)
  message(FATAL_ERROR
    "Kein MCUboot-Signaturschluessel.\n"
    "Ohne ihn baut MCUboot mit seinem eigenen BEISPIELSCHLUESSEL, und der ist\n"
    "oeffentlich bekannt: das Geraet nimmt dann Firmware von jedem an. Der Bau\n"
    "bricht hier ab, weil er sonst gruen wuerde.\n\n"
    "Setzen (PowerShell, Pfad AUSSERHALB des Projektordners):\n"
    "  $env:OSK_MCUBOOT_KEY = \"$env:USERPROFILE\\.osk-keys\\mcuboot-ed25519.pem\"\n\n"
    "Siehe docs/signatur.md.")
else()
  get_filename_component(OSK_MCUBOOT_KEY "${OSK_MCUBOOT_KEY}" ABSOLUTE)
  if(NOT EXISTS "${OSK_MCUBOOT_KEY}")
    message(FATAL_ERROR "Der MCUboot-Schluessel steht in OSK_MCUBOOT_KEY, liegt aber nicht da: "
                        "${OSK_MCUBOOT_KEY}")
  endif()
  # Dieselbe Regel wie in tools/sign.py: nicht im Projektordner, auch nicht
  # ausdruecklich. Ein Schluessel, der einmal in einem Commit steckt, ist aus
  # der Geschichte nicht mehr herauszuholen.
  get_filename_component(OSK_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
  string(FIND "${OSK_MCUBOOT_KEY}" "${OSK_REPO_ROOT}" OSK_KEY_INSIDE)
  if(OSK_KEY_INSIDE EQUAL 0)
    message(FATAL_ERROR
      "Der MCUboot-Schluessel liegt IM Projektordner: ${OSK_MCUBOOT_KEY}\n"
      "Er gehoert ausserhalb. Verschieben, nicht nur in .gitignore eintragen -\n"
      "ignoriert ist nicht weg (tools/keyguard.py sieht beides).")
  endif()
  set(mcuboot_CONFIG_BOOT_SIGNATURE_KEY_FILE "\"${OSK_MCUBOOT_KEY}\""
      CACHE STRING "MCUboot-Signaturschluessel" FORCE)
  # Der Pfad selbst wird NICHT protokolliert: Bauprotokolle wandern in
  # Fehlerberichte, und der Pfad verraet, wo der Schluessel liegt.
  message(STATUS "osk: MCUboot-Schluessel von aussen uebernommen")
endif()

# Die Fassung (mit oder ohne RSA) gehoert in den ABBILDKOPF, nicht nur in den
# Dateinamen: ein Dateiname ist umbenannt, bevor man zweimal hinsieht. Der
# AAGUID wandert als GESCHUETZTES TLV in den MCUboot-Kopf - geschuetzt heisst,
# von der Signatur gedeckt, also nicht austauschbar, ohne die Signatur zu
# brechen. Welcher Wert gilt, entscheidet nicht diese Datei: tools/variant.py
# liest ihn aus osk_ctap_core.c, wo ihn auch der Quelltext liest. Geprueft
# wird das fertige Abbild mit
#   python tools/variant.py --check <abbild> --expect <rsa|norsa>
if(OSK_AAGUID_TLV)
  set(mcuboot_CONFIG_MCUBOOT_EXTRA_IMGTOOL_ARGS
      "--custom-tlv 0x00a1 ${OSK_AAGUID_TLV}"
      CACHE STRING "AAGUID im Abbildkopf" FORCE)
  message(STATUS "osk: AAGUID als geschuetztes TLV 0x00a1 im Abbildkopf")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(opensecurekey C)

# Version: build.py gibt sie als -DOSK_VERSION mit; ohne das steht hier,
# was in apps/firmware/VERSION steht, damit ein Bau von Hand nicht luegt.
if(NOT OSK_VERSION)
  set(OSK_VERSION ${OSK_VERSION_RAW})
endif()

# OSK_TARGET_DEVICE schaltet in den Quellen den Geraeteweg frei,
# OSK_CRYPTO_PSA den Krypto-Unterbau ueber PSA (statt CNG oder OpenSSL
# aus dem Pruefbau am PC).
target_compile_definitions(app PRIVATE
  OSK_TARGET_DEVICE
  OSK_CRYPTO_PSA
  OSK_VERSION="${OSK_VERSION}"
)

# Suchpfade aus dem Baum, nicht aus einer Liste von Hand: jeder Ordner
# unter src/, in dem ein Kopf liegt, kommt hinein. Eine Liste von Hand
# vergisst den naechsten Unterordner (genau wie das Dateimuster weiter
# unten), und das faellt erst am Uebersetzer auf.
file(GLOB_RECURSE OSK_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
set(OSK_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
foreach(h IN LISTS OSK_HEADERS)
  get_filename_component(d "${h}" DIRECTORY)
  list(APPEND OSK_INCLUDE_DIRS "${d}")
endforeach()
list(REMOVE_DUPLICATES OSK_INCLUDE_DIRS)
list(LENGTH OSK_INCLUDE_DIRS OSK_INCLUDE_COUNT)
message(STATUS "osk: ${OSK_INCLUDE_COUNT} Suchpfade")

target_include_directories(app PRIVATE ${OSK_INCLUDE_DIRS})

# Alle sieben Teile. Die plattformabhaengigen Dateien tragen eigene
# Weichen (#ifdef OSK_TARGET_DEVICE, _WIN32, __ZEPHYR__) und uebersetzen
# sich hier zu nichts, wenn sie nicht gemeint sind - darum wird nichts
# von Hand ausgesiebt.
# GLOB_RECURSE, nicht GLOB: Teile legen Unterordner an (etwa
# src/classes/applets/guard/). Eine Ebene allein uebersieht die - und das
# faellt nicht auf, denn der Bau wird gruen und das Abbild nur zu klein.
# Genau dann luegt die gemessene Groesse.
file(GLOB_RECURSE OSK_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/classes/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/src/views/*.c
)
# Gezaehlt wird, was WIRKLICH uebersetzt wird - also samt der beiden
# Dateien direkt unter src/, die das Muster nicht fasst. Eine Zahl, die
# kleiner ist als der Bau, waere genau die irreführende Angabe, die eine
# zu kleine Groesse glaubwuerdig macht.
set(OSK_ALL_SOURCES src/main.c src/osk_mem.c ${OSK_SOURCES})
list(LENGTH OSK_ALL_SOURCES OSK_SOURCE_COUNT)
message(STATUS "osk: ${OSK_SOURCE_COUNT} Quelldateien")
# src/main.c und src/osk_mem.c liegen direkt unter src/ und damit
# ausserhalb des Musters oben - darum stehen sie in OSK_ALL_SOURCES
# ausdruecklich. osk_mem.c gehoert keinem Teil, sondern allen
# (include/osk_mem.h sagt, warum).
target_sources(app PRIVATE ${OSK_ALL_SOURCES})

# Messbau: nur mit -DOSK_MESSBAU=y. Dann kommen sechs Platzhalter dazu,
# damit der Binder durchkommt und eine Groesse herauskommt. Ohne den
# Schalter uebersetzt sich src/messbau.c zu nichts, und das Binden bricht
# an den sechs fehlenden Namen ab - so, wie es soll (src/messbau.c sagt,
# warum). Ein gewoehnlicher Bau meldet die Luecken also weiter.
if(OSK_MESSBAU)
  message(WARNING "osk: MESSBAU - sechs Platzhalter sind dabei, das Abbild gehoert in kein Geraet")
  target_compile_definitions(app PRIVATE OSK_MESSBAU)
  target_sources(app PRIVATE src/messbau.c)

  # Zwei Namen sind doppelt vergeben (osk_wipe, osk_apdu_parse - siehe
  # docs/build.md). Fuer die MESSUNG laesst der Binder das durchgehen,
  # damit eine Groesse herauskommt; behoben ist damit nichts, und welche
  # der beiden Fassungen jeweils gewinnt, ist dem Zufall der Bauordnung
  # ueberlassen. Darum NUR hier und nie im gewoehnlichen Bau.
  # zephyr_link_libraries, NICHT target_link_options(app): das letzte
  # Binden macht Zephyr selbst (Ziel zephyr_pre0), nicht die Bibliothek
  # "app". Eine Option an "app" steht in der Binde-Befehlszeile gar nicht
  # drin - gemessen am 24.10.2026.
  zephyr_link_libraries(-Wl,--allow-multiple-definition)
endif()
