# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
SET(CMAKE_C_COMPILER
"/home/knarl/x-tools/armv7-rpi-linux-gnueabihf-2.21_5.1/bin/armv7-rpi-linux-gnueabihf-gcc")

SET(CMAKE_CXX_COMPILER
"/home/knarl/x-tools/armv7-rpi-linux-gnueabihf-2.21_5.1/bin/armv7-rpi-linux-gnueabihf-g++")

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH
"/home/knarl/x-tools/armv7-rpi-linux-gnueabihf-2.21_5.1/armv7-rpi-linux-gnueabihf/sysroot/")

set(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} "/home/knarl/qtc_projects/sysroot/")

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY) 

