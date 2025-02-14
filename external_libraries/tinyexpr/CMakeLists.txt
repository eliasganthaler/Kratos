SET(CMAKE_INCLUDE_CURRENT_DIR ON)

# Generate variables with the sources
FILE(GLOB_RECURSE TINYEXPR_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tinyexpr/*.c") #all .cpp

# Compile library
ADD_LIBRARY(tinyexpr STATIC ${TINYEXPR_SOURCES})
IF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    TARGET_LINK_LIBRARIES(tinyexpr m)
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Linux")

# In case of using Cotire
IF(USE_COTIRE MATCHES ON)
    cotire(tinyexpr)
ENDIF(USE_COTIRE MATCHES ON)

# Define include and library
SET(TINYEXPR_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tinyexpr/)
SET(TINYEXPR_LIBRARIES tinyexpr)

# This compiles the "DLL" interface (C API)
TARGET_COMPILE_DEFINITIONS(tinyexpr PRIVATE TINYEXPR_DLL)

# Shared libraries
TARGET_COMPILE_DEFINITIONS(tinyexpr PRIVATE TINYEXPRLIB_EXPORTS)

# Add definitions
ADD_DEFINITIONS( -DTINYEXPRLIB_EXPORTS )

# Install targets
INSTALL(TARGETS tinyexpr DESTINATION libs)

# Printing a message
MESSAGE(STATUS "tinyexpr : ${TINYEXPR_INCLUDE_DIR}" )
