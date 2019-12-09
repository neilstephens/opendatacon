source scl_source enable rh-python36
/bin/cp -rf ./PyPortKafka.py odc/bin/PyPortKafka.py
cd /root/odc/bin
LD_LIBRARY_PATH=/usr/local/lib ./opendatacon -c "/root/opendatacon.conf"
cd /root
