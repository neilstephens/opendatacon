#use a real cross compiler when travis support > ubuntu bionic 

SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR i386)
SET(CMAKE_SYSTEM_VERSION generic)

# not actually cross compiling, just -m32
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")




