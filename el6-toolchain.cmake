# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR x86_64)
SET(CMAKE_SYSTEM_VERSION el6)

# specify the cross compiler
SET(CMAKE_C_COMPILER "$ENV{EL6_CC}")
SET(CMAKE_CXX_COMPILER "$ENV{EL6_CXX}")

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH "$ENV{SYSROOT}")

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH) 
SET(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH) 
