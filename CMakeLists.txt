#  HTTP Streaming Audio Server
#  Copyright (C) 2016  Andreas Stöckel
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU Affero General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU Affero General Public License for more details.
#
#  You should have received a copy of the GNU Affero General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

# CMake version and project metadata
cmake_minimum_required(VERSION 3.0)
project(http_audio_server)

# Add the local "cmake" directory to the search path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

# Search for libopus and libogg
find_package(PkgConfig)
pkg_check_modules(OPUS opus REQUIRED)

# Include external libraries
add_subdirectory(lib/libwebm)

# Enable C++14 support
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")

# Enable all warnings on MSVC and GCC/Clang/Intel
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
		string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	else()
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
	endif()
elseif(("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR
		("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR
		("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel"))
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

# Set a default build type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

# Add the top-level directory as include directory
include_directories(
	${CMAKE_SOURCE_DIR}
)

# Compile the library itself
add_library(http_audio_server_core
	http_audio_server/decoder
	http_audio_server/encoder
	http_audio_server/json
	http_audio_server/logger
	http_audio_server/metadata
	http_audio_server/process
	http_audio_server/server
	http_audio_server/string_utils
	http_audio_server/terminal
	lib/mongoose
)
target_include_directories(http_audio_server_core
	PRIVATE
		${OPUS_INCLUDE_DIRS}
		lib/libwebm/
)
target_link_libraries(http_audio_server_core
	pthread
	webm
	${OPUS_LIBRARIES}
)


# Compile the server application
add_executable(http_audio_server
	http_audio_server/main
)
target_link_libraries(http_audio_server
	http_audio_server_core
)

