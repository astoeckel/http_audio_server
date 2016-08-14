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
	static constexpr ssize_t BUF_SIZE_IN = 1 << 12;              // 4K
	static constexpr ssize_t BUF_SIZE_OUT = 1 << 18;             // 256K
	static constexpr ssize_t TAR_QUEUE_SIZE = BUF_SIZE_OUT * 8;  // 2M

	std::unique_ptr<std::istream> m_input;
	Process m_process;
	std::mutex m_mtx;
	std::deque<uint8_t> m_queue;
	std::atomic<bool> m_done;
	std::thread m_read_thread;
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

	static std::vector<std::string> ffmpeg_args(float offs,
	                                            const AudioFormat &output_fmt)
	{
		std::vector<std::string> res;
		if (offs > 0.0) {
			res.emplace_back("-ss");
			res.emplace_back(std::to_string(offs));
		}

		res.emplace_back("-i");
		res.emplace_back("-");

		res.emplace_back("-ac");
		res.emplace_back(std::to_string(output_fmt.n_channels));

		res.emplace_back("-ar");
		res.emplace_back(std::to_string(output_fmt.rate));

		res.emplace_back("-f");
		res.emplace_back(ffmpeg_fmt(output_fmt));
		res.emplace_back("-");

		return res;
	}

	static void read_thread(std::istream &is, std::mutex &mtx,
	                        std::deque<uint8_t> &queue, std::atomic<bool> &done)
	{
		uint8_t buf[BUF_SIZE_OUT];
		while (is.good()) {
			is.read((char *)buf, BUF_SIZE_OUT);
			{
				std::lock_guard<std::mutex> lock(mtx);
				queue.insert(queue.end(), buf, buf + is.gcount());
			}
		}
		done = true;
	}

public:
	DecoderImpl(std::unique_ptr<std::istream> input, float offs,
	            const AudioFormat &output_fmt)
	    : m_input(std::move(input)),
	      m_process("ffmpeg", ffmpeg_args(offs, output_fmt)),
	      m_done(false),
	      m_read_thread(read_thread, std::ref(m_process.child_stdout()),
	                    std::ref(m_mtx), std::ref(m_queue), std::ref(m_done)),
	      m_msg_thread(Process::generic_pipe,
	                   std::ref(m_process.child_stderr()), std::ref(m_msgs))
	{
	}

	~DecoderImpl()
	{
		close();
		wait();

		// Join with the read and the messages thread
		m_read_thread.join();
		m_msg_thread.join();
	}

	std::string messages() const { return m_msgs.str(); }
	void close()
	{
		// Close the input stream and read all still available data
		if (m_input) {
			m_input = nullptr;
			m_process.close_child_stdin();
		}
	}

	int wait()
	{
		// Wait for the read() method to return zero
		std::vector<uint8_t> null;
		while (read(BUF_SIZE_IN, null)) {
			null.clear();
		};

		return m_process.wait();
	}

	size_t read(size_t n_bytes, std::vector<uint8_t> &tar)
	{
		uint8_t buf[BUF_SIZE_IN];
		size_t queue_size = 0;
		size_t n_bytes_read = 0;
		while (n_bytes_read < n_bytes) {
			// Move as many bytes as possible from the read queue to the target
			// memory region
			{
				std::lock_guard<std::mutex> lock(m_mtx);

				size_t count = std::min(m_queue.size(), n_bytes);
				std::copy(m_queue.begin(), m_queue.begin() + count,
				          std::back_insert_iterator<std::vector<uint8_t>>(tar));
				m_queue.erase(m_queue.begin(), m_queue.begin() + count);
				queue_size = m_queue.size();
				n_bytes_read += count;
			}

			// Break once all data has been read from the queue end the read
			// thread has finished
			if (queue_size == 0 && m_done) {
				break;
			}

			// Feed more data into the decoder if the buffer underruns
			if (m_input && queue_size < TAR_QUEUE_SIZE) {
				if (m_input->good()) {
					m_input->read((char *)buf, BUF_SIZE_IN);
					if (m_input->gcount() > 0) {
						m_process.child_stdin().write((char *)buf,
						                              m_input->gcount());
					}
				}
				if (!m_input->good() || m_input->gcount() < BUF_SIZE_IN) {
					m_input = nullptr;
					m_process.close_child_stdin();
				}
			}

			// If the number of requested has not yet been reached, wait a
			// short time until the data has been processed by the decoder
			if (n_bytes_read < n_bytes) {
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
			}
		}
		return n_bytes_read;
	}
};

/*
 * Class Decoder
 */

Decoder::Decoder(std::unique_ptr<std::istream> input, float offs,
                 const AudioFormat &output_fmt)
    : m_impl(std::make_unique<DecoderImpl>(std::move(input), offs, output_fmt))
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

