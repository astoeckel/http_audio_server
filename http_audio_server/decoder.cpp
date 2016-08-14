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

#include <atomic>
#include <chrono>
#include <cstddef>
#include <deque>
#include <exception>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <http_audio_server/decoder.hpp>
#include <http_audio_server/process.hpp>

namespace http_audio_server {

class DecoderImpl {
private:
	std::unique_ptr<std::istream> m_input;
	Process m_process;
	std::stringstream m_msgs;
	std::thread m_msg_thread;

	static std::string ffmpeg_fmt(const AudioFormat &output_fmt)
	{
		std::string res;
		if (output_fmt.use_float) {
			switch (output_fmt.bit_depth) {
				case 32:
					res = "f32";
					break;
				case 64:
					res = "f64";
					break;
				default:
					throw std::invalid_argument(
					    "Only 32 and 64 bit are valid floating point bit "
					    "depths!");
			}
		}
		else {
			switch (output_fmt.bit_depth) {
				case 8:
					return "u8";
				case 16:
					res = "s16";
					break;
				case 24:
					res = "s24";
					break;
				case 32:
					res = "s32";
					break;
				default:
					throw std::invalid_argument(
					    "Only 8, 16, 24 and 32 bit are valid integer bit "
					    "depths!");
			}
		}
		return res + (output_fmt.little_endian ? "le" : "be");
	}

	static std::vector<std::string> ffmpeg_args(const std::string &filename,
	                                            float offs,
	                                            const AudioFormat &output_fmt)
	{
		std::vector<std::string> res;
		if (offs > 0.0) {
			res.emplace_back("-ss");
			res.emplace_back(std::to_string(offs));
		}

		res.emplace_back("-i");
		res.emplace_back(filename);

		res.emplace_back("-ac");
		res.emplace_back(std::to_string(output_fmt.n_channels));

		res.emplace_back("-ar");
		res.emplace_back(std::to_string(output_fmt.rate));

		res.emplace_back("-f");
		res.emplace_back(ffmpeg_fmt(output_fmt));
		res.emplace_back("-");

		return res;
	}

public:
	DecoderImpl(const std::string &filename, float offs,
	            const AudioFormat &output_fmt)
	    : m_process("ffmpeg", ffmpeg_args(filename, offs, output_fmt)),
	      m_msg_thread(Process::generic_pipe,
	                   std::ref(m_process.child_stderr()), std::ref(m_msgs))
	{
		m_process.close_child_stdin();
	}

	~DecoderImpl()
	{
		// Kill the process and wait for its completion
		wait();

		// Join with the read and the messages thread
		m_msg_thread.join();
	}

	std::string messages() const { return m_msgs.str(); }

	int wait()
	{
		// Kill the process by sending SIGINT
		m_process.signal(2);

		// Wait for the read() method to return zero
		std::vector<uint8_t> null;
		while (read(4096, null)) {
			null.clear();
		};

		return m_process.wait();
	}

	size_t read(size_t n_bytes, std::vector<uint8_t> &tar)
	{
		const size_t old_size = tar.size();

		size_t n_bytes_read = 0;
		auto &is = m_process.child_stdout();
		if (is.good()) {
			tar.resize(old_size + n_bytes);
			is.read((char*)&tar[old_size], n_bytes);
			n_bytes_read = is.gcount();
		}
		tar.resize(old_size + n_bytes_read);
		return n_bytes_read;
	}
};

/*
 * Class Decoder
 */

Decoder::Decoder(const std::string &filename, float offs,
                 const AudioFormat &output_fmt)
    : m_impl(std::make_unique<DecoderImpl>(filename, offs, output_fmt))
{
}

Decoder::~Decoder()
{
	// Make sure the unique_ptr<DecoderImpl> destructor can be called
}

std::string Decoder::messages() const { return m_impl->messages(); }
int Decoder::wait() { return m_impl->wait(); }
size_t Decoder::read(size_t n_bytes, std::vector<uint8_t> &tar)
{
	return m_impl->read(n_bytes, tar);
}
}

