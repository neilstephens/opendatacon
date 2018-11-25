# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR armv7)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
SET(CMAKE_C_COMPILER "arm-linux-gnueabihf-gcc")

SET(CMAKE_CXX_COMPILER "arm-linux-gnueabihf-g++")

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH "$ENV{RPI_SYSROOT}")
SET(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} "/usr/arm-linux-gnueabihf/")

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH) 

