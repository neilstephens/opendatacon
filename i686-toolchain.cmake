# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR i686)
SET(CMAKE_SYSTEM_VERSION generic)

# specify the cross compiler
SET(CMAKE_C_COMPILER "i686-linux-gnu-gcc")
SET(CMAKE_CXX_COMPILER "i686-linux-gnu-g++")

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH "$ENV{SYSROOT}")

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

#this isn't detected for some compiler versions
set(CMAKE_LIBRARY_ARCHITECTURE i686-linux-gnu)

