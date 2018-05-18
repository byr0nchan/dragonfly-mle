#!/bin/bash

# Script to daemonize mobster
# Requires "daemon", i.e. apt-get install daemon
#
DRAGONFLY_ROOT=/opt/dragonfly
export DRAGONFLY_ROOT

DRAGONFLY_BIN=${DRAGONFLY_ROOT}/bin/dragonfly

daemon --safe --env="DRAGONFLY_ROOT=${DRAGONFLY_ROOT}"   --name=dragonfly -- $DRAGONFLY_BIN

