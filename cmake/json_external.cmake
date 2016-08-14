#  Cypress -- C++ Spiking Neural Network Simulation Framework
#  Copyright (C) 2016  Andreas Stöckel
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Download the JSON library
message(STATUS "Downloading https://raw.githubusercontent.com/nlohmann/json/49dc2dff68abb4bbf1f3ee8b0327de7d0a673d16/src/json.hpp")
file(DOWNLOAD
	https://raw.githubusercontent.com/nlohmann/json/49dc2dff68abb4bbf1f3ee8b0327de7d0a673d16/src/json.hpp
	${CMAKE_BINARY_DIR}/include/cypress/json.hpp
	EXPECTED_HASH SHA512=5d3b15d5465dfb67e317d88aa4ecbc0a121e8e23b71c28ae7e362355796f22a2d87d8c4da841adc4375850b3152a6f8af658e7fa333a22e8cf5c3d3d3f7bcc89
)
