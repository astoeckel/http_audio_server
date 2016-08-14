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

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <thread>

#include <http_audio_server/decoder.hpp>
#include <http_audio_server/encoder.hpp>
#include <http_audio_server/process.hpp>
#include <http_audio_server/server.hpp>

using namespace http_audio_server;

bool cancel = false;
void signal_handler(int)
{
	if (cancel) {
		exit(1);
	}
	cancel = true;
}

class Stream {
private:
	Decoder m_decoder;
	Encoder m_encoder;
	size_t m_bytes_tranferred = 0;
	std::vector<uint8_t> m_buf;
	size_t m_bitrate;
	bool done = false;

public:
	Stream(const std::string &filename, size_t m_bitrate)
	    : m_decoder(filename),
	      m_encoder(48000, 2),
	      m_bitrate(m_bitrate)
	{
	}

	void advance(double seconds, std::ostream &os)
	{
		size_t samples = seconds * 48000;
		size_t n_bytes = samples * 2 * sizeof(float);
		if (m_decoder.read(n_bytes, m_buf)) {
			m_encoder.feed((float *)(&m_buf[0]),
			               m_buf.size() / sizeof(float) / 2, m_bitrate, os);
		}
		if (m_buf.size() < n_bytes) {
			m_encoder.finalize(m_bitrate, os);
			done = true;
		}
		m_buf.clear();
	}
};

int main(int argc, char *argv[])
{
	if (argc != 2) {
		std::cerr << "./http_audio_server <FILE>" << std::endl;
		return 1;
	}

	signal(SIGINT, signal_handler);

	std::vector<std::shared_ptr<Stream>> streams;

	HTTPServer server(
	    {RequestMapEntry("GET", "^/(index.html)?$",
	                     [](const Request &, Response &res) {
		                     res.header(200, {{"Content-Type",
		                                       "text/html; charset=utf-8"}});
		                     std::ifstream is("../static/index.html");
		                     Process::generic_pipe(is, res.stream());
		                 }),
	     RequestMapEntry("GET", "^/stream/create$",
	                     [&](const Request &, Response &res) {
		                     streams.emplace_back(
		                         std::make_shared<Stream>(argv[1], 128000));
		                     res.header(200, {{"Content-Type", "text/plain"}});
		                     res.stream() << streams.size() - 1 << std::endl;
		                 }),
	     RequestMapEntry(
	         "POST", "^/stream/[0-9]+/advance$",
	         [&](const Request &req, Response &res) {
		         std::regex re("^/stream/([0-9]+)/advance$");
		         std::smatch sm;
		         std::regex_match(req.uri, sm, re);
		         size_t stream_idx = std::stoi(sm[1]);
		         if (stream_idx < streams.size()) {
			         res.header(200, {{"Content-Type", "audio/webm"}});
			         streams[stream_idx]->advance(1.0, res.stream());
		         }
		         else {
			         res.header(404, {{"Content-Type", "application/json"}});
			         res.stream() << "\"Invalid stream id " << stream_idx
			                      << "\"";
		         }
		     })});

	while (!cancel) {
		server.poll(1000);
	}

	return 0;
}
