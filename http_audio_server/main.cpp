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

#include <fstream>
#include <iostream>

#include <http_audio_server/decoder.hpp>
#include <http_audio_server/encoder.hpp>

using namespace http_audio_server;

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cerr << "./http_audio_server <FILE>" << std::endl;
		return 1;
	}

	const size_t bitrate = 320000;

	// Create the decoder instance
	Decoder decoder(std::make_unique<std::ifstream>(argv[1]), 0.0);
	Encoder encoder(48000, 2);

	// Read the data from the decoder until the decoder has finished
	std::ofstream os("res.opus");
	std::vector<uint8_t> buf;
	while (decoder.read(4096, buf)) {
		encoder.feed((float *)(&buf[0]), buf.size() / sizeof(float) / 2,
		               bitrate, os);
		buf.clear();
	}
	encoder.finalize(bitrate, os);

	if (decoder.wait() != 0) {
		std::cerr << decoder.messages();
		return 1;
	}

	return 0;
}
