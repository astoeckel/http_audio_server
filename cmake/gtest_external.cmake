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

include(ExternalProject)

# Compile and include gtest -- set GTEST_CAN_STREAM_RESULTS_ to 0 to work around
# the confusing warning regarding getaddrinfo() being issued when statically
# linking
ExternalProject_Add(
	googletest
	URL https://github.com/google/googletest/archive/release-1.7.0.tar.gz
	URL_HASH SHA512=c623d5720c4ed574e95158529872815ecff478c03bdcee8b79c9b042a603533f93fe55f939bcfe2cd745ce340fd626ad6d9a95981596f1a4d05053d874cd1dfc
	INSTALL_COMMAND ""
)
ExternalProject_Get_Property(googletest SOURCE_DIR BINARY_DIR)
set(GTEST_INCLUDE_DIRS ${SOURCE_DIR}/include)
set(GTEST_LIBRARIES
	${BINARY_DIR}/libgtest.a
	${BINARY_DIR}/libgtest_main.a
)

