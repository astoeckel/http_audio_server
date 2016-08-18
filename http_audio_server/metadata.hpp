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

#include <string>

#include <http_audio_server/json.hpp>

namespace http_audio_server {

struct Metadata {
	std::string title;
	std::string album;
	std::string artist;
	std::string date;
	std::string format;
	int track_number = -1;
	int track_total = -1;
	int disc_number = -1;
	int disc_total = -1;
	double duration = -1.0;

	json to_json() const;
};

Metadata metadata_from_file(const std::string &filename);
}
