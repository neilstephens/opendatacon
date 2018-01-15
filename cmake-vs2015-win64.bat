rem @echo off
cmake . "-Bbuild-win64" -G "Visual Studio 14 2015 Win64" "-DFULL=ON" "-DDNP3_HOME=c:\local\dnp3" "-DMODBUS_HOME=c:\local\libmodbus\windows64" "-DASIO_HOME=C:\local\asio-1.10.1" "-DTCLAP_HOME=c:\local\tclap" "-DMICROHTTPD_HOME=c:\local\microhttpd"
pause