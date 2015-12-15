@echo off
mkdir Build-x64
cd Build-x64
cmake ../ -G "Visual Studio 12 2013 Win64" "-DDNP3_HOME=c:\local\dnp3" "-DASIO_HOME=C:\local\asio-1.10.1" "-DTCLAP_HOME=c:\local\tclap" "-DMICROHTTPD_HOME=c:\local\microhttpd"
pause