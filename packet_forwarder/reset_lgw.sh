#!/bin/sh

# This script is intended to be used on SX1302 CoreCell platform, it performs
# the following actions:
#       - export/unpexort GPIO23 and GPIO18 used to reset the SX1302 chip and to enable the LDOs
#       - export/unexport GPIO22 used to reset the optional SX1261 radio used for LBT/Spectral Scan
#
# Usage examples:
#       ./reset_lgw.sh stop
#       ./reset_lgw.sh start

# GPIO mapping has to be adapted with HW
#

SX1302_RESET_PIN=140     # SX1302 reset
#L70_R_RESET_PIN=70      # L70-R reset

WAIT_GPIO() {
    sleep 0.1
}

init() {
    # setup GPIOs
    echo "$SX1302_RESET_PIN" > /sys/class/gpio/export; WAIT_GPIO
#    echo "$L70_R_RESET_PIN" > /sys/class/gpio/export; WAIT_GPIO

    # set GPIOs as output
    echo "out" > /sys/class/gpio/gpio$SX1302_RESET_PIN/direction; WAIT_GPIO
#    echo "out" > /sys/class/gpio/gpio$L70_R_RESET_PIN/direction; WAIT_GPIO
}

reset() {
    echo "LoRa reset through GPIO$SX1302_RESET_PIN..."
#    echo "GPS reset through GPIO$L70_R_RESET_PIN..."

    # write output for SX1302 and L70-R to reset
    echo "0" > /sys/class/gpio/gpio$SX1302_RESET_PIN/value; WAIT_GPIO
    echo "1" > /sys/class/gpio/gpio$SX1302_RESET_PIN/value; WAIT_GPIO
    echo "0" > /sys/class/gpio/gpio$SX1302_RESET_PIN/value; WAIT_GPIO

#    echo "1" > /sys/class/gpio/gpio$L70_R_RESET_PIN/value; WAIT_GPIO
#    echo "0" > /sys/class/gpio/gpio$L70_R_RESET_PIN/value; WAIT_GPIO

}

term() {
    # cleanup all GPIOs
    if [ -d /sys/class/gpio/gpio$SX1302_RESET_PIN ]
    then
        echo "$SX1302_RESET_PIN" > /sys/class/gpio/unexport; WAIT_GPIO
    fi
#    if [ -d /sys/class/gpio/gpio$L70_R_RESET_PIN ]
#    then
#        echo "$L70_R_RESET_PIN" > /sys/class/gpio/unexport; WAIT_GPIO
#    fi
}

case "$1" in
    start)
    term # just in case
    init
    reset
    ;;
    stop)
    reset
    term
    ;;
    *)
    echo "Usage: $0 {start|stop}"
    exit 1
    ;;
esac

exit 0
