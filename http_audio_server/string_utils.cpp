/*
 *  HTTP Streaming Audio Server
 *  Copyright (C) 2016  Andreas Stöckel
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <random>

#include <http_audio_server/string_utils.hpp>

namespace http_audio_server {

std::string random_alphanum_string(const int len)
{
	static const char alphanum[] =
	    "0123456789"
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz";

	std::default_random_engine engine(std::random_device{}());
	std::uniform_int_distribution<size_t> distr(0, sizeof(alphanum) - 2);

	std::string res(16, ' ');
	for (int i = 0; i < len; ++i) {
		res[i] = alphanum[distr(engine)];
	}
	return res;
}

}

