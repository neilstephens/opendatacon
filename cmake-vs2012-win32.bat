@echo off
cmake . "-Bbuild-win32" -G "Visual Studio 12 2013" "-DFULL=ON" "-DDNP3_HOME=c:\local\dnp3" "-DASIO_HOME=C:\local\asio-1.10.1" "-DTCLAP_HOME=c:\local\tclap" "-DMICROHTTPD_HOME=c:\local\microhttpd"
pause