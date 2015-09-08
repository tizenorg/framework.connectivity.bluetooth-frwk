#!/bin/sh

#--------------------------------------
#    bluetooth
#--------------------------------------

BLUETOOTH_DEBUG=${1}/bluetooth
PREV_PWD=${PWD}
BT_DATA_DIR=/var/lib/bluetooth

mkdir -p ${BLUETOOTH_DEBUG}

if [ -e ${BT_DATA_DIR} ]
then
	cd ${BT_DATA_DIR}
	/bin/tar -cvzf ${BLUETOOTH_DEBUG}/bt_log.tar.gz *
fi

cd ${PREV_PWD}
