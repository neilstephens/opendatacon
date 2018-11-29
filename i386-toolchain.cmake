#use a real cross compiler when travis support > ubuntu bionic 

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR i386)
SET(CMAKE_SYSTEM_VERSION generic)

# not actually cross compiling, just -m32
SET(CMAKE_CXX_FLAGS "-m32" CACHE STRING "...")
SET(CMAKE_C_FLAGS "-m32" CACHE STRING "...")

#this isn't detected for some compiler versions
set(CMAKE_LIBRARY_ARCHITECTURE i386-linux-gnu)

