#!/bin/bash

SURI_ROOT="/opt/suricata/"

rm /opt/suricata/var/run/suricata.pid
echo ${SURI_ROOT}/bin/suricata -c ${SURI_ROOT}/etc/suricata-mobster.yaml --unix-socket -D
${SURI_ROOT}/bin/suricata -c ${SURI_ROOT}/etc/suricata-mobster.yaml --unix-socket -D

