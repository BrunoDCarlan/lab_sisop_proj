#!/bin/sh

set -u
set -e

# Add a console on tty1
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    grep -qE '^tty1::' ${TARGET_DIR}/etc/inittab || \
	sed -i '/GENERIC_SERIAL/a\
tty1::respawn:/sbin/getty -L  tty1 0 vt100 # QEMU graphical window' ${TARGET_DIR}/etc/inittab
fi
# Compila a aplicação que testa a syscall criada no tutorial 2.2
"$HOST_DIR/bin/i686-buildroot-linux-gnu-gcc" \
    -o "$TARGET_DIR/bin/syscall_test" \
    "$BASE_DIR/../custom-scripts/syscall_test.c"
# Compila a aplicação do Desafio 1
"$HOST_DIR/bin/i686-buildroot-linux-gnu-gcc" \
    -o "$TARGET_DIR/bin/sleep_processes_test" \
    "$BASE_DIR/../custom-scripts/sleep_processes_test.c"
# Compila a aplicação do Desafio 2
"$HOST_DIR/bin/i686-buildroot-linux-gnu-gcc" \
    -o "$TARGET_DIR/bin/log_message_test" \
    "$BASE_DIR/../custom-scripts/log_message_test.c"