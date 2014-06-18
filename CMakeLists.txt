cmake_minimum_required(VERSION 2.6)
project (LibRain C)

include(CheckTypeSize)

set(CMAKE_C_FLAGS "-fPIC -g -Wall -Wextra -Wunused -pipe -std=gnu99")

if (CMAKE_COMPILER_IS_GNUCC)
    EXEC_PROGRAM(gcc ARGS --version OUTPUT_VARIABLE CMAKE_C_COMPILER_VERSION)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-variadic-macros")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsequence-point")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wredundant-decls")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
    if(CMAKE_C_COMPILER_VERSION MATCHES " [4-9]\\.[4-9]\\.[0-9]+")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcomment")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmain")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wparentheses")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfloat-equal")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunsafe-loop-optimizations")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-but-set-parameter")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-but-set-variable")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wframe-larger-than=8192")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finline-limit=8192")
		if(CMAKE_C_COMPILER_VERSION MATCHES " [4-9]\\.[6-9]\\.[0-9]+")
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")
			#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Winline")
			#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wconversion")
			#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wswitch-enum")
		endif()
    endif()
endif()

set(CMAKE_C_FLAGS_DEBUG          "-O0 -fno-inline")
set(CMAKE_C_FLAGS_RELEASE        "-O3")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -fno-inline")
set(CMAKE_C_FLAGS_MINSIZEREL     "-Os -s")

###--------------------------------------------------------###

if (LD_LIBDIR)
	MESSAGE("LD_LIBDIR explicitely set to ${LD_LIBDIR}")
else()
	CHECK_TYPE_SIZE(long SIZEOF_LONG)
	if(SIZEOF_LONG EQUAL 8)
		set (LD_LIBDIR "lib64")
	else()
		set (LD_LIBDIR "lib")
	endif()
	MESSAGE("LD_LIBDIR guessed to ${LD_LIBDIR}")
endif()

if (PREFIX)
	set(CMAKE_INSTALL_PREFIX ${PREFIX})
endif (PREFIX)
message(STATUS "Installation prefix : ${CMAKE_INSTALL_PREFIX}")

###--------------------------------------------------------###

set(JERASURE_INCLUDE_DIRS "/usr/include")
set(JERASURE_LIBRARY_DIRS "/${LD_LIBDIR}")
if (JERASURE_LIBDIR)
	set(JERASURE_LIBRARY_DIRS "${JERASURE_LIBDIR}")
endif(JERASURE_LIBDIR)
if (JERASURE_INCDIR)
	set(JERASURE_INCLUDE_DIRS ${JERASURE_INCDIR})
endif (JERASURE_INCDIR)

###--------------------------------------------------------###

include_directories(BEFORE .)
include_directories(AFTER ${JERASURE_INCLUDE_DIRS})

link_directories(${JERASURE_LIBRARY_DIRS})

add_library(rain SHARED librain.c)
set_target_properties(rain PROPERTIES
		PUBLIC_HEADER "librain.h")
target_link_libraries(rain Jerasure)

install(TARGETS rain
        LIBRARY DESTINATION ${LD_LIBDIR}
		PUBLIC_HEADER DESTINATION include)

