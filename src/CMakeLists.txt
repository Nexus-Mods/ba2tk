CMAKE_MINIMUM_REQUIRED (VERSION 2.8)

SET(PROJ_AUTHOR NMM2)
SET(PROJ_ARCH x86)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  SET(PROJ_ARCH x64)
endif()


SET(ba2tk_SRCS
    ba2exception.cpp
    ba2archive.cpp
  )

SET(bsatk_HDRS
    errorcodes.h
	ba2type.h
    ba2types.h
    ba2exception.h
    ba2archive.h
	dds.h
  )

SET(Boost_USE_STATIC_LIBS        ON)
SET(Boost_USE_MULTITHREADED      ON)
SET(Boost_USE_STATIC_RUNTIME    OFF)
FIND_PACKAGE(Boost REQUIRED)

FIND_PACKAGE(zlib REQUIRED)

INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS}
                    ${ZLIB_INCLUDE_DIRS}
                    ${ZLIB_INCLUDE_DIRS}/build) # in case of an out-of-source build

ADD_LIBRARY(ba2tk STATIC ${ba2tk_HDRS} ${ba2tk_SRCS})

IF (NOT "${OPTIMIZE_COMPILE_FLAGS}" STREQUAL "")
  SET_TARGET_PROPERTIES(ba2tk PROPERTIES COMPILE_FLAGS_RELWITHDEBINFO
                        ${OPTIMIZE_COMPILE_FLAGS})
ENDIF()
SET_TARGET_PROPERTIES(ba2tk PROPERTIES LINK_FLAGS_RELWITHDEBINFO
                      "/LARGEADDRESSAWARE ${OPTIMIZE_LINK_FLAGS}")

###############
## Installation

INSTALL(TARGETS ba2tk
        ARCHIVE DESTINATION libs)
