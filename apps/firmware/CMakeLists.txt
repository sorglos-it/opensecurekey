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
