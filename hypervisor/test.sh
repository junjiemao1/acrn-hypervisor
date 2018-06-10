#!/bin/bash

BUILD_CMD="make `pwd`/build/arch/x86/cpu.o"
CLEAN_CMD="make clean"
CONFIG=build/.config
CONFIG_H=build/include/config.h
CONFIG_MK=build/include/config.mk

function assert_platform() {
    if ! grep "CONFIG_PLATFORM=\"$1\"" $CONFIG > /dev/null; then
	echo "$TEST fails"
	exit 1
    fi
    if ! grep "define CONFIG_PLATFORM \"$1\"" $CONFIG_H > /dev/null; then
	echo "$TEST fails"
	exit 1
    fi
    if ! grep "CONFIG_PLATFORM=$1" $CONFIG_MK > /dev/null; then
	echo "$TEST fails"
	exit 1
    fi
}

function assert_platform_configonly() {
    if ! grep "CONFIG_PLATFORM=\"$1\"" $CONFIG > /dev/null; then
	echo "$TEST fails"
	exit 1
    fi
}

TEST="target=all config=n cmdline=n"
$CLEAN_CMD > /dev/null 2>&1
$BUILD_CMD > /dev/null 2>&1
assert_platform "sbl"

TEST="target=defconfig config=n cmdline=n"
$CLEAN_CMD > /dev/null 2>&1
make defconfig > /dev/null 2>&1
assert_platform_configonly "sbl"

TEST="target=oldconfig config=n cmdline=n"
$CLEAN_CMD > /dev/null 2>&1
make oldconfig > /dev/null 2>&1
assert_platform_configonly "sbl"

TEST="target=all config=y cmdline=n"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
$BUILD_CMD > /dev/null 2>&1
assert_platform "uefi"

TEST="target=defconfig config=y cmdline=n"
$CLEAN_CMD > /dev/null 2>&1
make defconfig > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
assert_platform_configonly "uefi"

TEST="target=oldconfig config=y cmdline=n"
$CLEAN_CMD > /dev/null 2>&1
make defconfig > /dev/null 2>&1
make oldconfig PLATFORM=uefi > /dev/null 2>&1
assert_platform_configonly "uefi"

TEST="target=all config=n cmdline=y"
$CLEAN_CMD > /dev/null 2>&1
$BUILD_CMD PLATFORM=uefi > /dev/null 2>&1
assert_platform "uefi"

TEST="target=defconfig config=n cmdline=y"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
assert_platform_configonly "uefi"

TEST="target=oldconfig config=n cmdline=y"
$CLEAN_CMD > /dev/null 2>&1
make oldconfig PLATFORM=uefi > /dev/null 2>&1
assert_platform_configonly "uefi"

TEST="target=all config=y cmdline=y same"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
$BUILD_CMD PLATFORM=uefi > /dev/null 2>&1
assert_platform "uefi"

TEST="target=defconfig config=y cmdline=y same"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
assert_platform_configonly "uefi"

TEST="target=oldconfig config=y cmdline=y same"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
make oldconfig PLATFORM=uefi > /dev/null 2>&1
assert_platform_configonly "uefi"

TEST="target=all config=y cmdline=y diff"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
$BUILD_CMD PLATFORM=sbl > /dev/null 2>&1
assert_platform "sbl"

TEST="target=defconfig config=y cmdline=y diff"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=sbl > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
assert_platform_configonly "uefi"

TEST="target=oldconfig config=y cmdline=y diff"
$CLEAN_CMD > /dev/null 2>&1
make defconfig PLATFORM=uefi > /dev/null 2>&1
make oldconfig PLATFORM=sbl > /dev/null 2>&1
assert_platform_configonly "sbl"

echo "All test pass"
