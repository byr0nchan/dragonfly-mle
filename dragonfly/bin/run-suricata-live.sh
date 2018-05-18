#!/bin/bash

SURI_ROOT="/opt/suricata/"
INTERFACE=wlp1s0

rm /opt/suricata/var/run/suricata.pid
echo ${SURI_ROOT}/bin/suricata -c ${SURI_ROOT}/etc/suricata.yaml --af-packet=${INTERFACE} -D
${SURI_ROOT}/bin/suricata -c ${SURI_ROOT}/etc/suricata.yaml --af-packet=${INTERFACE} -D

