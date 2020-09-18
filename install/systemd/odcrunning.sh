#!/bin/sh
#
#
#
PIDS=$(ps ax | grep -i 'opendatacon' | grep -v grep | awk '{print $1}')

if [ -z "$PIDS" ]; then
  echo "No opendatacon server running"
else
  echo "Opendatacon PIDS "$PIDS
fi
