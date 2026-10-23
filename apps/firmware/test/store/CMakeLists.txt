# Pruefsaetze des Tresors fuer den PC. Zweiter Weg neben build_and_run.ps1 -
# gedacht fuer Linux und fuer eine CI mit gcc oder clang.
#
#   cmake -S . -B build && cmake --build build && ./build/store_tests
#
# Mit dem Geraet hat das nichts zu tun: hier laufen Attrappen fuer Flash,
# Krypto, SE050 und Anzeige.

cmake_minimum_required(VERSION 3.20)
project(osk_store_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(STORE ${CMAKE_CURRENT_SOURCE_DIR}/../../src/classes/store)
set(INC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

add_executable(store_tests
	${STORE}/store_core.c
	${STORE}/store_space.c
	${STORE}/store_items.c
	${STORE}/store_log.c
	${STORE}/store_migrate.c
	${STORE}/store_chunk.c
	${STORE}/osk_backup.c
	${STORE}/osk_shamir.c
	${STORE}/osk_transfer.c
	${STORE}/osk_wordlist.c
	fake_flash.c
	fake_crypto.c
	fake_se050.c
	fake_ui.c
	test_util.c
	test_core.c
	test_power.c
	test_migrate.c
	test_backup.c
	test_shamir.c
	test_transfer.c
	test_wear.c
	test_wordlist.c
	test_main.c)

target_include_directories(store_tests PRIVATE ${STORE} ${INC}
	${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(store_tests PRIVATE -Wall -Wextra -Werror -g)
endif()

enable_testing()
add_test(NAME store COMMAND store_tests)
