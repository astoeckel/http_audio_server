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

#ifndef HTTP_AUDIO_SERVER_DECODER_HPP
#define HTTP_AUDIO_SERVER_DECODER_HPP

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <vector>

namespace http_audio_server {

/*
 * Forward declaration.
 */
class DecoderImpl;

/**
 * Structure describing the RAW audio output format.
 */
struct AudioFormat {
	int n_channels = 2;
	int rate = 48000;
	int bit_depth = 32;
	bool use_float = true;
	bool little_endian = true;
};

/**
 * Class responsible for decoding an audio file to a RAW audio stream.
 */
class Decoder {
private:
	std::unique_ptr<DecoderImpl> m_impl;

public:
	Decoder(std::unique_ptr<std::istream> input, float offs = 0.0,
	        const AudioFormat &output_fmt = AudioFormat());

	~Decoder();

	std::string messages() const;

	int wait();

	size_t read(size_t n_bytes, std::vector<uint8_t> &tar);
};
}

#endif /* HTTP_AUDIO_SERVER_PROCESS_HPP */
