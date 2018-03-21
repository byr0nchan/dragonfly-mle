#!/bin/bash
# Script to automate the processing of pcap files with Suricata
#
SURI_ROOT="/opt/suricata/"
suricatasc  -c "pcap-file /home/td/Downloads/pcaps/vault1.pcap /tmp" ${SURI_ROOT}/var/run/suricata/suricata-command.socket 


