#!/bin/bash
#
# Script to daemonize mobster
# Requires "daemon", i.e. apt-get install daemon
#
MOBSTER_ROOT=/opt/mobster
export MOBSTER_ROOT

MOBSTER_BIN=${MOBSTER_ROOT}/bin/mobster

daemon --safe --env="MOBSTER_ROOT=${MOBSTER_ROOT}"   --name=mobster -- $MOBSTER_BIN

