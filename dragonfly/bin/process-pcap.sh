#!/bin/bash
#
# Script to automate the processing of pcap files with Suricata
#
DRAGONFLY_ROOT="/opt/dragonfly/"
SURI_ROOT="/opt/suricata/"
PCAP_ROOT="/opt/pcaps"
suricatasc  -c "pcap-file ${PCAP_ROOT}/bigFlows.pcap /tmp" ${DRAGONFLY_ROOT}/run/suricata-command.ipc


