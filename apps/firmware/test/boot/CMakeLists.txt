# Pruefsaetze des Firmware-Updates fuer den PC.
#
#   cmake -S . -B build && cmake --build build && ./build/boot_tests
#
# Mit dem Geraet hat das nichts zu tun: MCUboot laeuft nicht mit, es gibt
# keinen QSPI-Baustein und keinen SE050. Signaturpruefer, Hash, Flash und
# Zaehler sind Attrappen (fakes.c).

cmake_minimum_required(VERSION 3.20)
project(osk_boot_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(BOOT ${CMAKE_CURRENT_SOURCE_DIR}/../../boot)
set(INC ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

add_executable(boot_tests
	${BOOT}/boot_package.c
	${BOOT}/boot_state.c
	${BOOT}/boot_policy.c
	${BOOT}/boot_session.c
	${BOOT}/boot_recovery.c
	${BOOT}/boot_se050.c
	fakes.c
	test_state.c
	test_signature.c
	test_rollback.c
	test_board.c
	test_abort.c
	test_vault.c
	test_recovery.c
	test_main.c)

target_include_directories(boot_tests PRIVATE ${BOOT} ${INC}
	${CMAKE_CURRENT_SOURCE_DIR})

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(boot_tests PRIVATE -Wall -Wextra -Werror -g)
endif()

enable_testing()
add_test(NAME boot COMMAND boot_tests)
