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

#include <iosfwd>
#include <memory>

namespace http_audio_server {

class EncoderImpl;

class Encoder {
private:
	std::unique_ptr<EncoderImpl> m_impl;

public:
	Encoder(size_t rate, size_t n_channels);
	~Encoder();

	void feed(float *pcm, size_t n_samples, size_t bitrate, std::ostream &os);
	void finalize(size_t bitrate, std::ostream &os);
};

}
