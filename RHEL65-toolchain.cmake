# this one is important
SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR x86_64)
#this one not so much
SET(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
SET(CMAKE_C_COMPILER
"/home/knarl/x-tools/x86_64-RHEL65-linux-gnu/bin/x86_64-RHEL65-linux-gnu-gcc")

SET(CMAKE_CXX_COMPILER
"/home/knarl/x-tools/x86_64-RHEL65-linux-gnu/bin/x86_64-RHEL65-linux-gnu-g++")

# where is the target environment
SET(CMAKE_FIND_ROOT_PATH
"/home/knarl/x-tools/x86_64-RHEL65-linux-gnu")

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY) 
