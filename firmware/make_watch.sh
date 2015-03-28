#! /bin/bash

[ -f /run/lock/LCK..ttyUSB0 ] && { killall minicom; echo retry; exit 1; }

[ "$2" == "clean" ] && rm -fr build

./make watch-BCM920736TAG_Q32 download BT_DEVICE_ADDRESS=random UART=/dev/ttyUSB0 DEBUG=1 # VERBOSE=1
[ "$1" == "1" ] && exit
./make watch-BCM920736TAG_Q32 download BT_DEVICE_ADDRESS=random UART=/dev/ttyUSB1 DEBUG=1
[ "$1" == "2" ] && exit
