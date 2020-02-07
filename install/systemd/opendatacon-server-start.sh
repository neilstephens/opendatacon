#!/bin/bash
#
# Script to start up opendatacon - to be used by systemd - but should be able to be run separately.
# We copy any python files in the config file/working folder to the bin folder where opendatacon will look for them if required
#
source scl_source enable rh-python36
/bin/cp -rf /etc/opendatacon/*.py /opt/ODC/bin/
LD_LIBRARY_PATH=/usr/local/lib 
exec /opt/ODC/bin/opendatacon -p /etc/opendatacon