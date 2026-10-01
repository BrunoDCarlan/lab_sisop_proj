#!/bin/sh

cp $BASE_DIR/../custom-scripts/S41network-config $BASE_DIR/target/etc/init.d
chmod +x $BASE_DIR/target/etc/init.d/S41network-config
make -C "$BASE_DIR/../modules/simple_driver/"
BUILDROOT_DIR=$BASE_DIR/..
COMPILER=$BUILDROOT_DIR/output/host/bin/i686-buildroot-linux-gnu-gcc
$COMPILER -o $BUILDROOT_DIR/output/target/bin/syscall_test \
    $BUILDROOT_DIR/custom-scripts/syscall_test.c